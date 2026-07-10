import XCTest
@testable import BikeHudProtocol

final class BikeHudPacketTests: XCTestCase {
    func testV1EncodeSize() {
        let data = BikeHudPacketV1.testVector.encode(version: 1, includeClock: false)
        XCTAssertEqual(data.count, 16)
    }

    func testV2EncodeSizeWithClock() {
        var p = BikeHudPacketV1.testVector
        p.wallClock = .now()
        let data = p.encode()
        XCTAssertEqual(data.count, 24)
        XCTAssertEqual(data[0], 2)
        XCTAssertTrue(data[1] & (1 << 5) != 0) // clock valid flag
    }

    func testV1CanonicalHex() {
        let expected: [UInt8] = [
            0x01, 0x15, 0xBC, 0x02, 0x68, 0x30, 0x4C, 0x0F,
            0x94, 0xFF, 0x38, 0x01, 0x49, 0x05, 0x01, 0x00,
        ]
        let data = BikeHudPacketV1.testVector.encode(version: 1, includeClock: false)
        XCTAssertEqual([UInt8](data), expected)
    }

    func testV1RoundTrip() {
        let original = BikeHudPacketV1.testVector
        let decoded = BikeHudPacketV1.decode(
            original.encode(version: 1, includeClock: false)
        )
        XCTAssertEqual(decoded?.speedCmPerSec, original.speedCmPerSec)
        XCTAssertEqual(decoded?.heartRateBpm, original.heartRateBpm)
        XCTAssertNil(decoded?.wallClock)
    }

    func testV2ClockRoundTrip() {
        let clock = BikeHudPacketV1.WallClock(
            year: 2026, month: 7, day: 10, hour: 18, minute: 34, second: 0, dayOfWeek: 5
        )
        var p = BikeHudPacketV1.testVector
        p.wallClock = clock
        let decoded = BikeHudPacketV1.decode(p.encode())
        XCTAssertEqual(decoded?.wallClock, clock)
        XCTAssertTrue(decoded?.flags.contains(.clockValid) == true)
    }

    func testDecodeRejectsWrongSize() {
        XCTAssertNil(BikeHudPacketV1.decode(Data([0x01, 0x00])))
        XCTAssertNil(BikeHudPacketV1.decode(Data(repeating: 0, count: 15)))
        XCTAssertNil(BikeHudPacketV1.decode(Data(repeating: 0, count: 17)))
    }

    func testDecodeRejectsWrongVersion() {
        var bytes = [UInt8](
            BikeHudPacketV1.testVector.encode(version: 1, includeClock: false)
        )
        bytes[0] = 99
        XCTAssertNil(BikeHudPacketV1.decode(Data(bytes)))
    }

    func testSpeedMph() {
        XCTAssertEqual(BikeHudPacketV1.testVector.speedMph, 15.66, accuracy: 0.05)
    }

    func testDistanceMiles() {
        XCTAssertEqual(BikeHudPacketV1.testVector.distanceMiles, 7.70, accuracy: 0.02)
    }

    func testUUIDsAreValidHex() {
        let service = BikeHudPacketV1.serviceUUID.replacingOccurrences(of: "-", with: "")
        XCTAssertEqual(service.count, 32)
        XCTAssertTrue(service.allSatisfy { $0.isHexDigit })
    }
}
