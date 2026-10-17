# TapCloudSave Native SDK Configuration

跨 5 平台的原生依赖一览。Desktop + iOS 的二进制布局、构建命令、提取流程在
[`Source/ThirdParty/NATIVE_SDK_CONFIG.md`](Source/ThirdParty/NATIVE_SDK_CONFIG.md)
有完整细节，本文档作为 Plugin 根入口，叠加 Android Maven 坐标 + 升级流程清单。

## 平台总览

| 平台 | 接入方式 | 资产/坐标 | 当前状态 |
|---|---|---|---|
| Win64   | ThirdParty DLL (delay-load) | `Source/ThirdParty/Desktop/Win64/cloudsave_sdk.dll` | ✅ 已随仓库 |
| macOS   | ThirdParty dylib            | `Source/ThirdParty/Desktop/Mac/libcloudsave_sdk.dylib` | ✅ 已随 UE5 插件交付 |
| Linux   | ThirdParty so               | `Source/ThirdParty/Desktop/Linux/libcloudsave_sdk.so` | ⚠️ NOT shipped，需单独获取 |
| iOS     | xcframework（Git LFS）      | `Source/ThirdParty/iOS/Frameworks/TapTapCloudSaveSDK.xcframework` + `cloudsave_sdk.xcframework` | ✅ 已随仓库 |
| Android | Maven 远端依赖（UPL）       | `com.taptap.sdk:tap-cloudsave:4.10.3-beta.2`+（含 `TapCloudSaveUERouter`） | ⚠️ 待 Android Release 发版至 Maven |

`Build.cs` 对 Mac 缺少 dylib 时会直接失败，避免生成运行期固定返回 `InitFail (300002)` 的包；请先拉取 Git LFS 资产。Win64/Linux 的现有缺失库策略保持不变。

## iOS

- xcframework 路径（两个，必须配对使用）：
  - `Source/ThirdParty/iOS/Frameworks/TapTapCloudSaveSDK.xcframework`（Swift/ObjC 桥，暴露 `BridgeCloudSaveService`）
  - `Source/ThirdParty/iOS/Frameworks/cloudsave_sdk.xcframework`（C++ core 静态库；上者依赖它）
- 源 iOS 模块：`iOS/TapTapSDK/TapTapCloudSave/`（scheme `TapTapCloudSaveSDK`，workspace `iOS/TapTapSDK/TapTapSDK.xcworkspace`）
- Mach-O：`staticlib`（静态归档，需 UPL 注入 `OTHER_LDFLAGS = -ObjC` 防止 category symbols 被剥离）
- 完整构建命令（device + simulator + create-xcframework）：见
  [`Source/ThirdParty/NATIVE_SDK_CONFIG.md` § iOS](Source/ThirdParty/NATIVE_SDK_CONFIG.md)

## Android

- Maven 坐标：`com.taptap.sdk:tap-cloudsave:<version>`
  - **不是** `tap-cloudsave-unity`；UE 复用的 `TapCloudSaveUERouter` 位于 `tap-cloudsave` 主模块，避免引入 Unity 桥依赖。
  - 当前 UPL 钉版本：`4.10.3-beta.2`（见 `Source/TapCloudSave/TapCloudSave_Android_UPL.xml`）。
- 必要前置：Android Release Pipeline 必须将 `tap-cloudsave` 4.10.3-beta.2+ 推到内部 Maven，否则 gradle 报
  `Could not find com.taptap.sdk:tap-cloudsave:4.10.3-beta.2`。
- Router 类：`com.taptap.sdk.cloudsave.internal.ue.TapCloudSaveUERouter`（源在 `Android/TapCloudSave/`）。
- UPL 注入：`buildGradleAdditions` 添加 dependency；`gameActivityClassAdditions` 注入 `routeToTapCloudSave(...)`；
  ProGuard 已 keep `com.taptap.sdk.cloudsave.**` 及 `internal.ue.**`。
- AndroidManifest：`INTERNET` + `ACCESS_NETWORK_STATE` 权限。

## Desktop (Win64 / Mac / Linux)

- 共享头：`Source/ThirdParty/Desktop/Include/cloudsave_sdk.h`
  - 与 Unity 端 `Unity/Assets/TapSDK/CloudSave/Standalone/Runtime/Internal/TapCloudSaveWrapper.cs` 保持签名一致（含 `extraParams`，UE5 wrapper 始终传 `""`）。
- Win64 链接策略：`PublicDelayLoadDLLs` + `RuntimeDependencies` stage 到 `$(BinaryOutputDir)`。
- Mac / Linux 链接策略：`PublicAdditionalLibraries` + `RuntimeDependencies` stage 到 `$(BinaryOutputDir)`（`PublicDelayLoadDLLs` 仅限 Windows）。
- 二进制来源（与 Unity 共用）：
  - Win64: `Unity/Assets/TapSDK/CloudSave/Standalone/Plugins/x86_64/cloudsave_sdk.dll`
  - Mac: `UE5/Plugins/TapCloudSave/Source/ThirdParty/Desktop/Mac/libcloudsave_sdk.dylib`，与 Unity `Standalone/Plugins/macOS/libcloudsave_sdk.dylib` 使用同源 universal Mach-O（arm64 + x86_64）
  - Linux: 上游单独提供
- 提取/落盘细节：见
  [`Source/ThirdParty/NATIVE_SDK_CONFIG.md` § Drop-in steps](Source/ThirdParty/NATIVE_SDK_CONFIG.md)

## 升级流程 Checklist

原生 SDK 版本变更时，按此清单执行：

- [ ] **Desktop**：替换 `Source/ThirdParty/Desktop/{Win64,Mac,Linux}/` 下二进制；若头签名变化，同步 `Source/ThirdParty/Desktop/Include/cloudsave_sdk.h` 与 `Private/Desktop/TapCloudSaveDesktopWrapper.cpp`。
- [ ] **iOS**：按 `Source/ThirdParty/NATIVE_SDK_CONFIG.md` § iOS 重跑构建命令，覆盖两个 xcframework；如 `cloudsave_sdk.xcframework` 上游有更新，从 `iOS/TapTapSDK/TapTapCloudSave/Framework/` 重拷。Git LFS 跟踪，注意提交。
- [ ] **Android**：调整 `TapCloudSave_Android_UPL.xml` 中 `com.taptap.sdk:tap-cloudsave:<version>` 版本号；确认对应版本已发到 Maven。
- [ ] **版本常量**：如果引入了 SDK 版本字段，同步 `Source/TapCloudSave/Public/TapCloudSave.h` 中的版本号。
- [ ] **测试**：跑 Phase 5 单元测试（4 个文件）+ Phase 6 集成测试 + Demo 面板 7 API 手测。
- [ ] **平台清单**：若某平台二进制持续缺失，从 `TapCloudSave.uplugin` 的 `PlatformAllowList` 移除并在 `README.md` 平台矩阵更新状态。
