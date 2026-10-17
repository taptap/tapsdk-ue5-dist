# TapCore Android / iOS 桥接总结

更新时间: 2026-06-15

本文总结 `UE5/Plugins/TapCore` 如何把 UE5 侧的 TapCore API 桥接到 Android 和 iOS 原生 TapCore 模块。结论先行:

- UE 公共入口是 `FTapTapSDK` 和 `FTapTapEvent`，外部调用不直接接触 JNI 或 Objective-C。
- Android 走 `JNI -> GameActivity.routeToTapCore -> TapCoreUERouter -> TapCoreUEBridge / TapEventUEBridge -> Android tap-core`。
- iOS 走 `Objective-C++ -> TapCoreIOSBridge -> TapTapCoreSDK / TapTapEvent`。
- 两端都以 `FTapTapSdkOptions` + module options 为初始化配置源，但原生侧落地方式不同: Android 使用 Kotlin Router 的 JSON 协议，iOS 使用 ObjC 桥接类手动组装 SDK options。

## 1. UE 侧公共抽象

TapCore 的 UE API 层主要由两组门面组成:

| UE 入口 | 作用 | 平台抽象 |
| --- | --- | --- |
| `FTapTapSDK` | SDK 初始化、版本、初始化状态、语言、Gatekeeper | `ITapCorePlatform` |
| `FTapDB` / `FTapTapEvent` | TapDB / TapTapEvent 初始化与事件上报 | `ITapEventPlatform` |

`FTapTapSDK::Init` 的主流程:

1. 校验 `FTapTapSdkOptions.ClientID` 和 `FTapTapSdkOptions.ClientToken`。
2. `FTapSdkConfigState::SetCurrentConfig(Config)` 保存 SDK 内部全局配置。
3. 根据编译平台创建实现:
   - Android: `FTapCoreAndroid`
   - iOS: `FTapCoreiOS`
   - Desktop: `FTapCoreDesktop`
4. 调用平台实现的 `Init(Config)`。
5. 调用 `FTapDB::Init(Config)`，进而初始化 `FTapTapEvent` 的平台实现。
6. 初始化 UE 侧 HTTP 工具。
7. 移动端 Gatekeeper 由原生 tap-core 处理，UE 侧只调用平台实现的占位方法。

对应源码:

- `Source/TapCore/Public/TapCore.h`
- `Source/TapCore/Private/TapCore.cpp`
- `Source/TapCore/Public/Platform/ITapCorePlatform.h`
- `Source/TapCore/Public/TapTapEvent.h`
- `Source/TapCore/Private/TapTapEvent.cpp`

## 2. 构建与依赖接入

### 2.1 `TapCore.Build.cs`

`TapCore.Build.cs` 按平台挂接不同原生依赖:

| 平台 | 关键配置 |
| --- | --- |
| Android | 增加 `Launch` 模块，注册 `TapCore_Android_UPL.xml`，加入 `Private/Mobile/Android` include path |
| iOS | 增加 `Launch` 模块，注册 `TapCore_iOS_UPL.xml`，链接系统 framework 和随插件携带的 xcframework |
| Apple 平台 | 因混用 C++ 与 Objective-C++，`PCHUsage = NoPCHs` |
| 全平台 | `bUseUnity = false`，避免平台源文件混编冲突 |

iOS 随插件携带并复制的主要 framework:

- `THEMISLite.xcframework`
- `TapTapBasicToolsSDK.xcframework`
- `TapTapCoreSDK.xcframework`
- `TapTapGidSDK.xcframework`
- `TapTapNetworkSDK.xcframework`
- `TapTapSDKBridgeCore.xcframework`
- `tapsdkcorecpp.xcframework`

### 2.2 Android UPL

`TapCore_Android_UPL.xml` 做了四件事:

1. Gradle 依赖:
   - `implementation 'com.taptap.sdk:tap-core:2.2.2-alpha.13'`
   - 同时补充 AndroidX、Kotlin、Gson、OkHttp、Room 等依赖。
2. Maven 仓库:
   - 内部 GitHub raw Maven。
   - TapTap Maven。
   - `mavenLocal`、Maven Central、Google。
3. Manifest / Proguard:
   - 添加网络相关权限。
   - keep `com.taptap.sdk.core`、`kit`、`common`、`initializer`、`db`、`openlog`、`gid` 以及 UE Router 相关类。
4. 向 `GameActivity` 注入路由入口:

```java
import com.taptap.sdk.core.internal.ue.TapCoreUERouter;
import com.taptap.sdk.kit.internal.enginebridge.EngineBridgeCallback;

public String routeToTapCore(String requestJson, EngineBridgeCallback callback) {
    try {
        return TapCoreUERouter.route(this, requestJson, callback);
    } catch (Exception e) {
        android.util.Log.e(TAG, "TapCore route error: " + e.getMessage(), e);
        return "{\"success\":false,\"message\":\"" + e.getMessage() + "\"}";
    }
}
```

### 2.3 iOS UPL

`TapCore_iOS_UPL.xml` 主要负责 Xcode 工程配置:

- 写入 Info.plist 占位配置:
  - `TapTapSDKClientID`
  - `TapTapSDKClientToken`
  - `TapTapSDKRegion`
- 配置 ATS。
- 开启 Objective-C exceptions 和 ARC。
- C++ 标准设为 `gnu++17`。
- 关闭 bitcode。
- 设置 iOS deployment target 为 `15.0`。
- 添加 `OTHER_LDFLAGS = $(inherited) -ObjC`，保证 ObjC category 能被链接进来。

## 3. Android 桥接链路

Android 使用“UE 专属 Router”模式。UE C++ 不直接调用 `TapTapSdk`，而是通过 JNI 调 GameActivity 的路由方法，再由 Kotlin Router 分发到 tap-core 原生实现。

### 3.1 初始化调用链

```text
FTapTapSDK::Init(Config)
  -> FTapTapSDK::CreatePlatformImpl()
  -> FTapCoreAndroid::Init(Config)
  -> FTapCoreAndroid::CallAndroidInit(Config)
  -> FTapCoreAndroid::CallModuleRoute("init", ConfigJson)
  -> GameActivity.routeToTapCore(requestJson, null)
  -> TapCoreUERouter.route(activity, requestJson, callback)
  -> TapCoreUEBridge.init(activity, configJson)
  -> TapTapSdk.init(activity, nativeConfig, optionStrings)
```

`FTapCoreAndroid::CallAndroidInit` 还会先通过内部 route 设置两类运行时标识:

- `setPlatformAndVersion`: 写入 `TapSDK-UE` 和游戏版本。
- `setSDKArtifact`: 写入 `UE5`。

随后它把核心配置和 module options 转成内部 JSON，并调用 Router 的 `init` 方法。初始化后还会通过 JNI 调 `Activity.setRequestedOrientation` 应用横竖屏设置。

### 3.2 C++ 到 GameActivity

`FTapCoreAndroid::CallModuleRoute` 是 Android 同步调用的核心方法:

1. 获取 `JNIEnv`。
2. 获取 `FAndroidApplication::GetGameActivityThis()`。
3. 查找 GameActivity 上的实例方法:

```text
routeToTapCore(
  java.lang.String,
  com.taptap.sdk.kit.internal.enginebridge.EngineBridgeCallback
) : java.lang.String
```

4. 传入请求 JSON 和 `null` callback。
5. 得到返回 JSON 字符串。

请求 JSON 由 `CreateRouteRequest` 生成，固定包含:

```json
{
  "method": "init",
  "params": "{\"clientId\":\"...\",\"clientToken\":\"...\"}"
}
```

响应 JSON 约定:

```json
{"success":true}
```

```json
{"success":true,"data":"..."}
```

```json
{"success":false,"message":"..."}
```

### 3.3 Kotlin Router

Android 原生侧入口是:

- `Android/sdk/tap-core/src/main/java/com/taptap/sdk/core/internal/ue/TapCoreUERouter.kt`

Router 的职责:

1. 解析 `RouteRequest(method, params)`。
2. 根据 `RouteMethod` 枚举校验:
   - 方法名是否存在。
   - 是否需要 Activity。
   - 是否需要 params。
   - 是否需要 async callback。
3. 按方法分发到:
   - `TapCoreUEBridge`
   - `TapEventUEBridge`
4. 返回统一 JSON 响应。

核心方法映射:

| Router method | Kotlin 目标 | 用途 |
| --- | --- | --- |
| `init` | `TapCoreUEBridge.init` | 初始化 Android tap-core |
| `isInitialized` | `TapCoreUEBridge.isInitialized` | 查询初始化状态 |
| `updateLanguage` | `TapCoreUEBridge.updateLanguage` | 更新语言 |
| `setPlatformAndVersion` | `TapCoreUEBridge.setPlatformAndVersion` | 设置 XUA 平台标识 |
| `setSDKArtifact` | `TapCoreUEBridge.setSDKArtifact` | 设置 SDK 制品标识 |
| `setUserId` / `clearUser` / `getDeviceId` / `logEvent` | `TapCoreUEBridge` | 基础 TapTapEvent 能力 |
| `deviceInitialize` 等扩展事件方法 | `TapEventUEBridge` | TapDB / TapTapEvent 扩展能力 |
| `registerDynamicProperties` | `TapEventUEBridge.registerDynamicProperties` | 动态属性回调 |

### 3.4 Android 原生 TapCore 落地

`TapCoreUEBridge.init` 将 UE 配置落到 Android SDK:

1. 解析内部 `TapCoreUEConfig`，字段来自 UE 核心配置和 module options。
2. 校验 `clientId`、`clientToken`、`region`。
3. 设置平台 UA:
   - `PlatformXUA.addUA("TapSDK-UE", gameVersion)`
4. 设置 RND:
   - `TapTapKit.isRND = config.isRND`
5. 构造 `TapTapEventOptions`:
   - `enableTapDB`
   - `channel`
   - `overrideBuiltInParameters`
   - `disableAutoLogDeviceLogin`
   - `autoIAPEventEnabled`
   - 其他自定义 properties
6. 合并各业务模块通过内部 registry 序列化的 option JSON；TapCore 不依赖具体业务模块类型。
7. 切到 Android main looper 调用:

```kotlin
TapTapSdk.init(activity, config.toNativeConfig(), optionStrings)
```

注意: `ensureMainThread` 在非 main looper 时会 `post` 到主线程。UE 侧收到的 route 成功响应表示初始化请求已被接受或投递，实际原生初始化由 Android 主线程执行。

### 3.5 Android 异步回调

TapCore 提供可复用的异步回调基础设施，供 TapCore 或其他 UE 模块使用。

C++ 创建回调:

```text
FTapCoreAndroid::CreateEngineBridgeCallback
  -> new EngineBridgeCallbackProxy(callbackId)
  -> 保存 jobject global ref
  -> CallbackRegistry[callbackId] = C++ callback
```

Kotlin 回调代理:

```kotlin
class EngineBridgeCallbackProxy(private val nativeHandle: Long) : EngineBridgeCallback {
    override fun onResult(jsonString: String) {
        nativeOnResult(nativeHandle, jsonString)
    }

    private external fun nativeOnResult(nativeHandle: Long, jsonString: String)
}
```

C++ JNI 导出:

```text
Java_com_taptap_sdk_kit_internal_enginebridge_EngineBridgeCallbackProxy_nativeOnResult(
  JNIEnv*,
  jobject,
  jlong NativeHandle,
  jstring JsonString
)
```

收到回调后，`FTapCoreAndroid::DispatchGlobalCallback` 会先查 TapCore 自己的 `CallbackRegistry`，再查其他模块注册的全局 handler，最终在 GameThread 执行 C++ callback。

TapEvent 动态属性是另一条同步 JNI 回调:

```text
TapEventUEBridge.registerDynamicProperties
  -> EngineBridgeCallbackProxy.nativeGetDynamicProperties()
  -> FTapEventAndroid 保存的 C++ DynamicPropertiesCallback
```

## 4. iOS 桥接链路

iOS 不走 Router，也没有 JNI。UE Objective-C++ 文件直接调用一个纯 Objective-C 桥接类，桥接类再调用随插件携带的 iOS xcframework。

### 4.1 初始化调用链

```text
FTapTapSDK::Init(Config)
  -> FTapTapSDK::CreatePlatformImpl()
  -> FTapCoreiOS::Init(Config)
  -> FTapCoreiOS::CalliOSInit(Config)
  -> dispatch_async(dispatch_get_main_queue())
  -> [TapCoreIOSBridge initWithConfig:completion:]
  -> [TapTapSDK initWithOptions:otherOptions:]
  -> completion
  -> AsyncTask(GameThread)
  -> FTapCoreiOS::OnInitComplete
```

对应文件:

- `Source/TapCore/Private/Mobile/iOS/TapCoreiOS.mm`
- `Source/TapCore/Private/Mobile/iOS/TapCoreIOSBridge.h`
- `Source/TapCore/Private/Mobile/iOS/TapCoreIOSBridge.mm`

### 4.2 ObjC++ 平台层

`FTapCoreiOS::CalliOSInit` 做三件事:

1. `ConvertConfigToJson` 把核心配置和 module options 转成内部 JSON。
2. 切到 iOS main queue。
3. 调用:

```objc
[TapCoreIOSBridge initWithConfig:nsConfigJson
                      completion:^(BOOL success, NSString* errorMsg) {
    ...
}];
```

初始化成功后，`TapCoreiOS.mm` 会应用横竖屏:

- iOS 16 及以上: `UIWindowScene requestGeometryUpdateWithPreferences`。
- iOS 15: `UIDevice` KVC + `attemptRotationToDeviceOrientation`。

然后通过 `AsyncTask(ENamedThreads::GameThread, ...)` 回到 UE GameThread，调用 `OnInitComplete` 更新 `bIsInitialized`。

### 4.3 纯 Objective-C 桥接类

`TapCoreIOSBridge` 的目标是隔离 UE ObjC++ 和原生 iOS SDK:

```text
C++ / UE
  -> Objective-C++: TapCoreiOS.mm / TapEventiOS.mm
  -> Pure ObjC: TapCoreIOSBridge
  -> TapTapCoreSDK / TapTapEvent
```

`TapCoreIOSBridge.mm` 直接 import:

```objc
#import <TapTapCoreSDK/TapTapSDK.h>
#import <TapTapCoreSDK/TapTapEvent.h>
#import <TapTapBasicToolsSDK/TapTapSDKHostConfiguration.h>
```

初始化时主要做:

1. 解析 JSON 配置。
2. 校验 `clientId`、`clientToken`。
3. RND 环境下通过 runtime selector 调:

```objc
[TapTapSDKHostConfiguration performSelector:NSSelectorFromString(@"switchToRnd")]
```

4. 创建 `TapTapSdkOptions`:
   - `clientId`
   - `clientToken`
   - `region`
   - `gameVersion`
   - `enableLog`
   - `screenOrientation`
5. 创建 `TapTapEventOptions`，并通过 `enableTapTapEvent` 传递开关:
   - `enableTapTapEvent`
   - `channel`
   - `overrideBuiltInParameters`
   - `enableAutoIAPEvent`
   - `caid`
   - 自定义 properties
6. 通过内部 registry 提供的原生类名进行 runtime 反射，并调用各模块 option 的 `initWithJson:` 工厂，避免 TapCore 编译期依赖业务模块，也避免 Core 逐字段 KVC 写入业务配置。
7. 调用:

```objc
[TapTapSDK initWithOptions:options otherOptions:otherOptions];
```

### 4.4 iOS 事件桥接

iOS 的 `FTapEventiOS` 不走 JSON Router，而是直接调用 `TapCoreIOSBridge`:

```text
FTapTapEvent::LogEvent
  -> FTapEventiOS::LogEvent
  -> [TapCoreIOSBridge eventLogEvent:properties:]
  -> [TapTapEvent logEvent:properties:]
```

典型映射:

| UE C++ | ObjC bridge | iOS SDK |
| --- | --- | --- |
| `SetUserID` | `eventSetUserID` | `[TapTapEvent setUserID:]` |
| `ClearUser` | `eventClearUser` | `[TapTapEvent clearUser]` |
| `GetDeviceId` | `eventGetDeviceId` | `[TapTapEvent getDeviceId]` |
| `LogEvent` | `eventLogEvent` | `[TapTapEvent logEvent:properties:]` |
| `LogPurchasedEvent` | `eventLogPurchasedEvent` | `[TapTapEvent logPurchasedEvent:productName:amount:currencyType:paymentMethod:properties:]` |
| `RegisterDynamicProperties` | `eventRegisterDynamicProperties` | `[TapTapEvent registerDynamicProperties:]` |

动态属性在 iOS 上通过 ObjC block 回调 C++ `TFunction`:

```text
FTapEventiOS::RegisterDynamicProperties
  -> 保存 DynamicPropertiesCallback
  -> [TapCoreIOSBridge eventRegisterDynamicProperties:^NSDictionary* { ... }]
  -> block 内调用 C++ callback，返回 NSDictionary
```

## 5. Android 与 iOS 的关键差异

| 维度 | Android | iOS |
| --- | --- | --- |
| 原生依赖来源 | Maven AAR: `com.taptap.sdk:tap-core:2.2.2-alpha.13` | 插件内 vendored xcframework |
| UE 到原生入口 | JNI 调 `GameActivity.routeToTapCore` | ObjC++ 调 `TapCoreIOSBridge` |
| 原生分发方式 | Kotlin `TapCoreUERouter` 按 `method` 字符串分发 | ObjC 方法直接映射 |
| 参数协议 | `{method, params}` JSON，`params` 本身通常也是 JSON 字符串 | 初始化与属性参数使用 JSON 字符串，方法调用直接传 ObjC 参数 |
| 返回协议 | Router JSON: `success/data/message` | 初始化用 completion block，普通事件多为 fire-and-forget |
| 异步回调 | `EngineBridgeCallbackProxy(nativeHandle)` + JNI nativeOnResult | ObjC block |
| 动态属性 | Android SDK 同步调用 `nativeGetDynamicProperties()` | iOS SDK 调 ObjC block，block 再调用 C++ callback |
| 横竖屏 | JNI 调 `Activity.setRequestedOrientation` | iOS 16 用 scene geometry，iOS 15 用 UIDevice KVC |
| Gatekeeper | Android tap-core 内部处理 | iOS tap-core 内部处理 |

## 6. 配置字段流向

UE 初始化配置分为核心 `FTapTapSdkOptions` 和各模块自己的 options，与 Unity 的核心配置 + module options 分层一致。`FTapComplianceOptions` 归属 TapCompliance，`FTapAchievementOptions` 归属 TapAchievement；TapCore 只通过内部 registry 分发序列化结果。移动端主要字段流向如下:

| UE 字段 | Android | iOS |
| --- | --- | --- |
| `ClientID` | `clientId` -> `TapCoreUEConfig` -> `TapTapSdkOptions` | `clientId` -> `TapTapSdkOptions.clientId` |
| `ClientToken` | `clientToken` -> `TapTapSdkOptions` | `clientToken` -> `TapTapSdkOptions.clientToken` |
| `Region` | `region` -> `TapTapRegion` | `region` -> `TapTapRegionTypeCN / Overseas` |
| `PreferredLanguage` | `preferredLanguage` -> `TapTapLanguage` | `preferredLanguage` -> `TapTapSdkOptions.preferredLanguage` |
| `bEnableLog` | `enableLog` | `enableLog` |
| `GameVersion` | `gameVersion`，空值时原生 TapDB 读取应用 `versionName`；也用于 `PlatformXUA` | `gameVersion`，空值时沿用主包 `CFBundleShortVersionString` |
| `ScreenOrientation` | `Activity.setRequestedOrientation` | `TapTapSdkOptions.screenOrientation` + UE 侧旋转处理 |
| `FTapTapEventOptions` | `TapTapEventOptions` | `TapTapEventOptions` |
| `FTapComplianceOptions` | module option JSON 传入 `TapTapSdk.init` | 内部创建 `TapTapComplianceOptions` |
| `FTapAchievementOptions` | module option JSON 传入 `TapTapSdk.init` | 内部创建 `TapTapAchievementOptions` |
| 内部 RND 开关 | `TapTapKit.isRND` | `TapTapSDKHostConfiguration.switchToRnd` |

原生 option 类型标识和 JSON 序列化信息只存在于 SDK 内部 registry，不作为 `moduleName` 等可配置字段暴露给 UE 开发者。RND 开关同样不对开发者暴露，默认为 `false`；仅 Demo 和 SDK 内部测试通过 `FTapSdkConfigState` 显式切换到研发环境。

## 7. 扩展或维护时的注意点

1. Android 新增 Router 方法时，需要同时更新:
   - UE C++ 方法名常量或调用点。
   - `Android/sdk/tap-core/.../TapCoreUERouter.kt` 的 `RouteMethod`。
   - `TapCoreUEBridge.kt` 或 `TapEventUEBridge.kt` 的实际实现。
   - Proguard keep 规则如果引入新包名，也要补充。

2. Android 异步方法必须确认 callback 契约:
   - 一次性异步调用可复用 `EngineBridgeCallbackProxy(nativeHandle)`。
   - C++ 侧需要注册 callback，并确保回调最终切回 GameThread。
   - 如果其他模块复用 TapCore 的 callback 分发，需要注册 `RegisterGlobalCallbackHandler`。

3. Android C++ 当前多处手写 JSON 字符串。新增字符串字段时要注意转义，避免用户数据里包含引号或反斜杠导致 Router 解析失败。

4. iOS 新增原生能力时优先放到 `TapCoreIOSBridge`，再由 `.mm` 平台实现调用。这样可以把 SDK import 集中在纯 ObjC 桥接层，减少 UE ObjC++ 编译问题。

5. iOS 如果新增 framework:
   - 加到 `Source/ThirdParty/iOS/Frameworks`。
   - 在 `TapCore.Build.cs` 里加 `PublicAdditionalFrameworks`。
   - 如需要 ObjC category，确认 `-ObjC` 仍然保留。

6. 两端都要保持 `FTapTapSdkOptions` JSON 字段名一致。Android 由 `TapCoreUEConfig` 反序列化，iOS 由 `TapCoreIOSBridge` 手动读取 NSDictionary，任何字段改名都需要双端同步。

7. 移动端 Gatekeeper 当前由原生 tap-core 自动处理，UE 不保存移动端 gatekeeper URL。`GetGatekeeperConfigUrl` 在移动端返回空字符串是预期行为。

## 8. 关键源码索引

UE 侧:

- `UE5/Plugins/TapCore/Source/TapCore/Public/TapCore.h`
- `UE5/Plugins/TapCore/Source/TapCore/Private/TapCore.cpp`
- `UE5/Plugins/TapCore/Source/TapCore/Public/Platform/ITapCorePlatform.h`
- `UE5/Plugins/TapCore/Source/TapCore/Private/Mobile/Android/TapCoreAndroid.cpp`
- `UE5/Plugins/TapCore/Source/TapCore/Private/Mobile/Android/TapEventAndroid.cpp`
- `UE5/Plugins/TapCore/Source/TapCore/Private/Mobile/iOS/TapCoreiOS.mm`
- `UE5/Plugins/TapCore/Source/TapCore/Private/Mobile/iOS/TapEventiOS.mm`
- `UE5/Plugins/TapCore/Source/TapCore/Private/Mobile/iOS/TapCoreIOSBridge.h`
- `UE5/Plugins/TapCore/Source/TapCore/Private/Mobile/iOS/TapCoreIOSBridge.mm`
- `UE5/Plugins/TapCore/Source/TapCore/TapCore.Build.cs`
- `UE5/Plugins/TapCore/Source/TapCore/TapCore_Android_UPL.xml`
- `UE5/Plugins/TapCore/Source/TapCore/TapCore_iOS_UPL.xml`

Android 原生侧:

- `Android/sdk/tap-core/src/main/java/com/taptap/sdk/core/internal/ue/TapCoreUERouter.kt`
- `Android/sdk/tap-core/src/main/java/com/taptap/sdk/core/internal/ue/TapCoreUEBridge.kt`
- `Android/sdk/tap-core/src/main/java/com/taptap/sdk/core/internal/ue/TapCoreUEConfig.kt`
- `Android/sdk/tap-core/src/main/java/com/taptap/sdk/core/internal/ue/TapEventUEBridge.kt`
- `Android/sdk/tap-common/src/main/java/com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback.kt`
- `Android/sdk/tap-common/src/main/java/com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy.kt`

iOS 原生侧参考:

- `iOS/TapTapSDK/TapSDKCore/Public/TapTapSDK.h`
- `iOS/TapTapSDK/TapSDKCore/Public/TapTapEvent.h`
- `iOS/TapTapSDK/TapSDKCore/Private/UnityWrapper/BridgeCoreService.m`
