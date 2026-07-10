import XCTest
@testable import BikeHudProtocol

final class BikeHudPacketTests: XCTestCase {
    func testEncodeSize() {
        let data = BikeHudPacketV1.testVector.encode()
        XCTAssertEqual(data.count, 16)
    }

    func testRoundTrip() {
        let original = BikeHudPacketV1.testVector
        let decoded = BikeHudPacketV1.decode(original.encode())
        XCTAssertEqual(decoded, original)
    }

    func testCanonicalHex() {
        // 01 15 BC 02 68 30 4C 0F 94 FF 38 01 49 05 01 00
        let expected: [UInt8] = [
            0x01, 0x15, 0xBC, 0x02, 0x68, 0x30, 0x4C, 0x0F,
            0x94, 0xFF, 0x38, 0x01, 0x49, 0x05, 0x01, 0x00,
        ]
        XCTAssertEqual([UInt8](BikeHudPacketV1.testVector.encode()), expected)
    }

    func testDecodeRejectsWrongSize() {
        XCTAssertNil(BikeHudPacketV1.decode(Data([0x01, 0x00])))
        XCTAssertNil(BikeHudPacketV1.decode(Data(repeating: 0, count: 15)))
        XCTAssertNil(BikeHudPacketV1.decode(Data(repeating: 0, count: 17)))
    }

    func testDecodeRejectsWrongVersion() {
        var bytes = [UInt8](BikeHudPacketV1.testVector.encode())
        bytes[0] = 2
        XCTAssertNil(BikeHudPacketV1.decode(Data(bytes)))
    }

    func testSpeedKmh() {
        XCTAssertEqual(BikeHudPacketV1.testVector.speedKmh, 25.2, accuracy: 0.05)
    }

    func testSpeedMph() {
        // 700 cm/s ≈ 15.66 mph
        XCTAssertEqual(BikeHudPacketV1.testVector.speedMph, 15.66, accuracy: 0.05)
    }

    func testDistanceMiles() {
        // 0x3068 m = 12392 m ≈ 7.70 mi
        XCTAssertEqual(BikeHudPacketV1.testVector.distanceMiles, 7.70, accuracy: 0.02)
    }

    func testUUIDsAreValidHex() {
        let service = BikeHudPacketV1.serviceUUID.replacingOccurrences(of: "-", with: "")
        let telem = BikeHudPacketV1.telemetryUUID.replacingOccurrences(of: "-", with: "")
        XCTAssertEqual(service.count, 32)
        XCTAssertEqual(telem.count, 32)
        XCTAssertTrue(service.allSatisfy { $0.isHexDigit })
        XCTAssertTrue(telem.allSatisfy { $0.isHexDigit })
        XCTAssertEqual(BikeHudPacketV1.deviceName, "BikeHUD")
    }

    func testFlagsRoundTrip() {
        var p = BikeHudPacketV1(
            flags: [.live, .hrValid, .cadenceValid, .paused],
            speedCmPerSec: 100,
            hubType: .watch
        )
        p.heartRateBpm = 140
        p.cadenceRpm = 90
        let again = BikeHudPacketV1.decode(p.encode())
        XCTAssertEqual(again, p)
        XCTAssertEqual(again?.hubType, .watch)
        XCTAssertTrue(again?.flags.contains(.paused) == true)
    }
}
