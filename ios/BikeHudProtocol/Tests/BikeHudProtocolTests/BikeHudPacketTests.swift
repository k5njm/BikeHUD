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

    func testSpeedKmh() {
        XCTAssertEqual(BikeHudPacketV1.testVector.speedKmh, 25.2, accuracy: 0.05)
    }
}
