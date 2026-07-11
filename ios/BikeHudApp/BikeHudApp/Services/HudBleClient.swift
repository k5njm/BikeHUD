import CoreBluetooth
import Foundation
import BikeHudProtocol

/// CoreBluetooth central: scan for BikeHUD, connect, write 16-byte telemetry packets.
@MainActor
final class HudBleClient: NSObject, ObservableObject {
    enum ConnectionState: Equatable {
        case poweredOff
        case unauthorized
        case idle
        case scanning
        case connecting(name: String)
        case connected(name: String)
        case ready(name: String) // characteristic discovered
        case failed(String)
    }

    @Published private(set) var state: ConnectionState = .idle
    @Published private(set) var lastWriteOK = false
    @Published private(set) var writeCount: UInt64 = 0
    @Published private(set) var lastError: String?

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var telemetryChar: CBCharacteristic?

    private let serviceUUID = CBUUID(string: BikeHudPacketV1.serviceUUID)
    private let telemetryUUID = CBUUID(string: BikeHudPacketV1.telemetryUUID)

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func startScanning() {
        lastError = nil
        guard central.state == .poweredOn else {
            state = central.state == .unauthorized ? .unauthorized : .poweredOff
            return
        }
        // Prefer already-connected peripherals (reconnect after app relaunch).
        let connected = central.retrieveConnectedPeripherals(withServices: [serviceUUID])
        if let p = connected.first {
            peripheral = p
            p.delegate = self
            state = .connecting(name: p.name ?? BikeHudPacketV1.deviceName)
            if p.state == .connected {
                p.discoverServices([serviceUUID])
            } else {
                central.connect(p, options: nil)
            }
            return
        }
        state = .scanning
        central.scanForPeripherals(
            withServices: [serviceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
    }

    func disconnect() {
        if let p = peripheral {
            central.cancelPeripheralConnection(p)
        }
        stopScanOnly()
        peripheral = nil
        telemetryChar = nil
        state = .idle
        lastWriteOK = false
    }

    /// Write one telemetry packet (~1 Hz). Safe if not ready — no-ops.
    ///
    /// Default BLE ATT allows only ~20 B for write-without-response until MTU
    /// is raised. v2 packets are 24 B (include wall clock), so we must use
    /// write-with-response when the payload exceeds the NR limit — otherwise
    /// the clock trailer is dropped and the X4 never shows date/time.
    @discardableResult
    func write(_ packet: BikeHudPacketV1) -> Bool {
        guard let peripheral, let telemetryChar,
              case .ready = state
        else {
            lastWriteOK = false
            return false
        }
        let data = packet.encode()
        let maxNR = peripheral.maximumWriteValueLength(for: .withoutResponse)
        let canNR = telemetryChar.properties.contains(.writeWithoutResponse)
            && data.count <= maxNR
        let type: CBCharacteristicWriteType = canNR ? .withoutResponse : .withResponse
        peripheral.writeValue(data, for: telemetryChar, type: type)
        writeCount += 1
        lastWriteOK = true
        return true
    }

    private func stopScanOnly() {
        if central.isScanning {
            central.stopScan()
        }
    }
}

// MARK: - CBCentralManagerDelegate

extension HudBleClient: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                if case .poweredOff = self.state { self.state = .idle }
                if case .unauthorized = self.state { self.state = .idle }
            case .poweredOff:
                self.state = .poweredOff
                self.telemetryChar = nil
            case .unauthorized:
                self.state = .unauthorized
            default:
                break
            }
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        Task { @MainActor in
            self.stopScanOnly()
            self.peripheral = peripheral
            peripheral.delegate = self
            let name = peripheral.name
                ?? (advertisementData[CBAdvertisementDataLocalNameKey] as? String)
                ?? BikeHudPacketV1.deviceName
            self.state = .connecting(name: name)
            central.connect(peripheral, options: nil)
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didConnect peripheral: CBPeripheral
    ) {
        Task { @MainActor in
            let name = peripheral.name ?? BikeHudPacketV1.deviceName
            self.state = .connecting(name: name)
            peripheral.discoverServices([self.serviceUUID])
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            self.state = .failed(error?.localizedDescription ?? "Connect failed")
            self.peripheral = nil
            self.telemetryChar = nil
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        Task { @MainActor in
            self.telemetryChar = nil
            self.peripheral = nil
            if let error {
                self.lastError = error.localizedDescription
            }
            // Auto re-scan so desk testing is sticky.
            self.state = .idle
            self.startScanning()
        }
    }
}

// MARK: - CBPeripheralDelegate

extension HudBleClient: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        Task { @MainActor in
            if let error {
                self.state = .failed(error.localizedDescription)
                return
            }
            guard let service = peripheral.services?.first(where: {
                $0.uuid == self.serviceUUID
            }) else {
                self.state = .failed("BikeHUD service not found")
                return
            }
            peripheral.discoverCharacteristics([self.telemetryUUID], for: service)
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        Task { @MainActor in
            if let error {
                self.state = .failed(error.localizedDescription)
                return
            }
            guard let char = service.characteristics?.first(where: {
                $0.uuid == self.telemetryUUID
            }) else {
                self.state = .failed("Telemetry characteristic not found")
                return
            }
            self.telemetryChar = char
            let name = peripheral.name ?? BikeHudPacketV1.deviceName
            self.state = .ready(name: name)
        }
    }

    nonisolated func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        Task { @MainActor in
            if let error {
                self.lastWriteOK = false
                self.lastError = error.localizedDescription
            }
        }
    }
}
