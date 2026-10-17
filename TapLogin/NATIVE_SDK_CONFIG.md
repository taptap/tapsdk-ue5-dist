# TapLogin 原生 SDK 配置说明

## 概述

TapLogin 插件使用桥接方式集成原生 SDK：
- **Android**: 通过 Maven 远程依赖 + JNI 桥接
- **iOS**: 通过 CocoaPods 远程依赖 + Objective-C++ 桥接
- **Desktop**: 纯 UE C++ 实现（OAuth 2.0 流程）

## Android 配置

### 1. Maven 远程依赖

**配置文件**: `TapLogin_Android_UPL.xml`

**Maven 仓库**:
```gradle
repositories {
    // TapSDK 官方仓库
    maven {
        url "https://raw.githubusercontent.com/TapTap/TapSDK-Android/main/repository"
    }
    // 备用：JitPack
    maven {
        url "https://jitpack.io"
    }
}
```

**依赖配置**:
```gradle
dependencies {
    // TapTap 登录 SDK（自动获取最新稳定版）
    implementation 'com.taptap.sdk:tap-login:latest.release'

    // 如果需要指定版本
    // implementation 'com.taptap.sdk:tap-login:3.x.x'
}
```

### 2. AndroidManifest.xml 配置

**URL Scheme**:
```xml
<activity
    android:name="com.taptap.sdk.login.TapLoginActivity"
    android:exported="true"
    android:theme="@android:style/Theme.Translucent.NoTitleBar">
    <intent-filter>
        <action android:name="android.intent.action.VIEW" />
        <category android:name="android.intent.category.DEFAULT" />
        <category android:name="android.intent.category.BROWSABLE" />
        <data
            android:scheme="tapsdk"
            android:host="login" />
    </intent-filter>
</activity>
```

### 3. ProGuard 混淆规则

```proguard
# TapSDK Login
-keep class com.taptap.sdk.login.** { *; }
-keep interface com.taptap.sdk.login.** { *; }

# TapSDK Core
-keep class com.taptap.sdk.core.** { *; }

# JNI 方法
-keepclasseswithmembernames class * {
    native <methods>;
}
```

### 4. JNI 桥接（待实现）

**Java 类路径**: `com.taptap.sdk.login.TapLogin`

**主要方法**:
```java
// 初始化
TapLogin.init(Context context, String clientId, String region);

// 登录
TapLogin.loginWithScopes(Activity activity, String[] scopes, TapLoginCallback callback);

// 一键登录
TapLogin.oneKeyLogin(Activity activity, String phoneToken, TapLoginCallback callback);

// 登出
TapLogin.logout();

// 获取当前账号
TapAccount account = TapLogin.getCurrentAccount();
```

**JNI 实现位置**: `TapLoginPlatformAndroid.cpp`

## iOS 配置

### 1. CocoaPods 远程依赖

**配置文件**: `TapLogin_IOS_UPL.xml`

**Podfile 配置**:
```ruby
# TapTap 登录 SDK
pod 'TapLoginSDK', '~> 3.0'

# 或者指定具体版本
# pod 'TapLoginSDK', '3.x.x'

# TapTap 核心 SDK（如果需要）
# pod 'TapCommonSDK', '~> 3.0'
```

### 2. Info.plist 配置

**URL Scheme**:
```xml
<key>CFBundleURLTypes</key>
<array>
    <dict>
        <key>CFBundleURLSchemes</key>
        <array>
            <string>tapsdk</string>
        </array>
        <key>CFBundleURLName</key>
        <string>com.taptap.sdk.login</string>
    </dict>
</array>

<key>LSApplicationQueriesSchemes</key>
<array>
    <string>tapsdk</string>
    <string>tapiosdk</string>
</array>
```

### 3. 系统框架依赖

自动添加的框架：
- Foundation.framework
- UIKit.framework
- SafariServices.framework
- AuthenticationServices.framework
- WebKit.framework

### 4. Objective-C++ 桥接（待实现）

**Objective-C 类**: `TapLogin`

**主要方法**:
```objc
// 初始化
[TapLogin initWithClientId:@"xxx" region:@"CN"];

// 登录
[TapLogin loginWithScopes:@[@"public_profile"]
                  callback:^(TapAccount *account, NSError *error) {
    // 处理登录结果
}];

// 一键登录
[TapLogin oneKeyLoginWithPhoneToken:@"xxx"
                           callback:^(TapAccount *account, NSError *error) {
    // 处理登录结果
}];

// 登出
[TapLogin logout];

// 获取当前账号
TapAccount *account = [TapLogin currentAccount];
```

**Objective-C++ 实现位置**: `TapLoginPlatformIOS.mm`（需要创建）

## Desktop 配置

Desktop 平台不依赖原生 SDK，使用纯 UE C++ 实现：

- **OAuth 2.0 授权流程**
- **系统浏览器集成**
- **本地 HTTP 服务器接收回调**（待实现）
- **Token 交换和用户信息获取**

实现位置：`TapLoginPlatformDesktop.cpp`

## 构建和打包

### Android 打包

```bash
# 使用项目提供的脚本
bash Scripts/build_hud_ui.sh

# 或使用 UAT 命令
/Users/Shared/Epic\ Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh \
    BuildCookRun \
    -project="[项目路径]/TapSDKDemo.uproject" \
    -platform=Android \
    -architecture=arm64 \
    -configuration=Development \
    -build -cook -stage -pak -package
```

**注意事项**:
1. 首次打包会自动下载 Maven 依赖（需要网络连接）
2. 依赖下载位置：`~/.gradle/caches/modules-2/`
3. 如果下载失败，检查网络或手动配置代理

### iOS 打包

```bash
# 使用 UAT 命令
/Users/Shared/Epic\ Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh \
    BuildCookRun \
    -project="[项目路径]/TapSDKDemo.uproject" \
    -platform=iOS \
    -configuration=Development \
    -build -cook -stage -pak -package
```

**注意事项**:
1. 首次打包会自动执行 `pod install`（需要安装 CocoaPods）
2. CocoaPods 安装：`sudo gem install cocoapods`
3. 依赖下载位置：项目的 `Pods/` 目录
4. 如果下载失败，检查网络或配置 CocoaPods 源

## 验证配置

### 1. 检查 Gradle 日志（Android）

打包时查看 Gradle 输出，确认依赖下载：

```
> Configure project :app
TapLogin Android UPL: Initializing
TapLogin Android UPL: Configuration completed
TapLogin Android UPL: Maven repository added
TapLogin Android UPL: Dependencies configured

> Task :app:dependencies
+--- com.taptap.sdk:tap-login:3.x.x
     +--- com.taptap.sdk:tap-core:3.x.x
     ...
```

### 2. 检查 CocoaPods 日志（iOS）

打包时查看 CocoaPods 输出：

```
Analyzing dependencies
Downloading dependencies
Installing TapLoginSDK (3.x.x)
Installing TapCommonSDK (3.x.x)
Generating Pods project
Integrating client project
```

### 3. 运行时日志

Android:
```
adb logcat | grep TapSDK
[TapSDK] [Android] TapLogin Platform Initialization
[TapSDK] [Android] TapLogin native SDK available
```

iOS:
```
# Xcode Console 或设备日志
[TapSDK] [iOS] TapLogin Platform Initialization
[TapSDK] [iOS] TapLogin native SDK available
```

## 下一步开发

### Android JNI 桥接实现

需要在 `TapLoginPlatformAndroid.cpp` 中实现：

1. **初始化方法**: 调用 Java 的 `TapLogin.init()`
2. **登录方法**: 调用 Java 的 `loginWithScopes()`
3. **回调处理**: Java 回调 -> JNI -> UE C++ 回调

参考文档：
- [UE JNI 官方文档](https://docs.unrealengine.com/5.0/en-US/API/Runtime/Launch/Android/AndroidJNI/)
- [TapSDK Android 文档](https://developer.taptap.cn/docs/sdk/taptap-login/guide/)

### iOS Objective-C++ 桥接实现

需要创建 `TapLoginPlatformIOS.mm` 文件：

1. **初始化方法**: 调用 Objective-C 的 `[TapLogin initWithClientId:]`
2. **登录方法**: 调用 Objective-C 的 `loginWithScopes:callback:`
3. **回调处理**: Objective-C Block -> C++ Lambda -> UE 回调

参考文档：
- [UE iOS 集成文档](https://docs.unrealengine.com/5.0/en-US/ios-support-for-unreal-engine/)
- [TapSDK iOS 文档](https://developer.taptap.cn/docs/sdk/taptap-login/guide/)

## 故障排除

### Maven 依赖下载失败

1. 检查网络连接
2. 配置 Gradle 代理（如果在国内）
3. 手动下载 AAR 文件并放入本地 Maven 仓库

### CocoaPods 依赖下载失败

1. 检查 CocoaPods 是否已安装：`pod --version`
2. 更新 CocoaPods：`sudo gem install cocoapods --pre`
3. 清理缓存：`pod cache clean --all`
4. 配置国内镜像（如果在国内）

### JNI 调用失败

1. 检查 Java 类路径是否正确
2. 检查方法签名是否匹配
3. 查看 logcat 输出的 JNI 错误信息

### Objective-C++ 调用失败

1. 检查 .mm 文件是否被正确编译
2. 检查头文件是否正确导入
3. 查看 Xcode Console 的错误信息

## 相关文件

- `TapLogin_Android_UPL.xml` - Android UPL 配置
- `TapLogin_IOS_UPL.xml` - iOS UPL 配置
- `TapLogin.Build.cs` - 构建配置
- `TapLoginPlatformAndroid.cpp` - Android 桥接实现
- `TapLoginPlatformIOS.cpp` - iOS 桥接实现
- `TapLoginPlatformDesktop.cpp` - Desktop 原生实现

## 参考资源

- [TapSDK 官方文档](https://developer.taptap.cn/docs/sdk/)
- [UE5 插件开发文档](https://docs.unrealengine.com/5.0/en-US/plugins-in-unreal-engine/)
- [UE5 Android 平台文档](https://docs.unrealengine.com/5.0/en-US/android-platform-support-in-unreal-engine/)
- [UE5 iOS 平台文档](https://docs.unrealengine.com/5.0/en-US/ios-support-for-unreal-engine/)
