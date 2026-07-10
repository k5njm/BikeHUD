# Contributing to BikeHUD

This project is **WIP**. Issues and pull requests are welcome — keep changes focused and CI green.

## Workflow

1. **Open an issue** for bugs, ideas, or design questions (use a template when it fits).
2. **Fork / branch** from `main`: `git checkout -b feat/short-name` or `fix/…`.
3. **Make a small PR** against `main`. One concern per PR when practical.
4. **Wait for CI** (firmware builds, protocol C tests, Swift tests, iOS simulator compile).
5. Prefer discussion on the issue/PR over long private threads.

We do **not** require the $99 Apple Developer Program for contributions. Firmware and protocol work is enough; iOS can be Simulator-only in CI.

## Local checks (match CI)

```bash
# Protocol C host tests
cc -std=c11 -I protocol -o /tmp/test_protocol protocol/tests/test_protocol.c
/tmp/test_protocol

# Firmware
cd firmware && pio run -e x4 && pio run -e x4_demo

# Swift package
cd ios/BikeHudProtocol && swift build && swift test   # needs Xcode for XCTest

# iOS app (optional; needs Xcode)
cd ios/BikeHudApp
xcodebuild -scheme BikeHudApp -project BikeHudApp.xcodeproj \
  -destination 'generic/platform=iOS Simulator' \
  CODE_SIGNING_ALLOWED=NO build
```

## Design guardrails

- Keep the **16-byte v1 wire format** stable; bump `version` only for breaking changes.
- X4 stays a **BLE peripheral** — sensors live on the Apple hub.
- Don’t commit flash dumps (`backups/`, `*.bin`), certs, or signing profiles.
- Prefer imperial **display** conversion on the HUD; metric on the wire.

Docs index: [`docs/README.md`](docs/README.md).

## License

Contributions are under the [MIT License](LICENSE).
