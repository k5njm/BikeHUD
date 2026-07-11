import SwiftUI
import UIKit
import BikeHudProtocol

struct ContentView: View {
    @EnvironmentObject private var ride: RideController

    var body: some View {
        NavigationStack {
            List {
                Section("X4 HUD") {
                    LabeledContent("BLE") {
                        Text(bleStatusText)
                            .foregroundStyle(bleStatusColor)
                            .multilineTextAlignment(.trailing)
                    }
                    LabeledContent("Writes") {
                        Text("\(ride.ble.writeCount)")
                    }
                    if let err = ride.ble.lastError {
                        Text(err)
                            .font(.caption)
                            .foregroundStyle(.red)
                    }
                    HStack(spacing: 12) {
                        Button {
                            UIImpactFeedbackGenerator(style: .medium).impactOccurred()
                            ride.connectHud()
                        } label: {
                            HStack(spacing: 8) {
                                if isBleBusy {
                                    ProgressView()
                                        .controlSize(.small)
                                        .tint(.white)
                                }
                                Text(connectButtonTitle)
                            }
                            .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.borderedProminent)
                        // Keep enabled so the press highlights; ignore taps while busy.
                        .opacity(isBleBusy ? 0.85 : 1.0)
                        .allowsHitTesting(!isBleBusy)

                        if ride.isHudReady || isConnectedish {
                            Button("Disconnect", role: .destructive) {
                                UIImpactFeedbackGenerator(style: .light).impactOccurred()
                                ride.disconnectHud()
                            }
                            .buttonStyle(.bordered)
                        }
                    }
                }

                Section("Ride") {
                    Picker("Mode", selection: $ride.mode) {
                        ForEach(RideController.Mode.allCases) { mode in
                            Text(mode.rawValue).tag(mode)
                        }
                    }
                    .disabled(ride.session != .idle)

                    LabeledContent("Session") {
                        Text(sessionText)
                    }

                    if ride.mode == .demo, !ride.demoNote.isEmpty, ride.session != .idle {
                        Text(ride.demoNote)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }

                    if ride.mode == .gps {
                        LabeledContent("GPS") {
                            Text(gpsText)
                                .multilineTextAlignment(.trailing)
                        }
                    }

                    rideButtons
                }

                if let packet = ride.lastPacket {
                    Section("Last packet → X4") {
                        packetRows(packet)
                    }
                }

                Section("Desk test") {
                    Text(
                        "1. Power the X4 (BikeHUD firmware).\n"
                            + "2. Tap Connect BikeHUD.\n"
                            + "3. Start Demo ride — digits should move ~1 Hz.\n"
                            + "4. GPS ride needs outdoor location permission."
                    )
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Bike HUD")
        }
        .onAppear {
            // Kick BLE early so permission prompt appears.
            ride.connectHud()
        }
    }

    // MARK: - Pieces

    private var rideButtons: some View {
        HStack(spacing: 12) {
            switch ride.session {
            case .idle:
                Button("Start") { ride.startSession() }
                    .buttonStyle(.borderedProminent)
            case .running:
                Button("Pause") { ride.pauseSession() }
                    .buttonStyle(.bordered)
                Button("Stop", role: .destructive) { ride.stopSession() }
                    .buttonStyle(.bordered)
            case .paused:
                Button("Resume") { ride.startSession() }
                    .buttonStyle(.borderedProminent)
                Button("Stop", role: .destructive) { ride.stopSession() }
                    .buttonStyle(.bordered)
            }
        }
    }

    @ViewBuilder
    private func packetRows(_ p: BikeHudPacketV1) -> some View {
        LabeledContent("Speed") {
            Text(String(format: "%.1f mph", p.speedMph))
        }
        LabeledContent("Distance") {
            Text(String(format: "%.2f mi", p.distanceMiles))
        }
        LabeledContent("Time") {
            Text(formatElapsed(p.elapsedSeconds))
        }
        LabeledContent("HR") {
            Text(p.flags.contains(.hrValid) ? "\(p.heartRateBpm) bpm" : "—")
        }
        LabeledContent("Cadence") {
            Text(
                p.flags.contains(.cadenceValid) && p.cadenceRpm != BikeHudPacketV1.unknownU8
                    ? "\(p.cadenceRpm) rpm" : "—"
            )
        }
        LabeledContent("Elev") {
            Text(String(format: "%.0f ft", Double(p.elevationMeters) * 3.28084))
        }
        LabeledContent("Flags") {
            Text(flagsSummary(p.flags))
                .font(.caption.monospaced())
        }
    }

    private var bleStatusText: String {
        switch ride.ble.state {
        case .poweredOff: return "Bluetooth off"
        case .unauthorized: return "Bluetooth permission denied"
        case .idle: return "Idle"
        case .scanning: return "Scanning…"
        case .connecting(let n): return "Connecting \(n)…"
        case .connected(let n): return "Connected \(n)"
        case .ready(let n): return "Ready · \(n)"
        case .failed(let m): return "Failed: \(m)"
        }
    }

    private var bleStatusColor: Color {
        switch ride.ble.state {
        case .ready: return .green
        case .failed, .poweredOff, .unauthorized: return .red
        case .scanning, .connecting: return .orange
        default: return .primary
        }
    }

    private var isBleBusy: Bool {
        switch ride.ble.state {
        case .scanning, .connecting: return true
        default: return false
        }
    }

    private var connectButtonTitle: String {
        switch ride.ble.state {
        case .scanning:
            return "Scanning…"
        case .connecting:
            return "Connecting…"
        case .ready:
            return "Reconnect"
        case .failed:
            return "Retry Connect"
        default:
            return "Connect BikeHUD"
        }
    }

    private var isConnectedish: Bool {
        switch ride.ble.state {
        case .connected, .ready, .connecting: return true
        default: return false
        }
    }

    private var sessionText: String {
        switch ride.session {
        case .idle: return "Idle"
        case .running: return "Running · \(formatElapsed(ride.elapsedSeconds))"
        case .paused: return "Paused · \(formatElapsed(ride.elapsedSeconds))"
        }
    }

    private var gpsText: String {
        if let s = ride.location.speedMps, s >= 0 {
            let mph = s * 2.23694
            return String(format: "%.1f mph · ±%.0fm", mph, ride.location.horizontalAccuracyMeters ?? -1)
        }
        return "No fix"
    }

    private func formatElapsed(_ seconds: UInt16) -> String {
        let s = Int(seconds)
        let h = s / 3600
        let m = (s % 3600) / 60
        let sec = s % 60
        if h > 0 {
            return String(format: "%d:%02d:%02d", h, m, sec)
        }
        return String(format: "%d:%02d", m, sec)
    }

    private func flagsSummary(_ f: BikeHudPacketV1.Flags) -> String {
        var parts: [String] = []
        if f.contains(.live) { parts.append("LIVE") }
        if f.contains(.gpsValid) { parts.append("GPS") }
        if f.contains(.hrValid) { parts.append("HR") }
        if f.contains(.cadenceValid) { parts.append("CAD") }
        if f.contains(.paused) { parts.append("PAUSE") }
        return parts.isEmpty ? "—" : parts.joined(separator: " ")
    }
}

#Preview {
    ContentView()
        .environmentObject(RideController())
}
