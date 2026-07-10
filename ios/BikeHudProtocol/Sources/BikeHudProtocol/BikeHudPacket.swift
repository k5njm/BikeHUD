import Foundation

/// Wire format — must match `protocol/bike_hud_protocol.h`.
/// v1 = 16 bytes core metrics; v2 = 24 bytes with hub-local wall clock.
public struct BikeHudPacketV1: Equatable, Sendable {
    public static let versionV1: UInt8 = 1
    public static let versionV2: UInt8 = 2
    /// Preferred on-wire version for new hubs.
    public static let version: UInt8 = versionV2
    public static let sizeV1 = 16
    public static let sizeV2 = 24
    public static let size = sizeV2
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
        public static let clockValid = Flags(rawValue: 1 << 5)
    }

    public enum HubType: UInt8, Sendable {
        case unknown = 0
        case iPhone = 1
        case watch = 2
    }

    /// Local wall clock already in the rider's timezone (from the hub).
    public struct WallClock: Equatable, Sendable {
        public var year: UInt16
        public var month: UInt8
        public var day: UInt8
        public var hour: UInt8
        public var minute: UInt8
        public var second: UInt8
        /// 0 = Sunday … 6 = Saturday
        public var dayOfWeek: UInt8

        public init(
            year: UInt16,
            month: UInt8,
            day: UInt8,
            hour: UInt8,
            minute: UInt8,
            second: UInt8,
            dayOfWeek: UInt8
        ) {
            self.year = year
            self.month = month
            self.day = day
            self.hour = hour
            self.minute = minute
            self.second = second
            self.dayOfWeek = dayOfWeek
        }

        public static func now(calendar: Calendar = .current, date: Date = Date()) -> WallClock {
            let c = calendar
            let comps = c.dateComponents(
                [.year, .month, .day, .hour, .minute, .second, .weekday],
                from: date
            )
            // Foundation weekday: 1 = Sunday … 7 = Saturday
            let dow = UInt8(max((comps.weekday ?? 1) - 1, 0))
            return WallClock(
                year: UInt16(comps.year ?? 2026),
                month: UInt8(comps.month ?? 1),
                day: UInt8(comps.day ?? 1),
                hour: UInt8(comps.hour ?? 0),
                minute: UInt8(comps.minute ?? 0),
                second: UInt8(comps.second ?? 0),
                dayOfWeek: dow
            )
        }
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
    public var wallClock: WallClock?

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
        reserved: UInt8 = 0,
        wallClock: WallClock? = nil
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
        self.wallClock = wallClock
    }

    /// Encode v2 (24 bytes) when wall clock present or always as v2 with clock flag clear.
    public func encode() -> Data {
        encode(version: Self.versionV2, includeClock: wallClock != nil)
    }

    public func encode(version: UInt8, includeClock: Bool) -> Data {
        var flagsOut = flags
        var data = Data(capacity: includeClock ? Self.sizeV2 : Self.sizeV1)
        let ver: UInt8
        if includeClock, wallClock != nil {
            ver = Self.versionV2
            flagsOut.insert(.clockValid)
        } else if version == Self.versionV2 {
            ver = Self.versionV2
            flagsOut.remove(.clockValid)
        } else {
            ver = Self.versionV1
            flagsOut.remove(.clockValid)
        }

        data.append(ver)
        data.append(flagsOut.rawValue)
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

        if ver == Self.versionV2 {
            if let clock = wallClock, includeClock {
                appendUInt16(clock.year, to: &data)
                data.append(clock.month)
                data.append(clock.day)
                data.append(clock.hour)
                data.append(clock.minute)
                data.append(clock.second)
                data.append(clock.dayOfWeek)
            } else {
                // v2 without clock: zero trailer
                data.append(contentsOf: [UInt8](repeating: 0, count: 8))
            }
            assert(data.count == Self.sizeV2)
        } else {
            assert(data.count == Self.sizeV1)
        }
        return data
    }

    public static func decode(_ data: Data) -> BikeHudPacketV1? {
        guard data.count == sizeV1 || data.count == sizeV2 else { return nil }
        let bytes = [UInt8](data)
        let ver = bytes[0]
        guard ver == versionV1 || ver == versionV2 else { return nil }

        func u16(_ i: Int) -> UInt16 {
            UInt16(bytes[i]) | (UInt16(bytes[i + 1]) << 8)
        }
        func i16(_ i: Int) -> Int16 {
            Int16(bitPattern: u16(i))
        }

        var flags = Flags(rawValue: bytes[1])
        var clock: WallClock?
        if data.count == sizeV2, ver == versionV2, flags.contains(.clockValid) {
            clock = WallClock(
                year: u16(16),
                month: bytes[18],
                day: bytes[19],
                hour: bytes[20],
                minute: bytes[21],
                second: bytes[22],
                dayOfWeek: bytes[23]
            )
        } else {
            flags.remove(.clockValid)
        }

        return BikeHudPacketV1(
            flags: flags,
            speedCmPerSec: u16(2),
            distanceMeters: u16(4),
            elapsedSeconds: u16(6),
            heartRateBpm: bytes[8],
            cadenceRpm: bytes[9],
            elevationMeters: i16(10),
            batteryPercent: bytes[12],
            gpsAccuracyMeters: bytes[13],
            hubType: HubType(rawValue: bytes[14]) ?? .unknown,
            reserved: bytes[15],
            wallClock: clock
        )
    }

    /// Canonical v1 test vector from protocol.md
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
        reserved: 0,
        wallClock: nil
    )

    public var speedKmh: Double { Double(speedCmPerSec) * 0.036 }
    public var speedMph: Double { Double(speedCmPerSec) * 0.0223693629 }
    public var distanceMiles: Double { Double(distanceMeters) / 1609.344 }
    public var distanceKm: Double { Double(distanceMeters) / 1000.0 }
}

private func appendUInt16(_ value: UInt16, to data: inout Data) {
    data.append(UInt8(value & 0xFF))
    data.append(UInt8((value >> 8) & 0xFF))
}

private func appendInt16(_ value: Int16, to data: inout Data) {
    appendUInt16(UInt16(bitPattern: value), to: &data)
}
