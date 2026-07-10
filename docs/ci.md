# CI & releases

## What runs automatically

### `ci.yml` (pull requests + `main`)

| Job | Runner | Checks |
|---|---|---|
| `firmware` | `ubuntu-latest` | `pio run -e x4`, `pio run -e x4_demo` |
| `protocol-swift` | `macos-latest` | `swift build` in `ios/BikeHudProtocol` |
| `ios-simulator` | `macos-latest` | `xcodebuild` **iphonesimulator**, `CODE_SIGNING_ALLOWED=NO` |

No Apple certificates are required. Failures mean “doesn’t compile,” not “won’t install on a phone.”

### `release.yml` (tags `v*` or manual)

Builds firmware + compiles iOS for simulator, then attaches artifacts to a GitHub Release:

- `bikehud-x4-firmware.bin` — app image at flash offset `0x10000`
- `bikehud-x4-factory.bin` — combined image if produced by PlatformIO
- Build logs / checksums as available

## Signed IPA (optional, later)

Blog posts like [Andrew Hoog’s GHA guide](https://www.andrewhoog.com/posts/how-to-build-an-ios-app-with-github-actions-2023/) and marketplace “build iOS” actions assume:

1. **Paid Apple Developer Program** (or at least exportable Development/Ad Hoc profiles)
2. Repo secrets: `BUILD_CERTIFICATE_BASE64`, `P12_PASSWORD`, `BUILD_PROVISION_PROFILE_BASE64`, `KEYCHAIN_PASSWORD`, `EXPORT_OPTIONS_PLIST`
3. `xcodebuild archive` + `-exportArchive` for an `.ipa`

A free **Personal Team** profile expires ~7 days and is awkward to automate. Prefer:

- **Local install** via Xcode + Personal Team while WIP  
- Add signed CI only after you join the paid program and need TestFlight / Ad Hoc testers  

Skeleton steps (when secrets exist) match [GitHub’s cert install doc](https://docs.github.com/en/actions/deployment/deploying-xcode-applications/installing-an-apple-certificate-on-macos-runners-for-xcode-development).

## Tagging a release

```bash
git checkout main
git pull
git tag -a v0.1.0 -m "BikeHUD WIP: firmware + iOS scaffold"
git push origin v0.1.0
```

Or: Actions → **Release** → Run workflow.

## Local parity

```bash
# Firmware
cd firmware && pio run -e x4 && pio run -e x4_demo

# Protocol
cd ios/BikeHudProtocol && swift build

# iOS (with Xcode installed)
cd ios/BikeHudApp
xcodebuild -scheme BikeHudApp -project BikeHudApp.xcodeproj \
  -destination 'generic/platform=iOS Simulator' \
  -configuration Debug \
  CODE_SIGNING_ALLOWED=NO \
  build
```
