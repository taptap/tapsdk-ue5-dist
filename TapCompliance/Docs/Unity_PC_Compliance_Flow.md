# Unity PC 端合规认证（Compliance）流程与代码梳理

> 目的：把 Unity 端 `TapSDK.Compliance` 模块在 **Windows / Mac Standalone** 平台的实现完整拆解，作为 UE5 迁移与后续回溯的参考资料。
>
> 源码根目录：`Unity/Assets/TapSDK/Compliance/`
> 文档侧重：纯讲解 Unity 现状，**不写 UE5 落地建议**。
> 版本依据：`TapTapCompliance.Version = "4.9.3"`（`Runtime/Public/TapTapCompliance.cs:13`）。

---

## 目录

1. [总览](#一总览)
2. [模块物理结构](#二模块物理结构)
3. [平台分发与初始化](#三平台分发与初始化)
4. [门面 API 速查](#四门面-api-速查)
5. [核心数据模型](#五核心数据模型)
6. [Startup 全流程拆解](#六startup-全流程拆解重头戏)
7. [Verification 子系统（实名）](#七verification-子系统实名)
8. [UserConfig & Playable 子系统（策略 + 时段）](#八userconfig--playable-子系统策略--时段)
9. [心跳轮询与剩余时长（CompliancePoll）](#九心跳轮询与剩余时长compliancepoll)
10. [UI 体系](#十ui-体系)
11. [支付链路](#十一支付链路)
12. [Exit / Token 失效闭环](#十二exit--token-失效闭环)
13. [回调码 StartUpResult](#十三回调码-startupresult)
14. [网络层（Network）](#十四网络层network)
15. [已知坑与注意点](#十五已知坑与注意点)

---

## 一、总览

合规模块要解决三件事：

1. **实名（Verification）**：拿到 `anti_addiction_token`（v2 token），证明这个 `userId` 是哪个真人在使用。
2. **适龄 + 策略下发（UserConfig）**：拿到当前用户的年龄段、是否被适龄限制、心跳间隔、时段配置等。
3. **时段 / 时长判定（Playable）**：当前能否游戏、剩余多久；并通过周期心跳持续校验。

`TapTapCompliance` 门面对外提供 8 个静态 API，PC 端最终落到 **`ComplianceNewJob`（作业层） → `TapTapComplianceManager`（全局状态） → `BaseComplianceWorker` / `ComplianceWorker`（编排层） → `Verification / Config / CompliancePoll / Network`（功能子系统） → UI Controller** 的调用链。

> 关键约束：当前 PC 实现 **仅支持中国大陆**。Worker 启动时调 `CheckInitState()`，发现 region 为 `Overseas` 直接弹错并 return（`ComplianceNewJob.cs:288`）。

---

## 二、模块物理结构

```
Unity/Assets/TapSDK/Compliance/
├── Runtime/
│   ├── Public/
│   │   ├── TapTapCompliance.cs                    # 门面（业务方唯一入口）
│   │   └── TapTapComplianceOption.cs              # 初始化配置（showSwitchAccount / useAgeRange）
│   └── Internal/
│       ├── ComplianceJobManager.cs                # 反射按平台选 Job
│       ├── IComplianceJob.cs                      # 作业层契约
│       ├── ComplianceResult.cs                    # 对外数据结构（CheckPayResult、回调 extras 等）
│       ├── Init/ComplianceInitTask.cs             # IInitTask（Order=12）
│       └── Model/StartUpResult.cs                 # 回调码常量
├── Mobile/Runtime/                                # Android / iOS 桥接（本文不展开）
│   ├── ComplianceMobileOldJob.cs
│   └── ComplianceMobileBridge.cs
└── Standalone/Runtime/                            # ★ PC 实现 ★
    ├── ComplianceWorker.cs                        # 中国 Worker（手动认证 + TapToken 快速认证）
    └── Internal/
        ├── TapTapComplianceManager.cs             # 全局状态机
        ├── Verification.cs                        # 实名状态（内存 + 磁盘双层缓存）
        ├── Config.cs                              # 全局合规配置
        ├── CompliancePoll.cs                      # MonoBehaviour 心跳协程
        ├── Network.cs                             # HTTP API
        ├── ComplianceConst.cs                     # 字符串常量（VERIFICATION_STATUS_*、Prefab path 等）
        ├── ComplianceException.cs                 # 业务异常（IsTokenExpired 判断 200000）
        ├── Persistence.cs                         # 磁盘持久化封装
        ├── Tool.cs                                # EncryptString 等工具
        ├── TapComplianceUI.cs                     # UI 工厂入口
        ├── TapLoginPermissionProvider.cs
        ├── AccessTokenJsonConverter.cs
        ├── Tracker/TapComplianceTracker.cs        # 打点（TrackInit / TrackStart / TrackSuccess / TrackFailure / TrackCancel）
        ├── Http/ComplianceHttpClient.cs
        ├── Model/                                 # 数据契约
        │   ├── Verification.cs                    # VerificationResult / LocalVerification
        │   ├── UserComplianceConfig.cs            # UserState / Policy / AgeCheckResult / Local + TimeRangeConfig
        │   ├── Playable.cs / Payable.cs / SubmitPayment.cs
        │   ├── RealNameConfig.cs / ServerTime.cs / BaseResponse.cs / ErrorResponse.cs / StandaloneResponse.cs
        └── UI/Controller/                         # 4 个 UI 面板控制器
            ├── TaptapComplianceIDInputController.cs
            ├── TapTapComplianceQuickVerifyTipController.cs
            ├── TaptapComplianceHealthReminderController.cs
            └── TaptapComplianceHealthPaymentController.cs
```

---

## 三、平台分发与初始化

### 3.1 IInitTask 注册

Core 启动期通过反射枚举 `IInitTask`，按 `Order` 排序执行：

```csharp
// Unity/Assets/TapSDK/Compliance/Runtime/Internal/Init/ComplianceInitTask.cs:7
public class ComplianceInitTask : IInitTask
{
    public int Order => 12;

    public void Init(TapTapSdkOptions coreOption, TapTapSdkBaseOptions[] otherOptions)
    {
        if (coreOption == null) return;

        TapTapComplianceOption complianceOption = null;
        if (otherOptions != null && otherOptions.Length > 0)
        {
            foreach (var option in otherOptions)
            {
                if (option is TapTapComplianceOption option1) complianceOption = option1;
            }
        }
        if (complianceOption == null) complianceOption = new TapTapComplianceOption();
        ComplianceJobManager.Init(coreOption.clientId, coreOption.clientToken,
                                  coreOption.region, complianceOption);
    }
}
```

### 3.2 ComplianceJobManager —— 按平台选 Job

```csharp
// Runtime/Internal/ComplianceJobManager.cs:58
private static void InitJob()
{
    // 国内-移动端防沉迷用桥接的方式
    if (Application.platform == RuntimePlatform.Android
        || Application.platform == RuntimePlatform.IPhonePlayer)
    {
        _job = CreateJob(false);  // ComplianceMobileOldJob（Mobile 程序集）
    }
    // 其他均使用 Unity Native 的方式（PC、Editor、Standalone）
    else
    {
        _job = CreateJob(true);   // ComplianceNewJob（任何 TapSDK.Compliance 开头的程序集）
    }
    TapLog.Log(string.Format("Anti Addiction Job Type: {0} ! Platform: {1}",
                             _job.GetType(), Application.platform.ToString()));
}
```

`CreateJob(isNewJob)` 内部用 `AppDomain.CurrentDomain.GetAssemblies()` 反射查找：

* `isNewJob=true`：在所有 `TapSDK.Compliance*` 程序集里找类名包含 `ComplianceNewJob` 的实现。
* `isNewJob=false`：在 `TapSDK.Compliance.Mobile.Runtime` 程序集里找 `ComplianceMobileOldJob`。

> 这种"按类名匹配"的反射方式比较脆弱，下游若改名或拆包名会静默挂掉。

### 3.3 ComplianceNewJob.Init —— PC 入口

```csharp
// Standalone/Runtime/Internal/ComplianceNewJob.cs:96
public void Init(string clientId, string clientToken, TapTapRegionType reginType, TapTapComplianceOption config)
{
    UseAgeRange = config.useAgeRange;
    currentRegionType = reginType;
    TapTapComplianceManager.Init(clientId, clientToken, config);
    TapComplianceTracker.Instance.TrackInit();
}
```

`TapTapComplianceManager.Init`（`TapTapComplianceManager.cs:85`）做三件事：
1. 缓存 `config / ClientId`；
2. 把 `clientId/clientToken` 写到 `Network`，并把 `Accept-Language` 设为 `zh-CN`，把 host 锁死为 `https://tapsdk.tapapis.cn`；
3. 通过 `Resources.Load<TextAsset>(ComplianceLocalizationItems.PATH)` 预载本地化文案。

---

## 四、门面 API 速查

`Runtime/Public/TapTapCompliance.cs` 是业务方唯一入口，全部为静态转发：

| 方法 | 转发目标 | 备注 |
|---|---|---|
| `RegisterComplianceCallback(Action<int,string>)` | `ComplianceJobManager.Job.RegisterComplianceCallback` | **未初始化时只 Warn，不阻断**；底层去重（同一 callback 不会注册两次） |
| `Startup(userId)` | `Job.Startup` | 异步，结果通过已注册回调以 `StartUpResult.*` 码返回 |
| `Exit()` | `Job.Exit` | 触发 `EXITED(1000)` 回调，并清除本地实名缓存 |
| `GetAgeRange()` → `Task<int>` | `Job.GetAgeRange` | 未实名 / 未启用 `useAgeRange` → `-1`；其它返回 `AgeLimit` |
| `GetRemainingTime()` → `Task<int>` | `Job.GetRemainingTime` | 成年人固定 `9999`；未启动返回 `0` |
| `GetCurrentAccessToken()` → `Task<string>` | `Job.GetCurrentToken` | 实际返回的是 `anti_addiction_token`（即 v2 实名 token） |
| `CheckPaymentLimit(amount, ok, err)` | `Job.CheckPaymentLimit` | 单位：分；`status==1` 表示可支付 |
| `SubmitPayment(amount, ok, err)` | `Job.SubmitPayment` | 单位：分 |
| `SetTestEnvironment(bool)` `[Obsolete]` | `Job.SetTestEnvironment` | **已废弃**，但仍会弹"调试模式"红字 Tip（PC 实现） |
| `OnInvokeExternalCallback(code, msg)` | `Job.OnInvokeExternalCallback` | 内部使用，业务方不应主动调 |

> `GetAgeRange / GetRemainingTime / GetCurrentAccessToken` 虽然是 `Task<T>`，但 PC 实现里 **全部同步完成**（用 `TaskCompletionSource.TrySetResult` 立即返回），不会真正进入异步等待。

---

## 五、核心数据模型

### 5.1 `TapTapComplianceOption`（`Runtime/Public/TapTapComplianceOption.cs:8`）

```csharp
public class TapTapComplianceOption : TapTapSdkBaseOptions
{
    public bool showSwitchAccount = false; // 未成年人不可玩时是否提供"切号"按钮
    public bool useAgeRange = true;        // 是否申请 compliance scope（采集年龄段）
}
```

* `useAgeRange = true` → 走 `SCOPE_COMPLIANCE`（"compliance"），含年龄段；
* `useAgeRange = false` → 走 `SCOPE_COMPLIANCE_BASIC`（"compliance_basic"），不返回精确年龄段。
* Android 端额外提供运行时 setter（`setUseAgeRange`），**PC 端没有**，只能 Init 时一次性指定。

### 5.2 `VerificationResult / LocalVerification`（`Standalone/Runtime/Internal/Model/Verification.cs`）

```csharp
public class VerificationResult
{
    [JsonProperty("status")]               public string Status { get; private set; }
    [JsonProperty("anti_addiction_token")] internal string ComplianceToken { get; private set; }
    [JsonProperty("code")]                 public int errorCode { get; private set; }
    [JsonProperty("error")]                public string error { get; private set; }
    [JsonProperty("error_description")]    public string errorDescription { get; private set; }
    [JsonProperty("msg")]                  public string msg { get; private set; }

    internal bool IsVerified    => Status?.Equals(VERIFICATION_STATUS_SUCCESS) ?? false;
    internal bool IsVerifing    => Status?.Equals(VERIFICATION_STATUS_WAITING) ?? false;
    internal bool IsVerifyFailed=> Status?.Equals(VERIFICATION_STATUS_FAILED)  ?? false;
}

public class LocalVerification : VerificationResult
{
    internal string UserId { get; set; }
    public int AgeLimit;
    public bool IsAdult;

    internal bool CheckIsAdult => AgeLimit == AGE_LIMIT_ADULT || AgeLimit == UNKNOWN_AGE_ADULT;
}
```

年龄段常量（`Verification.cs:14`）：

| 常量 | 值 | 含义 |
|---|---|---|
| `AGE_LIMIT_UNKNOWN`     | -1  | 未实名 / 用户未授权 |
| `AGE_LIMIT_CHILD`       | 0   | 儿童 |
| `AGE_LIMIT_TEEN`        | 8   | 8~15 |
| `AGE_LIMIT_YOUNG`       | 16  | 16~17 |
| `AGE_LIMIT_ADULT`       | 18  | 成年 |
| `UNKNOWN_AGE`           | 100 | 已实名但未授权年龄段，且未成年 |
| `UNKNOWN_AGE_ADULT`     | 110 | 已实名但未授权年龄段，且成年 |

> "成年判定" 不只看 `== 18`，还要看是否 `== 110`（`CheckIsAdult`）。

### 5.3 `UserComplianceConfigResult`（`Model/UserComplianceConfig.cs:87`）

```csharp
public class UserComplianceConfigResult
{
    [JsonProperty("real_name")]            public UserState userState;       // age_limit / is_adult
    [JsonProperty("anti_addiction")]       public Policy policy;             // policy_active / policy_model / policy_heartbeat_interval
    [JsonProperty("content_rating_check")] public AgeCheckResult ageCheckResult; // allow
    [JsonProperty("local")]                public Local localConfig;         // time_range（time_start/time_end/holidays/text）
}
```

策略相关常量（`ComplianceConst.cs`）有 `POLICY_ACTIVE_TIME_RANGE` 等，决定离线兜底时是否启用 `IsGameTime` 检查。

### 5.4 `PlayableResult / PayableResult`

```csharp
// Model/Playable.cs
public class PlayableResult
{
    [JsonProperty("remain_time")]        public int    RemainTime { get; set; } // 单位：秒
    [JsonProperty("title")]              public string Title      { get; set; }
    [JsonProperty("description_plain")]  public string Content    { get; set; }
}

// Model/Payable.cs
public class PayableResult
{
    public bool   Status;   // false → 不可支付
    public string Title;
    public string Content;
}
```

### 5.5 `ComplianceException`（`Standalone/Runtime/Internal/ComplianceException.cs`）

```csharp
public class ComplianceException : TapException
{
    public string Error { get; internal set; }
    public string Description { get; internal set; }
    public long   Now { get; internal set; }   // 服务端时间，用于校正 timestamp
    public long   ErrorCode { get; internal set; }
    public string ErrorMessage { get; internal set; }

    public bool IsTokenExpired() =>
        Error != null && Error.Equals("business_code_error") && ErrorCode == 200000;
}
```

> 整个 PC 实现的"用户登出"闭环都靠 `IsTokenExpired()` 触发——服务端返回 `error=business_code_error & errorCode=200000` 时业务侧才会被回调 `EXITED(1000)`。

---

## 六、Startup 全流程拆解（重头戏）

### 6.1 时序图（文字版）

```
业务调用 TapTapCompliance.Startup(userId)
  └─ ComplianceNewJob.Startup
       ├─ CheckInitState              (Core + region 校验)
       ├─ isCheckingUser 互斥锁
       ├─ ClearUserCache              (如果上一个 userId 不同)
       ├─ TapComplianceTracker.TrackStart("startup", sessionId)
       │
       └─ TapTapComplianceManager.StartUp(userId)
            ├─ UIManager.OpenLoading()
            ├─ Config.Fetch(userId)             ← 拉全局 RealNameConfig（带本地缓存兜底）
            └─ Worker.StartUp(userId)
                 │
                 ├─ FetchVerificationAsync(userId)
                 │    └─ Verification.Fetch:
                 │         (1) 读本地 v2 缓存
                 │         (2) 有 TapToken & compliance scope → FetchVerificationByTapToken
                 │         (3) 否则 → Network.FetchVerification(userId)   ← clientId+userId
                 │
                 ├─ 分支 A：Current == null || IsVerifyFailed
                 │    InternalStartup → GetVerificationResult
                 │      a) 弹 QuickVerifyTip → 用户同意/拒绝
                 │      b) 同意：TapLogin.Authorize([compliance|compliance_basic])
                 │           → FetchByTapToken (TapToken 拿 v2 token)
                 │      c) 拒绝 / TapToken 拿不到：VerifyManuallyAsync
                 │           → OpenVerificationPanelCn → Network.FetchVerificationManual
                 │      d) 用户关窗 → REAL_NAME_STOP(9002)
                 │
                 ├─ 分支 B：IsVerifing
                 │    ShowVerifingTip （提示"认证中"，确认即 REAL_NAME_STOP）
                 │
                 └─ 分支 C：已实名 → OnVerificationFetched
                      └─ ValidateUserConfigAsync
                           ├─ CheckUserConfigAsyncWithFallback
                           │    Network.CheckUserConfig → 持久化 + 拿到 ageLimit / isAdult / policy
                           │    （5xx 时回退本地缓存 + ResetBaseLocalTime + GenerateSession）
                           ├─ Verification.setAgeState
                           ├─ ageCheckResult.allow == false → AGE_LIMIT(1100)
                           └─ ValidatePlayableAsync
                                CheckPlayableAsyncWithFallback (Network.CheckPlayable，异常走 CheckOfflinePlayable)
                                  成年人 → OnCheckedPlayableWithAdult → TryStartPoll → LOGIN_SUCCESS(500)
                                  未成年 → OnCheckedPlayableWithMinorAsync
                                            RemainTime > 0：弹提示 + 确认 → LOGIN_SUCCESS + 启动轮询
                                            RemainTime <= 0：弹提示，确认 = Application.Quit
                                                             (showSwitchAccount 时多一个"切号"按钮 → SWITCH_ACCOUNT)
                                                             返回 PERIOD_RESTRICT(1030)
↑ 最终任意 result code 回到 ComplianceNewJob.Startup，调 OnInvokeExternalCallback
```

### 6.2 `ComplianceNewJob.Startup` —— 互斥 + 打点

```csharp
// Standalone/Runtime/Internal/ComplianceNewJob.cs:115
public async void Startup(string userId)
{
    if (!CheckInitState()) return;
    if (string.IsNullOrEmpty(userId)) { TapLog.Error(" current user is invalid:" + userId); return; }
    if (isCheckingUser) { TapLog.Log(" current user is checking so return"); return; }

    isCheckingUser = true;
    string sessionId = Guid.NewGuid().ToString();
    TapComplianceTracker.Instance.TrackStart("startup", sessionId);

    if (TapTapComplianceManager.UserId != null)
        TapTapComplianceManager.ClearUserCache();  // 清掉上一个 user

    var code = await TapTapComplianceManager.StartUp(userId);

    switch (code)
    {
        case StartUpResult.LOGIN_SUCCESS:
        case StartUpResult.PERIOD_RESTRICT:
        case StartUpResult.DURATION_LIMIT:
        case StartUpResult.AGE_LIMIT:
            TapComplianceTracker.Instance.TrackSuccess("startup", sessionId); break;
        case StartUpResult.INVALID_CLIENT_OR_NETWORK_ERROR:
            TapComplianceTracker.Instance.TrackFailure("startup", sessionId, code,
                                                      "invalid client or network error"); break;
        case StartUpResult.EXITED:
        case StartUpResult.SWITCH_ACCOUNT:                                     break;
        case StartUpResult.REAL_NAME_STOP:
            TapComplianceTracker.Instance.TrackCancel("startup", sessionId);   break;
    }
    OnInvokeExternalCallback(code, null);
}
```

要点：
- **`isCheckingUser` 互斥**：startup 进行中再点会 silent return，避免双开。
- **`ClearUserCache`**：切 userId 时清掉内存里旧用户的 Playable / RemainSeconds，并 `Logout(false)`（不删本地文件）。
- 打点：startup 视为一次"会话"，全程一个 `sessionId`，最终分四种结局 success / failure / cancel / 静默。

### 6.3 `TapTapComplianceManager.StartUp` —— Loading + 拉全局配置 + 建 Worker

```csharp
// Standalone/Runtime/Internal/TapTapComplianceManager.cs:117
public static async Task<int> StartUp(string userId)
{
    if (string.IsNullOrEmpty(userId)) throw new ArgumentNullException(nameof(userId));

    UserId = userId;
    UIManager.Instance.OpenLoading();
    InitWorker();                                  // 反射拿 ComplianceWorker
    bool isSuccess = await UpdateConfig(userId);   // → Config.Fetch
    if (isSuccess)
        return await Worker.StartUp(UserId);
    else
    {
        UIManager.Instance.CloseLoading();
        return StartUpResult.INVALID_CLIENT_OR_NETWORK_ERROR;
    }
}
```

`InitWorker / GetChinaWorker` 用反射在 `TapSDK.Compliance.Standalone.Runtime` 程序集找 `BaseComplianceWorker` 的子类——目前只有 `ComplianceWorker`。

### 6.4 `BaseComplianceWorker.StartUp` —— 实名分支判定

```csharp
// Standalone/Runtime/Internal/Worker/BaseComplianceWorker.cs:350
public async Task<int> StartUp(string userId)
{
    await FetchVerificationAsync(userId);

    if (Verification.Current == null || Verification.IsVerifyFailed)
    {
        TapLog.Log("try get token internal by UI");
        var result = await InternalStartup(userId);  // 进入快速 / 手动认证 UI 流程
        if (result != 0) return result;              // REAL_NAME_STOP 等中断
    }
    else if (Verification.IsVerifing)
    {
        return await ShowVerifingTip();              // 认证中提示
    }
    else
    {
        UIManager.Instance.CloseLoading();           // 已认证，直接关 loading
    }
    return await OnVerificationFetched();            // → ValidateUserConfigAsync → ValidatePlayableAsync
}
```

### 6.5 `InternalStartup` / `GetVerificationResult` —— 快速 vs 手动认证

PC 端的 `ComplianceWorker.InternalStartup` 唯一职责就是包一层 try/catch，真正逻辑在 `GetVerificationResult`：

```csharp
// Standalone/Runtime/ComplianceWorker.cs:293
private async Task<int> GetVerificationResult(string userId)
{
    int mannualVerify = 0;
    TapTapAccount tapAccount = await TapTapLogin.Instance.GetCurrentTapAccount();
    AccessToken  accessToken = tapAccount?.accessToken;
    bool isTapUser = accessToken != null;

    try
    {
        accessToken = await GetAccessToken(accessToken);   // 弹快速认证提示 + TapLogin.Authorize
        if (accessToken != null)
        {
            if (HaveComplianceScope(accessToken))
            {
                var fetchResult = await FetchByTapToken(userId, accessToken);
                // 0-正常; 1-异常; -1-实名失败
                if (fetchResult != 0)
                {
                    if (fetchResult == 1)
                        UIManager.Instance.OpenToast("授权异常", UIManager.GeneralToastLevel.Error);
                    mannualVerify = 2;   // 保底走手动
                }
            }
            else
            {
                UIManager.Instance.OpenToast("授权错误", UIManager.GeneralToastLevel.Error);
                mannualVerify = 2;
            }
        }
        else mannualVerify = 2;
    }
    catch (TaskCanceledException)       { return StartUpResult.REAL_NAME_STOP; }
    catch (ComplianceException aae)
    {
        // TapToken 过期 → 重新走快速认证；其它一律保底手动
        if (aae.message.ToLower().Contains("refuse quick verify")) mannualVerify = 1;
        else mannualVerify = 2;
    }
    catch (Exception)                   { mannualVerify = 2; }

    if (mannualVerify != 0)
    {
        try   { await VerifyManuallyAsync(); }
        catch (TaskCanceledException) { return StartUpResult.REAL_NAME_STOP; }
    }
    return Verification.IsVerified ? 0 : await ShowVerifingTip();
}
```

`mannualVerify` 三态：
- `0`：完全不走手动；
- `1`：用户主动选了"我不要快速认证"；
- `2`：保底兜底（TapToken 拿不到 / 授权错误 / 异常）。

### 6.6 `GetAccessToken` —— 弹"快速认证提示"

```csharp
// Standalone/Runtime/ComplianceWorker.cs:389
private static async Task<AccessToken> InternalGetAccessToken(bool isTapUser, bool haveCompliance, AccessToken accessToken)
{
    bool useAgeRange  = TapTapComplianceManager.config.useAgeRange;
    string permission = useAgeRange ? SCOPE_COMPLIANCE : SCOPE_COMPLIANCE_BASIC;

    // 是 Tap 用户但 token 没 compliance scope，或不允许手动 → 只显示"确认"按钮
    bool justShowConfirmBtn = (isTapUser && !haveCompliance) || !Config.Current.useManual;
    var goQuickVerify = await ShowQuickVerifyTipWindow(justShowConfirmBtn);

    if (!goQuickVerify)
        throw new ComplianceException(-1, "refuse quick verify");  // 进入手动流程

    IAuthorizationProvider provider = PlatformTypeUtils
        .CreatePlatformImplementationObject(typeof(IAuthorizationProvider), "TapSDK.Login")
        as IAuthorizationProvider;
    return await provider.Authorize(new[] { permission });
}
```

要点：
- 不是 Tap 用户 / Tap token scope 不全 → 必须弹"快速认证 + 手动认证"两个按钮；
- `Config.Current.useManual` 由全局 RealNameConfig 控制是否允许手动。

### 6.7 `OnVerificationFetched` → `ValidateUserConfigAsync`

```csharp
// Worker/BaseComplianceWorker.cs:416
private async Task<int> ValidateUserConfigAsync()
{
    do {
        try {
            UserComplianceConfigResult result = await CheckUserConfigAsyncWithFallback();
            TapTapComplianceManager.CurrentUserAntiResult = result;
            await Verification.setAgeState(result.userState.ageLimit, result.userState.isAdult);

            if (!result.ageCheckResult.allow) return StartUpResult.AGE_LIMIT;      // 适龄拦截
            return await ValidatePlayableAsync();                                  // 进入时段判定
        }
        catch (Exception e) {
            if (e is ComplianceException aae && aae.IsTokenExpired()) {
                Logout(); return StartUpResult.EXITED;
            }
            return StartUpResult.INVALID_CLIENT_OR_NETWORK_ERROR;
        }
    } while (true);
}
```

注意：`do...while(true)` 的循环结构看起来是要重试，但所有路径都 `return`，**事实上只执行一次**——遗留代码。

### 6.8 `CheckUserConfigAsyncWithFallback` —— 服务端 + 本地双写

```csharp
// Worker/BaseComplianceWorker.cs:133
protected virtual async Task<UserComplianceConfigResult> CheckUserConfigAsyncWithFallback()
{
    string filename = Tool.EncryptString(Verification.Current.UserId);
    Persistence persistence = new Persistence(Path.Combine("user_anti_config", filename));
    UserComplianceConfigResult cached = await persistence.Load<UserComplianceConfigResult>();

    try {
        var serverResult = await Network.CheckUserConfig();
        if (serverResult != null) {
            await persistence.Save(serverResult);
            TapTapComplianceManager.CurrentSession = GenerateSession();   // 32 位随机串
        }
        return serverResult;
    }
    catch (ComplianceException e) {
        if (e.IsTokenExpired() || e.Code < 500) throw;   // 业务错 / 客户端错 → 上抛
        if (cached != null) {
            ResetBaseLocalTime();                        // 后续走离线时间
            TapTapComplianceManager.CurrentSession = GenerateSession();
            return cached;
        }
        throw;
    }
    catch (Exception) {
        if (cached == null) throw;
        ResetBaseLocalTime();
        TapTapComplianceManager.CurrentSession = GenerateSession();
        return cached;
    }
}
```

### 6.9 `ValidatePlayableAsync` —— 成年 vs 未成年分流

```csharp
// Worker/BaseComplianceWorker.cs:450
private async Task<int> ValidatePlayableAsync()
{
    int tryCount = 0;
    do {
        try {
            tryCount++;
            PlayableResult playable = await CheckPlayableAsyncWithFallback();
            TapTapComplianceManager.CurrentPlayableResult = playable;

            if (Verification.Current.CheckIsAdult) {
                OnCheckedPlayableWithAdult(playable);     // → TryStartPoll
                return StartUpResult.LOGIN_SUCCESS;
            }
            return await OnCheckedPlayableWithMinorAsync(playable);
        }
        catch (Exception e) {
            if (e is ComplianceException aae && aae.IsTokenExpired()) {
                Logout(); return StartUpResult.EXITED;
            }
            return StartUpResult.INVALID_CLIENT_OR_NETWORK_ERROR;
        }
    } while (true);
}
```

未成年人分流详见 `OnCheckedPlayableWithMinorAsync`（`Worker/BaseComplianceWorker.cs:70`）：

```csharp
protected virtual async Task<int> OnCheckedPlayableWithMinorAsync(PlayableResult playable)
{
    if (playable.RemainTime > 0) TapTapComplianceManager.ShowAntiAddictionTip();

    var tcs = new TaskCompletionSource<int>();
    Action onOk;
    if (playable.RemainTime > 0) {
        onOk = () => { tcs.TrySetResult(StartUpResult.LOGIN_SUCCESS); TryStartPoll(); };
    } else {
        tcs.TrySetResult(StartUpResult.PERIOD_RESTRICT);
        onOk = () => Application.Quit();                  // 不可玩，确认即退出
    }

    Action onSwitchAccount = null;
    if (config.showSwitchAccount && playable.RemainTime <= 0) {
        onSwitchAccount = () => {
            Logout(false);
            TapTapCompliance.OnInvokeExternalCallback(StartUpResult.SWITCH_ACCOUNT, null);
        };
    }
    TapComplianceUI.OpenHealthReminderPanel(playable, onOk, onSwitchAccount);
    return await tcs.Task;
}
```

### 6.10 `OnInvokeExternalCallback` —— 终态分发

```csharp
// Standalone/Runtime/Internal/ComplianceNewJob.cs:222
public void OnInvokeExternalCallback(int code, string msg)
{
    // 1) 更新 CanPlay
    switch (code) {
        case StartUpResult.LOGIN_SUCCESS:           TapTapComplianceManager.CanPlay = true;  break;
        case StartUpResult.AGE_LIMIT:
        case StartUpResult.PERIOD_RESTRICT:
        case StartUpResult.DURATION_LIMIT:
        case StartUpResult.EXITED:
        case StartUpResult.INVALID_CLIENT_OR_NETWORK_ERROR:
        case StartUpResult.SWITCH_ACCOUNT:          TapTapComplianceManager.CanPlay = false; break;
    }

    // 2) 终态码解锁 isCheckingUser
    if (code == LOGIN_SUCCESS || code == REAL_NAME_STOP || code == EXITED
        || code == AGE_LIMIT || code == SWITCH_ACCOUNT
        || code == INVALID_CLIENT_OR_NETWORK_ERROR) isCheckingUser = false;

    // 3) 用户身份变动 → 写 openlog 上下文
    if (code == LOGIN_SUCCESS || code == EXITED || code == SWITCH_ACCOUNT) {
        var userData = new Dictionary<string, string> {
            ["anti_addict_user_identifier"] = (code == LOGIN_SUCCESS) ? TapTapComplianceManager.UserId    ?? "" : "",
            ["anti_addict_session_id"]      = (code == LOGIN_SUCCESS) ? TapTapComplianceManager.CurrentSession ?? "" : ""
        };
        EventManager.TriggerEvent(EventManager.OnComplianceUserChanged, JsonConvert.SerializeObject(userData));
    }

    // 4) 调业务回调
    if (StartUpResult.Contains(code) && _externalCallbackList != null)
        foreach (var cb in _externalCallbackList) cb?.Invoke(code, msg);

    // 5) 成功首次弹 Toast
    if (code == LOGIN_SUCCESS) TapTapComplianceManager.ShowAntiAddictionTip();
}
```

---

## 七、Verification 子系统（实名）

文件：`Standalone/Runtime/Internal/Verification.cs` + `Standalone/Runtime/Internal/Model/Verification.cs`。

### 7.1 双层缓存模型

- 内存：`static LocalVerification current`
- 磁盘：`Persistence` 写到 `verification_v2/<EncryptString(userId)>`（旧版 v1 文件名为 `verification/`，代码已基本不再使用）。

### 7.2 `Verification.Fetch` —— 优先级链

```csharp
// Standalone/Runtime/Internal/Verification.cs:44
public static async Task Fetch(string userId)
{
    if (current != null && !current.UserId.Equals(userId)) Logout(false);

    persistenceV2 ??= new Persistence(Path.Combine("verification_v2", Tool.EncryptString(userId)));

    // ① 本地 v2 缓存
    LocalVerification local = await persistenceV2.Load<LocalVerification>();
    if (local?.ComplianceToken?.Length > 0) { current = local; return; }

    // ② 当前 TapAccount 有 compliance scope → 用 TapToken 拉
    var accessToken = (await TapTapLogin.Instance.GetCurrentTapAccount())?.accessToken;
    if (HasComplianceAuthInTapToken(accessToken)) { await FetchByTapToken(userId); return; }

    // ③ 兜底：用 clientId + userId 直接拉
    var result = await Network.FetchVerification(userId);
    await Save(userId, result);
}
```

> 注意 `Fetch` 一旦抛异常会被 `BaseComplianceWorker.FetchVerificationAsync` 吞掉（仅打日志），所以最终走向由 `Verification.Current` 状态决定，而不是异常。

### 7.3 `FetchVerificationByTapToken` —— MAC 签名

```csharp
// Standalone/Runtime/Internal/Network.cs:96
public static async Task<VerificationResult> FetchVerificationByTapToken(string userId, AccessToken token, long timestamp = 0)
{
    string path = $"real-name/v1/anti-addiction-token-taptap?client_id={gameId}&user_identifier={WebUtility.UrlEncode(userId)}";
    var uri = new Uri(host + "/" + path);
    var sign = GetMacToken(token, uri, timestamp);          // MAC kid+macKey+macAlgorithm
    var headers = GetAuthHeaders();
    headers.Add("Authorization", sign);
    ...
}
```

时间戳异常（`ComplianceConst.SERVER_ERROR_TYPE_INVALID_TIME`）会用服务端返回的 `Now` 重试一次：

```csharp
// Verification.cs:118
if (e is ComplianceException aee
    && aee.Error?.Equals(ComplianceConst.SERVER_ERROR_TYPE_INVALID_TIME) == true
    && retryTimes > 0)
{
    timestamp = aee.Now;
    retryTimes--;
    continue;
}
```

### 7.4 `setAgeState` —— 年龄段补全

服务端拿到 ageLimit/isAdult 后写回本地缓存：

```csharp
// Verification.cs:232
public static async Task setAgeState(int ageLimit, bool isAdult)
{
    current.AgeLimit = ageLimit < 0
        ? (isAdult ? UNKNOWN_AGE_ADULT : UNKNOWN_AGE)   // 已实名但未授权年龄段
        : ageLimit;
    current.IsAdult = isAdult;
    await persistenceV2.Save(current);
}
```

---

## 八、UserConfig & Playable 子系统（策略 + 时段）

### 8.1 全局配置 `Config.Fetch`

```csharp
// Standalone/Runtime/Internal/Config.cs:38
internal static async Task<bool> Fetch(string userId)
{
    while (true) {
        // ① 服务端
        try {
            Current = await Network.FetchConfig(userId);
            if (IsValid()) { await persistence.Value.Save(Current); return true; }
        } catch (Exception e) {
            if (e is ComplianceException aee && aee.code < 500) return false;  // 4xx 直接失败
        }
        // ② 本地缓存
        try {
            Current = await persistence.Value.Load<RealNameConfigResult>();
            if (Current != null) return true;
        } catch { }
        return false;     // 既没服务端也没缓存
    }
}
```

### 8.2 离线时段判定 `IsGameTime`

```csharp
// Standalone/Runtime/ComplianceWorker.cs:223
private bool IsGameTime()
{
    DateTimeOffset now = DateTimeOffset.FromUnixTimeSeconds(GetCurrentTime())
                                       .ToOffset(DateTimeOffset.Now.Offset);
    string currentDate = now.ToString("yyyy-MM-dd");
    var holidays = TapTapComplianceManager.CurrentUserAntiResult.localConfig.timeRangeConfig.holidays;

    if (holidays.Contains(currentDate))
    {
        DateTimeOffset start = Config.StrictStartTime;
        DateTimeOffset end   = Config.StrictEndTime;
        return now.TimeOfDay >= start.TimeOfDay && now.TimeOfDay < end.TimeOfDay;
    }
    return false;            // 非节假日一律不可玩
}

private long GetCurrentTime()
{
    long serverTime = TapHttpTime.GetCurrentServerTime();
    return serverTime > 0 ? serverTime : GetCurrentLocalTime();
}
```

`GetCurrentLocalTime` 由 `BaseComplianceWorker` 维护：

```csharp
// Worker/BaseComplianceWorker.cs:178
private void ResetBaseLocalTime() {
    if (BaseLocalTime > 0) return;
    BaseLocalTime        = DateTimeOffset.Now.ToUnixTimeSeconds();
    BaseLocalStartUpTime = (long)Time.realtimeSinceStartup;
}
protected long GetCurrentLocalTime() {
    return BaseLocalTime + (long)Time.realtimeSinceStartup - BaseLocalStartUpTime;
}
```

> 这样做的目的：失联后不能信用户系统时间，于是把"首次离线时刻的本地时间"作为基准，后续按 `realtimeSinceStartup` 推算。

### 8.3 `CheckPlayableAsyncWithFallback` —— 降级策略

```csharp
// Worker/BaseComplianceWorker.cs:106
protected virtual async Task<PlayableResult> CheckPlayableAsyncWithFallback()
{
    try { return await CheckPlayableAsync(); }
    catch (ComplianceException e) {
        if (e.IsTokenExpired() || e.Code < 500
            || !TapTapComplianceManager.CurrentUserAntiResult.policy.active.Equals(POLICY_ACTIVE_TIME_RANGE))
            throw;                                       // 不允许降级
        return CheckOfflinePlayable();
    }
    catch (Exception)  { return CheckOfflinePlayable(); }
}
```

成年人离线时直接返回 `RemainTime=9999`，未成年人走 `CheckOfflineMinorPlayable`：

```csharp
// Standalone/Runtime/ComplianceWorker.cs:100
protected override PlayableResult CheckOfflineMinorPlayable()
{
    if (IsGameTime()) {
        var tip = Config.GetMinorPlayableHealthReminderTip();
        DateTimeOffset gameEnd = Config.StrictEndTime;
        DateTimeOffset now     = DateTimeOffset.FromUnixTimeSeconds(GetCurrentTime())
                                               .ToOffset(DateTimeOffset.Now.Offset);
        TimeSpan remain = gameEnd.TimeOfDay - now.TimeOfDay;
        return new PlayableResult { Title=tip?.tipTitle, Content=tip?.tipDescription,
                                    RemainTime = (int)remain.TotalSeconds };
    }
    var rejectTip = Config.GetMinorUnplayableHealthReminderTip();
    return new PlayableResult { Title=rejectTip?.tipTitle, Content=rejectTip?.tipDescription, RemainTime = 0 };
}
```

---

## 九、心跳轮询与剩余时长（CompliancePoll）

文件：`Standalone/Runtime/Internal/CompliancePoll.cs`，基于 `MonoBehaviour` + Coroutine + `DontDestroyOnLoad`。

### 9.1 启动条件

`BaseComplianceWorker.IsNeedStartPoll()`：

```csharp
protected virtual bool IsNeedStartPoll()
    => !Verification.IsAdult || Config.NeedUploadUserAction;
```

`Config.NeedUploadUserAction` 在 PC 实现里恒为 `true`，因此 **PC 端不论成年与否都会启动心跳**。

### 9.2 双 timer：心跳 + 1 秒倒计时

```csharp
// Standalone/Runtime/Internal/CompliancePoll.cs:79
private void Update()
{
    if (_elpased != null) {
        _elpased += Time.unscaledDeltaTime;
        if (_elpased >= 1) {
            _elpased = 0;
            if (TapTapComplianceManager.CurrentRemainSeconds != null)
                TapTapComplianceManager.CurrentRemainSeconds--;     // 本地倒计时
        }
    }
}

IEnumerator Poll()
{
    while (true) {
        var task = TapTapComplianceManager.CheckPlayableOnPolling(); // 心跳
        yield return new WaitUntil(() => task.IsCompleted);
        if (task.Result.RemainTime <= 0) { _elpased = null; break; }
        if (task.Result.RemainTime < pollInterval)
            pollInterval = task.Result.RemainTime;                   // 压缩间隔
        if (_elpased == null) _elpased = 0;
        yield return new WaitForSeconds(pollInterval);
    }
}
```

* 默认 `pollInterval = 120s`，首启被服务端 `policy.heartbeatInterval` 覆盖。
* 心跳返回 `RemainTime <= 0` 时 `OnUnplayablePostPoll` 弹宵禁面板并回调 `PERIOD_RESTRICT`：

```csharp
// Worker/BaseComplianceWorker.cs:299
protected virtual void OnUnplayablePostPoll(PlayableResult playable)
{
    Action onExitGame = Application.Quit;
    Action onSwitch = null;
    if (config.showSwitchAccount && playable.RemainTime <= 0)
        onSwitch = () => {
            Logout(false);
            TapTapCompliance.OnInvokeExternalCallback(StartUpResult.SWITCH_ACCOUNT, null);
        };
    TapTapCompliance.OnInvokeExternalCallback(StartUpResult.PERIOD_RESTRICT, null);
    TapComplianceUI.OpenHealthReminderPanel(playable, onExitGame, onSwitch);
}
```

### 9.3 切后台暂停 / 恢复

```csharp
// CompliancePoll.cs:119
void OnApplicationPause(bool pauseStatus)
{
    if (pauseStatus) TapTapComplianceManager.LeaveGame();
    else             TapTapComplianceManager.EnterGame();
}

// TapTapComplianceManager.cs:191
public static void EnterGame()
{
    if (CanPlay && needResumePoll) { CompliancePoll.StartUp(); needResumePoll = false; }
}
public static void LeaveGame()
{
    if (CanPlay && CompliancePoll.StartPoll) { CompliancePoll.Logout(); needResumePoll = true; }
}
```

> Unity Standalone 桌面端 `OnApplicationPause` 触发并不稳定，需要结合 `OnApplicationFocus` 或 `Application.runInBackground` 评估。

---

## 十、UI 体系

`Standalone/Runtime/Internal/TapComplianceUI.cs` 是工厂入口，所有面板从 `ComplianceConst.GetPrefabPath(name, isMobile)` 取 Prefab，由通用 `UIManager.OpenUI<T>()` 实例化。

| Controller | Prefab 常量 | 用途 |
|---|---|---|
| `TapTapComplianceQuickVerifyTipController` | `QUICK_VERIFY_TIP_PANEL_NAME` | 快速实名提示（确认 / 取消） |
| `TaptapComplianceIDInputController`        | `ID_NUMBER_INPUT_PANEL_NAME`  | 手动实名（姓名 + 身份证） |
| `TaptapComplianceHealthReminderController` | `HEALTH_REMINDER_PANEL_NAME`  | 健康提醒（时段 / 时长 / 宵禁） |
| `TaptapComplianceHealthPaymentController`  | `HEALTH_PAYMENT_PANEL_NAME`   | 健康充值提醒 / 通用提示弹窗 |

`IsUseMobileUI()` PC 端固定返回 `false`，因此始终走桌面端 Prefab。

---

## 十一、支付链路

```csharp
// Standalone/Runtime/Internal/ComplianceNewJob.cs:170
public async void CheckPaymentLimit(long amount, Action<CheckPayResult> ok, Action<string> err)
{
    if (!CheckInitState()) return;
    try {
        var payResult = await TapTapComplianceManager.CheckPayLimit(amount);
        ok?.Invoke(new CheckPayResult {
            status      = payResult.Status ? 1 : 0,   // 1 可支付，0 不可
            title       = payResult.Title,
            description = payResult.Content
        });
    } catch (Exception e) {
        err?.Invoke(e.Message);
        if (e is ComplianceException aee && aee.IsTokenExpired()) Exit();
    }
}
```

底层：`Worker.CheckPayableAsync` → `Network.CheckPayable` → 若不可支付且服务端返回了文案，弹 `HealthPaymentPanel`。`SubmitPayment` 类似，只调 `Network.SubmitPayment`。

---

## 十二、Exit / Token 失效闭环

- **业务主动调** `TapTapCompliance.Exit()`：
  ```csharp
  // ComplianceNewJob.cs:160
  public void Exit() {
      if (!CheckInitState()) return;
      TapTapComplianceManager.Logout();          // 清缓存 + Worker.Logout + 停轮询
      OnInvokeExternalCallback(StartUpResult.EXITED, null);
  }
  ```
- **服务器 Token 过期**：散落在 Worker 各个 try/catch 里，统一通过 `ComplianceException.IsTokenExpired()` 触发：
  ```csharp
  if (e is ComplianceException aae && aae.IsTokenExpired()) {
      Logout();
      TapTapCompliance.OnInvokeExternalCallback(StartUpResult.EXITED, null);
  }
  ```

`Logout(needClearCache=true)` 会：

```csharp
// Worker/BaseComplianceWorker.cs:331
public virtual void Logout(bool needClearCache = true)
{
    if (needClearCache) {
        string filename = Tool.EncryptString(Verification.Current.UserId);
        new Persistence(Path.Combine("user_anti_config", filename)).Delete();
    }
    Verification.Logout(needClearCache);
    CompliancePoll.Logout();
    TapTapComplianceManager.UserId = null;
}
```

---

## 十三、回调码 StartUpResult

```csharp
// Runtime/Internal/Model/StartUpResult.cs
public const int LOGIN_SUCCESS                   = 500;   // 通过校验，可玩
public const int EXITED                          = 1000;  // 用户登出 / token 过期
public const int SWITCH_ACCOUNT                  = 1001;  // 触发切号
public const int PERIOD_RESTRICT                 = 1030;  // 宵禁 / 时段限制
public const int DURATION_LIMIT                  = 1050;  // 时长限制（保留，PC 当前未主动产出）
public const int AGE_LIMIT                       = 1100;  // 适龄拦截
public const int INVALID_CLIENT_OR_NETWORK_ERROR = 1200;  // 配置错 / 网络错
public const int REAL_NAME_STOP                  = 9002;  // 实名过程中关闭弹窗
```

`StartUpResult.Contains(code)` 决定哪些码会下发到业务回调列表。

---

## 十四、网络层（Network）

### 14.1 Host 与签名

```csharp
// Standalone/Runtime/Internal/Network.cs:17
static readonly string ChinaHost = "https://tapsdk.tapapis.cn";
private static ComplianceHttpClient HttpClient = new ComplianceHttpClient(ChinaHost);
```

- 仅中国大陆。Init 时通过反射给 `serverUrl` 私有字段赋值（`InitSetting`）。
- `Accept-Language` 固定 `zh-CN`。
- 所有请求自动注入 `X-Tap-Anti-Addiction-Token`（v2 token，若有）。

### 14.2 5 类端点

| 用途 | Method + Path | 调用方 |
|---|---|---|
| 拉全局合规配置（文案、是否允许手动等） | `GET /real-name/v1/get-global-config` | `Config.Fetch` |
| clientId + userId 拉 v2 token            | `GET /real-name/v1/anti-addiction-token` | `Verification.Fetch` 兜底分支 |
| TapToken（MAC 签名）拉 v2 token          | `GET /real-name/v1/anti-addiction-token-taptap` | `FetchByTapToken` |
| 手动提交身份证拉 v2 token                | `POST /real-name/v1/anti-addiction-token-manual` | `VerifyManuallyAsync` |
| v1 升 v2（兼容路径）                     | `POST /real-name/v1/anti-addiction-token-upgrade` | `Network.UpgradeToken`（当前未被调用） |
| 拉用户合规配置                            | `GET /anti-addiction/v1/get-config-by-token` (`platform=pc`) | `CheckUserConfig` |
| 心跳 / 可玩性判定                          | `POST /anti-addiction/v1/heartbeat`             | `CheckPlayable` |
| 支付检查                                  | `GET /anti-addiction/v1/payable`                | `CheckPayable` |
| 支付上报                                  | `POST /anti-addiction/v1/payment-submit`        | `SubmitPayment` |

### 14.3 MAC 签名实现

```csharp
// Network.cs:116
private static string GetMacToken(AccessToken token, Uri uri, long timestamp = 0)
{
    int ts = (int)timestamp;
    if (ts == 0) ts = (int)(DateTime.UtcNow - new DateTime(1970, 1, 1)).TotalSeconds;
    return "MAC " + LoginService.GetAuthorizationHeader(
        token.kid, token.macKey, token.macAlgorithm,
        "GET", uri.PathAndQuery, uri.Host, "443", ts);
}
```

签名串由 Login 模块 `LoginService.GetAuthorizationHeader` 生成；如服务端返回 `SERVER_ERROR_TYPE_INVALID_TIME`，会用响应里的 `Now` 字段再签一次。

---

## 十五、已知坑与注意点

按"影响优先级"排列，迁移时建议逐条对照：

1. **`useAgeRange` 仅 Init 时可设**。PC 没有运行时 setter（Android 有 `setUseAgeRange`），切 scope 必须重启 Init 流程。
2. **`SetTestEnvironment` 已废弃**：`Network.SetTestEnvironment` 是空实现，但 `TapTapComplianceManager.SetTestEnvironment` 仍会弹"调试模式"红字 Tip，**调用即"伪生效"**，容易让 QA 误以为切换了环境。
3. **平台分发依赖类名反射**：`ComplianceJobManager.CreateJob` 在 `TapSDK.Compliance*` 程序集里找类名包含 `ComplianceNewJob` / `ComplianceMobileOldJob` 的类。改名 / 拆包名会静默失效。
4. **`do…while(true)` 实为 `if`**：`ValidateUserConfigAsync` / `ValidatePlayableAsync` 内部所有分支都 `return`，循环结构是历史遗留。
5. **`GetVerificationResult` 同步取结果**：多处用 `tcs.Task.Result` 而不是 `await`；当前 task 已先 `TrySetResult` 不死锁，但风格危险，迁移时建议改 `await`。
6. **`FetchByTapToken` 把 5xx 与业务错压成同一路径**：`ComplianceWorker.cs:202` 一律返回 `1`（保底走手动），与上方注释"网络错误打断流程"不符。迁移时需要重新设计错误分级。
7. **`IsGameTime` 默认非节假日不可玩**：失联 + `policy_active=time_range` + 非节假日 → 未成年人离线一律 `RemainTime=0`。这是策略所致，但需要明确告知运营。
8. **`OnApplicationPause` 在桌面端不稳定**：Standalone 失焦未必触发，`needResumePoll` 等于"标记摆设"。如果心跳要在最小化时挂起，迁移时需要换更可靠的事件源。
9. **`GetRemainingTime` 默认值不一致**：PC 未启动返回 `0`，Android 返回 `-1`；桥接层透传时业务侧拿到的语义会变。
10. **Token 失效闭环散落**：每个外部调用都自己 try `IsTokenExpired()`，没有集中拦截，迁移时建议在 HTTP 层统一处理。
11. **`Verification.Fetch` 异常吞噬**：失败时只 `TapLog.Error`，上层只看 `Verification.Current` 状态决定流程，调用者无法区分"无网络"和"业务错"。
12. **`CompliancePoll` 是 `MonoBehaviour`**：依赖 Unity 主循环 + GameObject 生命周期。UE5 没有对等机制，需要换成 `FTSTicker` / `FTickableGameObject` / `TimerManager`。
13. **`Application.Quit` 直接关进程**：未成年人不可玩时确认即退出，没有给业务方"我自己处理退出"的钩子。UE5 迁移建议改成事件/回调由业务方决定。
14. **本地化资源用 `Resources.Load`**：`ComplianceLocalizationItems.PATH` 走 Unity Resources 系统，UE5 需要换成 DataAsset / Localization 表。
15. **`Config.Fetch` 的 `while(true)`**：所有路径都 `return`，外层 `while(true)` 同样是死代码。

---

## 附：典型调用时序（成年人 + 已有缓存）

```
业务: TapTapCompliance.Startup("uid_001")
SDK:  ComplianceNewJob.Startup -> StartUp(uid_001)
       -> Config.Fetch          (服务端 200, 写本地)
       -> Worker.StartUp
            -> Verification.Fetch
                 → 命中本地 v2 缓存 (LocalVerification 含 ComplianceToken + IsAdult=true)
            -> ValidateUserConfigAsync
                 → Network.CheckUserConfig (200, 写本地, 生成 32 位 session)
                 → Verification.setAgeState(18, true)
                 → ageCheckResult.allow=true
            -> ValidatePlayableAsync
                 → Network.CheckPlayable (200, RemainTime=9999)
                 → Verification.CheckIsAdult=true
                 → OnCheckedPlayableWithAdult
                      → TryStartPoll (按 policy.heartbeatInterval 启动协程)
                 → return LOGIN_SUCCESS(500)
SDK:  OnInvokeExternalCallback(500, null)
       -> CanPlay = true
       -> isCheckingUser = false
       -> 触发 EventManager.OnComplianceUserChanged
       -> 回调业务侧 Action<int,string>(500, null)
       -> ShowAntiAddictionTip (首次弹底部 Toast)
```

---

> 文档维护提示：当 Unity 源码发生变化时，请同步更新本文档的"源码引用行号"。可用 `grep -rn "<关键字>" Unity/Assets/TapSDK/Compliance` 定位。
