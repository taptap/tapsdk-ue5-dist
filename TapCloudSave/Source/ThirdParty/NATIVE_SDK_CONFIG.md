# TapCloudSave Native SDK Configuration

Native binaries required at runtime per desktop platform.

| Platform | Path                                                         | Status                                                                                         |
|----------|--------------------------------------------------------------|------------------------------------------------------------------------------------------------|
| Win64    | `Source/ThirdParty/Desktop/Win64/cloudsave_sdk.dll`          | Shipped (copied from `Unity/Assets/TapSDK/CloudSave/Standalone/Plugins/x86_64/`).              |
| Mac      | `Source/ThirdParty/Desktop/Mac/libcloudsave_sdk.dylib`       | Shipped with the UE5 plugin. This is the same universal dylib as Unity `Standalone/Plugins/macOS/libcloudsave_sdk.dylib` (arm64 + x86_64). |
| Linux    | `Source/ThirdParty/Desktop/Linux/libcloudsave_sdk.so`        | NOT shipped. Source `.so` separately when a Linux build is required.                            |

`Build.cs` fails the Mac target when the dylib is absent, so a package that always reports
`InitFail (300002)` cannot be produced. Fetch Git LFS assets before building. Win64 and
Linux retain their existing missing-library behavior.

## Shared header

`Source/ThirdParty/Desktop/Include/cloudsave_sdk.h` mirrors the Unity wrapper
(`Unity/Assets/TapSDK/CloudSave/Standalone/Runtime/Internal/TapCloudSaveWrapper.cs`)
verbatim, including the `extraParams` argument on the business APIs which the
UE5 layer always passes as `""`.

## Drop-in steps for Mac / Linux

1. Place the dylib/so under the corresponding directory above.
2. Re-run UE5 build for the affected target. No code changes required —
   `Build.cs` and the wrapper detect availability lazily via
   `FPlatformProcess::GetDllHandle`.

## iOS

- Framework: `Source/ThirdParty/iOS/Frameworks/TapTapCloudSaveSDK.xcframework`
- Built from: `iOS/TapTapSDK/TapTapCloudSave/` (scheme `TapTapCloudSaveSDK` in `iOS/TapTapSDK/TapTapSDK.xcworkspace`)
- Slices: `ios-arm64` (device, arm64), `ios-arm64_x86_64-simulator` (arm64 + x86_64)
- Mach-O type: static (`MACH_O_TYPE = staticlib`) — linked statically into the host app.
- Tracked in Git LFS (see `.gitattributes` entries near other UE5 xcframework binaries).

### Build command (re-run when iOS source changes)

```bash
cd iOS/TapTapSDK
pod install   # if Pods/Manifest.lock is stale

# 1) iOS Simulator slice (arm64 + x86_64)
xcodebuild \
  -workspace TapTapSDK.xcworkspace \
  -scheme TapTapCloudSaveSDK \
  -configuration Release \
  -sdk iphonesimulator \
  -derivedDataPath build/DerivedData \
  SKIP_INSTALL=NO BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
  CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
  ARCHS="arm64 x86_64" ONLY_ACTIVE_ARCH=NO build

# 2) iOS Device slice (arm64)
xcodebuild \
  -workspace TapTapSDK.xcworkspace \
  -scheme TapTapCloudSaveSDK \
  -configuration Release \
  -sdk iphoneos \
  -derivedDataPath build/DerivedData \
  SKIP_INSTALL=NO BUILD_LIBRARY_FOR_DISTRIBUTION=YES \
  CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
  ARCHS="arm64" ONLY_ACTIVE_ARCH=NO SUPPORTS_MACCATALYST=NO build

# 3) Wrap into xcframework
DEV=build/DerivedData/Build/Products/Release-iphoneos/TapTapCloudSaveSDK.framework
SIM=build/DerivedData/Build/Products/Release-iphonesimulator/TapTapCloudSaveSDK.framework
OUT=../../UE5/Plugins/TapCloudSave/Source/ThirdParty/iOS/Frameworks/TapTapCloudSaveSDK.xcframework
rm -rf "$OUT"
xcodebuild -create-xcframework -framework "$DEV" -framework "$SIM" -output "$OUT"

# 4) Cleanup intermediates
rm -rf build/
```

Notes:
- `SUPPORTS_MACCATALYST=NO` on the device build is required on Xcode 26 when the
  iOS platform component fails to register (scheme falls back to Mac Catalyst as
  the only "available" destination). Passing `-sdk iphoneos` alone still triggers
  Mac Catalyst build of dependent xcframeworks; this flag fully constrains the
  build to plain iOS.
- `TapTapCloudSaveSDK.xcframework` is a **static library archive** (`MACH_O_TYPE = staticlib`)
  containing only its own Swift/Obj-C wrapper `.o` files. The underlying C++ core
  (`cloudsave_sdk`) is NOT bundled into it. Therefore the UE5 plugin must vendor
  both side-by-side:
  - `Source/ThirdParty/iOS/Frameworks/TapTapCloudSaveSDK.xcframework`
  - `Source/ThirdParty/iOS/Frameworks/cloudsave_sdk.xcframework`
  Both must be added via `PublicAdditionalFrameworks` in `TapCloudSave.Build.cs`.
  Without `cloudsave_sdk`, the app-link step will fail with undefined C++ symbol
  errors (e.g. `boost::asio::ssl::...`).
- `cloudsave_sdk.xcframework` is copied verbatim from
  `iOS/TapTapSDK/TapTapCloudSave/Framework/cloudsave_sdk.xcframework` — re-copy
  whenever that upstream file changes.
