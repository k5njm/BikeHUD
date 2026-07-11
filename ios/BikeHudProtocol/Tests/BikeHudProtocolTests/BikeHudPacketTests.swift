import XCTest
@testable import BikeHudProtocol

final class BikeHudPacketTests: XCTestCase {
    func testEncodeSize() {
        XCTAssertEqual(BikeHudPacketV1.testVector.encode().count, 16)
        XCTAssertEqual(BikeHudTimeSync.now().encode().count, 16)
    }

    func testRoundTrip() {
        let original = BikeHudPacketV1.testVector
        XCTAssertEqual(BikeHudPacketV1.decode(original.encode()), original)
    }

    func testCanonicalHex() {
        let expected: [UInt8] = [
            0x01, 0x15, 0xBC, 0x02, 0x68, 0x30, 0x4C, 0x0F,
            0x94, 0xFF, 0x38, 0x01, 0x49, 0x05, 0x01, 0x00,
        ]
        XCTAssertEqual([UInt8](BikeHudPacketV1.testVector.encode()), expected)
    }

    func testTimeSyncTypeAndLayout() {
        let t = BikeHudTimeSync(
            year: 2026, month: 7, day: 11, hour: 18, minute: 34, second: 5, dayOfWeek: 6
        )
        let bytes = [UInt8](t.encode())
        XCTAssertEqual(bytes[0], 0x10)
        XCTAssertEqual(bytes[1], 0)
        XCTAssertEqual(UInt16(bytes[2]) | (UInt16(bytes[3]) << 8), 2026)
        XCTAssertEqual(bytes[4], 7)
        XCTAssertEqual(bytes[5], 11)
        XCTAssertEqual(bytes[6], 18)
        XCTAssertEqual(bytes[7], 34)
        XCTAssertEqual(bytes[8], 5)
        XCTAssertEqual(bytes[9], 6)
        XCTAssertEqual(bytes.count, 16)
    }

    func testDecodeRejectsTimeSyncAsTelemetry() {
        let data = BikeHudTimeSync.now().encode()
        XCTAssertNil(BikeHudPacketV1.decode(data))
    }

    func testSpeedMph() {
        XCTAssertEqual(BikeHudPacketV1.testVector.speedMph, 15.66, accuracy: 0.05)
    }
}
