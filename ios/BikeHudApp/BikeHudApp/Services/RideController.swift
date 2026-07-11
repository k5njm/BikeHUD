import Foundation
import UIKit
import BikeHudProtocol

/// Owns BLE + sensors and pushes a v1 packet ~1 Hz while a session is live.
@MainActor
final class RideController: ObservableObject {
    enum Mode: String, CaseIterable, Identifiable {
        case demo = "Demo ride"
        case gps = "GPS ride"
        var id: String { rawValue }
    }

    enum SessionState: Equatable {
        case idle
        case running
        case paused
    }

    let ble = HudBleClient()
    let location = LocationTelemetry()

    @Published var mode: Mode = .demo
    @Published private(set) var session: SessionState = .idle
    @Published private(set) var lastPacket: BikeHudPacketV1?
    @Published private(set) var elapsedSeconds: UInt16 = 0
    @Published private(set) var demoNote: String = ""

    private var tickTimer: Timer?
    private var clockTimer: Timer?
    private var sessionStartedAt: Date?
    private var pausedAccumulated: TimeInterval = 0
    private var pauseBeganAt: Date?
    private var lastTimeSyncAt: Date?

    // Demo synthetic ride state
    private var demoElapsed: UInt16 = 0
    private var demoDistanceM: UInt16 = 0

    /// Re-sync wall clock at least this often while connected (HUD free-runs between).
    private let timeSyncInterval: TimeInterval = 5 * 60

    var isHudReady: Bool {
        if case .ready = ble.state { return true }
        return false
    }

    init() {
        // Poll so we send TIME_SYNC soon after the HUD becomes Ready.
        clockTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.maybeSyncTime(force: false) }
        }
    }

    func connectHud() {
        ble.startScanning()
        // Next Ready transition will be picked up by the clock timer.
    }

    func disconnectHud() {
        stopSession()
        ble.disconnect()
    }

    func startSession() {
        guard session == .idle || session == .paused else { return }

        if session == .idle {
            sessionStartedAt = Date()
            pausedAccumulated = 0
            pauseBeganAt = nil
            elapsedSeconds = 0
            demoElapsed = 0
            demoDistanceM = 0
            if mode == .gps {
                location.requestPermission()
                location.start()
            }
        } else if session == .paused, let began = pauseBeganAt {
            pausedAccumulated += Date().timeIntervalSince(began)
            pauseBeganAt = nil
            if mode == .gps {
                location.start()
            }
        }

        session = .running
        startTicker()
    }

    func pauseSession() {
        guard session == .running else { return }
        session = .paused
        pauseBeganAt = Date()
        stopTicker()
        if mode == .gps {
            location.stop()
        }
        // Send one paused packet so HUD shows PAUSED chrome.
        writeCurrentPacket(paused: true)
    }

    func stopSession() {
        session = .idle
        stopTicker()
        location.stop()
        sessionStartedAt = nil
        pauseBeganAt = nil
        pausedAccumulated = 0
    }

    // MARK: - Private

    private func startTicker() {
        stopTicker()
        // Fire immediately then every 1s.
        tick()
        tickTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.tick()
            }
        }
    }

    private func stopTicker() {
        tickTimer?.invalidate()
        tickTimer = nil
    }

    private func tick() {
        guard session == .running else { return }
        updateElapsed()
        maybeSyncTime(force: false)
        writeCurrentPacket(paused: false)
    }

    /// Push a 16-byte TIME_SYNC so the HUD can free-run wall clock.
    /// Call on connect and about every `timeSyncInterval` while linked.
    private func maybeSyncTime(force: Bool) {
        guard isHudReady else { return }
        if !force, let last = lastTimeSyncAt,
           Date().timeIntervalSince(last) < timeSyncInterval
        {
            return
        }
        if ble.writeTimeSync(.now()) {
            lastTimeSyncAt = Date()
        }
    }

    private func updateElapsed() {
        guard let start = sessionStartedAt else { return }
        var pauseExtra = pausedAccumulated
        if let began = pauseBeganAt {
            pauseExtra += Date().timeIntervalSince(began)
        }
        let total = Date().timeIntervalSince(start) - pauseExtra
        elapsedSeconds = UInt16(min(max(total, 0), Double(UInt16.max)))
    }

    private func writeCurrentPacket(paused: Bool) {
        let packet: BikeHudPacketV1
        switch mode {
        case .demo:
            packet = makeDemoPacket(paused: paused)
        case .gps:
            packet = makeGpsPacket(paused: paused)
        }
        lastPacket = packet
        _ = ble.write(packet)
    }

    private func makeDemoPacket(paused: Bool) -> BikeHudPacketV1 {
        // Walk a synthetic road ride so the e-ink updates without leaving the desk.
        if session == .running {
            demoElapsed = elapsedSeconds
            // ~15.5 mph ≈ 693 cm/s
            let speed: UInt16 = 693
            if demoElapsed > 0 {
                // integrate roughly 6.93 m/s
                demoDistanceM = UInt16(min(UInt32(demoElapsed) * 7, UInt32(UInt16.max)))
            }
            demoNote = "Synthetic ~15.5 mph"
        }

        var flags: BikeHudPacketV1.Flags = [.live, .gpsValid, .hrValid, .cadenceValid]
        if paused { flags.insert(.paused) }

        // Vary HR and cadence so X4 sparklines show movement in demo mode.
        let hr = UInt8(120 + Int(demoElapsed % 40))
        let cad: UInt8 = paused
            ? BikeHudPacketV1.unknownU8
            : UInt8(78 + Int(demoElapsed % 20))

        return BikeHudPacketV1(
            flags: flags,
            speedCmPerSec: paused ? 0 : 693,
            distanceMeters: demoDistanceM,
            elapsedSeconds: demoElapsed,
            heartRateBpm: hr,
            cadenceRpm: cad,
            elevationMeters: 200 + Int16(demoElapsed % 80),
            batteryPercent: hubBatteryPercent(),
            gpsAccuracyMeters: 5,
            hubType: .iPhone
        )
    }

    private func makeGpsPacket(paused: Bool) -> BikeHudPacketV1 {
        var flags: BikeHudPacketV1.Flags = [.live]
        if paused { flags.insert(.paused) }
        if location.hasFix {
            flags.insert(.gpsValid)
        }

        let speed = paused ? UInt16(0) : location.speedCmPerSec
        return BikeHudPacketV1(
            flags: flags,
            speedCmPerSec: speed,
            distanceMeters: location.distanceMetersU16,
            elapsedSeconds: elapsedSeconds,
            heartRateBpm: 0,
            cadenceRpm: BikeHudPacketV1.unknownU8,
            elevationMeters: location.elevationMetersI16,
            batteryPercent: hubBatteryPercent(),
            gpsAccuracyMeters: location.gpsAccuracyU8,
            hubType: .iPhone
        )
    }

    private func hubBatteryPercent() -> UInt8 {
        UIDevice.current.isBatteryMonitoringEnabled = true
        let level = UIDevice.current.batteryLevel
        if level < 0 { return BikeHudPacketV1.unknownU8 }
        return UInt8(min(max(Int(level * 100), 0), 100))
    }
}
