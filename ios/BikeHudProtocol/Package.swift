// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "BikeHudProtocol",
    platforms: [
        .iOS(.v16),
        .watchOS(.v9),
        .macOS(.v13),
    ],
    products: [
        .library(name: "BikeHudProtocol", targets: ["BikeHudProtocol"]),
    ],
    targets: [
        .target(
            name: "BikeHudProtocol",
            path: "Sources/BikeHudProtocol"
        ),
        .testTarget(
            name: "BikeHudProtocolTests",
            dependencies: ["BikeHudProtocol"],
            path: "Tests/BikeHudProtocolTests"
        ),
    ]
)
