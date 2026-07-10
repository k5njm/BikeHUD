import Foundation

/// Wire format v1 — must match `protocol/bike_hud_protocol.h` (16 bytes, little-endian).
public struct BikeHudPacketV1: Equatable, Sendable {
    public static let version: UInt8 = 1
    public static let size = 16
    public static let deviceName = "BikeHUD"

    public static let serviceUUID = "B10E0001-C0C0-41A3-B4C6-42494B454855"
    public static let telemetryUUID = "B10E0002-C0C0-41A3-B4C6-42494B454855"

    public struct Flags: OptionSet, Sendable {
        public let rawValue: UInt8
        public init(rawValue: UInt8) { self.rawValue = rawValue }

        public static let hrValid = Flags(rawValue: 1 << 0)
        public static let cadenceValid = Flags(rawValue: 1 << 1)
        public static let gpsValid = Flags(rawValue: 1 << 2)
        public static let paused = Flags(rawValue: 1 << 3)
        public static let live = Flags(rawValue: 1 << 4)
    }

    public enum HubType: UInt8, Sendable {
        case unknown = 0
        case iPhone = 1
        case watch = 2
    }

    public static let unknownU8: UInt8 = 0xFF

    public var flags: Flags
    public var speedCmPerSec: UInt16
    public var distanceMeters: UInt16
    public var elapsedSeconds: UInt16
    public var heartRateBpm: UInt8
    public var cadenceRpm: UInt8
    public var elevationMeters: Int16
    public var batteryPercent: UInt8
    public var gpsAccuracyMeters: UInt8
    public var hubType: HubType
    public var reserved: UInt8

    public init(
        flags: Flags = [.live],
        speedCmPerSec: UInt16 = 0,
        distanceMeters: UInt16 = 0,
        elapsedSeconds: UInt16 = 0,
        heartRateBpm: UInt8 = 0,
        cadenceRpm: UInt8 = Self.unknownU8,
        elevationMeters: Int16 = 0,
        batteryPercent: UInt8 = Self.unknownU8,
        gpsAccuracyMeters: UInt8 = Self.unknownU8,
        hubType: HubType = .unknown,
        reserved: UInt8 = 0
    ) {
        self.flags = flags
        self.speedCmPerSec = speedCmPerSec
        self.distanceMeters = distanceMeters
        self.elapsedSeconds = elapsedSeconds
        self.heartRateBpm = heartRateBpm
        self.cadenceRpm = cadenceRpm
        self.elevationMeters = elevationMeters
        self.batteryPercent = batteryPercent
        self.gpsAccuracyMeters = gpsAccuracyMeters
        self.hubType = hubType
        self.reserved = reserved
    }

    /// Encode little-endian 16-byte payload for GATT write.
    public func encode() -> Data {
        var data = Data(capacity: Self.size)
        data.append(Self.version)
        data.append(flags.rawValue)
        appendUInt16(speedCmPerSec, to: &data)
        appendUInt16(distanceMeters, to: &data)
        appendUInt16(elapsedSeconds, to: &data)
        data.append(heartRateBpm)
        data.append(cadenceRpm)
        appendInt16(elevationMeters, to: &data)
        data.append(batteryPercent)
        data.append(gpsAccuracyMeters)
        data.append(hubType.rawValue)
        data.append(reserved)
        assert(data.count == Self.size)
        return data
    }

    public static func decode(_ data: Data) -> BikeHudPacketV1? {
        guard data.count == size else { return nil }
        let bytes = [UInt8](data)
        guard bytes[0] == version else { return nil }

        func u16(_ i: Int) -> UInt16 {
            UInt16(bytes[i]) | (UInt16(bytes[i + 1]) << 8)
        }
        func i16(_ i: Int) -> Int16 {
            Int16(bitPattern: u16(i))
        }

        return BikeHudPacketV1(
            flags: Flags(rawValue: bytes[1]),
            speedCmPerSec: u16(2),
            distanceMeters: u16(4),
            elapsedSeconds: u16(6),
            heartRateBpm: bytes[8],
            cadenceRpm: bytes[9],
            elevationMeters: i16(10),
            batteryPercent: bytes[12],
            gpsAccuracyMeters: bytes[13],
            hubType: HubType(rawValue: bytes[14]) ?? .unknown,
            reserved: bytes[15]
        )
    }

    /// Canonical test vector from protocol.md
    public static let testVector = BikeHudPacketV1(
        flags: [.live, .gpsValid, .hrValid],
        speedCmPerSec: 700,
        distanceMeters: 0x3068,
        elapsedSeconds: 0x0F4C,
        heartRateBpm: 0x94,
        cadenceRpm: unknownU8,
        elevationMeters: 312,
        batteryPercent: 0x49,
        gpsAccuracyMeters: 5,
        hubType: .iPhone,
        reserved: 0
    )

    public var speedKmh: Double {
        Double(speedCmPerSec) * 0.036
    }

    public var speedMph: Double {
        Double(speedCmPerSec) * 0.0223693629
    }

    public var distanceMiles: Double {
        Double(distanceMeters) / 1609.344
    }

    public var distanceKm: Double {
        Double(distanceMeters) / 1000.0
    }
}

private func appendUInt16(_ value: UInt16, to data: inout Data) {
    data.append(UInt8(value & 0xFF))
    data.append(UInt8((value >> 8) & 0xFF))
}

private func appendInt16(_ value: Int16, to data: inout Data) {
    appendUInt16(UInt16(bitPattern: value), to: &data)
}
