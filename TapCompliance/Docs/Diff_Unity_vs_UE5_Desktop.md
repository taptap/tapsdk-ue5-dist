# UE5 Desktop vs Unity PC 合规实现 Diff 报告

> **目的**：把 UE5 `TapCompliance/Private/Desktop/` 与 Unity `Assets/TapSDK/Compliance/Standalone/` 的实现做完整对照，标注：
> - 🔴 **严重 Bug**（一致性问题，影响业务方功能）
> - 🟡 **行为差异**（需要产品/技术决策）
> - 🟢 **UE5 优于 Unity 的设计**（保留）
> - 🟢 **细节验证一致**（不用动）
>
> **配套阅读**：[Unity_PC_Compliance_Flow.md](./Unity_PC_Compliance_Flow.md) 是 Unity 端流程基线，引用源码格式 `文件:行号`。
>
> **状态说明**：本文档记录历史差异及当前状态。Bug 1/2 已修复；Bug 3 的修复后来被明确撤回。详见各节和末尾「修复记录」。

---

## 目录

1. [整体结构对照](#一整体结构对照)
2. [🔴 严重问题（已修复）](#二严重问题已修复)
3. [🟡 行为差异（待决策）](#三行为差异待决策)
4. [🟢 UE5 优于 Unity](#四ue5-优于-unity)
5. [🟢 边界细节一致](#五边界细节一致)
6. [其他次要观察](#六其他次要观察)
7. [修复记录](#七修复记录)
8. [回归测试覆盖](#八回归测试覆盖)

---

## 一、整体结构对照

| 子系统 | Unity | UE5 Desktop | 评价 |
|---|---|---|---|
| 门面 | `TapTapCompliance`（静态类） | `FTapCompliance` → `FTapCompliancePlatformDesktop`（接口实现） | ✓ 加了 Platform 抽象层，更适合 UE5 多平台 |
| 平台分发 | `ComplianceJobManager`（反射查类名） | `FTapCompliancePlatformFactory`（工厂模式） | ✓ 工厂比反射稳 |
| 全局状态 | `TapTapComplianceManager`（静态字段散落） | `FComplianceState`（单例 + `FCriticalSection`） | ✓ UE5 加锁，多线程安全 |
| 编排层 | `BaseComplianceWorker.StartUp` | `FComplianceAuthFlow::StartUp` | ✓ 三步流程对齐 |
| 实名缓存 | `Verification`（双层 v2 文件） | `FComplianceState.Verification` + `FCompliancePersistence` | ✓ UE5 用机器绑定 KV |
| 心跳 | `CompliancePoll`（`MonoBehaviour`） | `FCompliancePoll`（`FTSTicker`） | ✓ 桌面端 FTSTicker 更可靠 |
| Network | `Network`（5 端点） | `FComplianceNetwork`（同 5 端点） | ✓ 路径/参数完全对齐 |
| UI | 4 个 Controller | 8 个业务 WBP + 纯 Slate Loading/Toast + `FTapCompliancePanelManager` | ✓ UE5 按业务面板拆分，通用提示不依赖 WBP |

调用链对齐：

```
Unity:
  TapTapCompliance.Startup
   └─ ComplianceJobManager.Job (ComplianceNewJob).Startup
      └─ TapTapComplianceManager.StartUp
         └─ Worker (ComplianceWorker).StartUp
            ├─ FetchVerificationAsync → Verification.Fetch
            ├─ InternalStartup / VerifyManuallyAsync
            └─ OnVerificationFetched → ValidateUserConfigAsync → ValidatePlayableAsync
         → OnInvokeExternalCallback(code)

UE5:
  FTapCompliance::Startup
   └─ FTapCompliancePlatformDesktop::Startup (区域门控)
      └─ FDesktopComplianceManager::StartUp (互斥 + 清上一用户)
         └─ FComplianceAuthFlow::StartUp
            ├─ Step1_FetchVerification
            ├─ ShowFastAuthThenManual / ShowRealNameManual / OnRealNameSubmit
            ├─ Step2_ValidateUserConfig
            └─ Step3_ValidatePlayable
         → Finish(code) → DispatchCallback
```

---

## 二、🔴 严重问题（已修复）

> 注：Bug 3「Startup 重入互斥」曾在历史提交中修复，但随后由 `cb2f4eab54 revert(TapCompliance): 撤回 Desktop Startup 防重入互斥保护` 撤回；当前 UE5 实现不包含 `bIsStarting` 守卫，下面对应内容仅保留为历史记录。

### Bug 1 · `Exit()` 既不清缓存又不派发 EXITED 回调 【已修复】

#### 问题

**Unity**（`ComplianceNewJob.cs:160-167`）：

```csharp
public void Exit()
{
    if (!CheckInitState()) return;
    TapTapComplianceManager.Logout();   // → Worker.Logout(true) 删持久缓存
    OnInvokeExternalCallback(StartUpResult.EXITED, null);  // 通知业务
}
```

**UE5 修复前**（`TapCompliancePlatformDesktop.cpp:64-68`）：

```cpp
void FTapCompliancePlatformDesktop::Exit()
{
    UE_LOG(LogTapCompliance, Log, TEXT("[Desktop] Exit"));
    FDesktopComplianceManager::Get().Logout(/*bClearCache=*/false);
    // ← 缺 DispatchCallback(EXITED) ！
}
```

`FDesktopComplianceManager::Logout` 内部也只清 Poll/State/Panel，**没有派发回调**。

#### 后果

1. **业务方监听 EXITED(1000) 的回调永远不触发**，游戏 UI 卡在"已登录"状态。
2. `bClearCache=false` 导致下次同账号 Startup 仍读旧 verification + UserConfig 缓存——Unity 是清掉的（强制走完整流程，避免拿到陈旧策略）。

#### 修复

```cpp
void FTapCompliancePlatformDesktop::Exit()
{
    UE_LOG(LogTapCompliance, Log, TEXT("[Desktop] Exit"));
    // 对齐 Unity ComplianceNewJob.cs:160-167 Exit():
    //   1) Logout(true) — 清掉本地 verification + user_anti_config 持久缓存,
    //      下次同账号 Startup 必须重新拉取实名与策略, 避免拿到陈旧数据.
    //   2) DispatchCallback(EXITED) — 主动通知业务方合规会话已结束,
    //      否则游戏侧 UI 没法从"已通过"状态退出, 也拿不到 isCheckingUser 复位时机
    //      (DispatchCallback 内部走 ClearStartupGuardIfTerminal 解锁守卫).
    FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
    FDesktopComplianceManager::Get().DispatchCallback(TapComplianceConst::EXITED, FString());
}
```

#### 回归测试

`TapCompliance.Manager.Pure.ExitDispatchesEXITED`：
- 注册一个 callback
- 调 `Platform.Exit()`
- 断言：callback 被调一次、code = `EXITED(1000)`、startup 守卫被解锁

---

### Bug 2 · 未成年人首次通过的 Toast 文案错误 【已修复】

#### 问题

**Unity**（`TapTapComplianceManager.cs:230`）：

```csharp
TapMessage.ShowMessage("已通过防沉迷校验，祝您游戏愉快！", ...);
```

**UE5 修复前**（`ComplianceAuthFlow.cpp:755`）：

```cpp
if (bAllowEnter && !FCompliancePersistence::HasShownFirstPassTip(UserId))
{
    if (GEngine && GEngine->GameViewport)
    {
        UTapComplianceToast::ShowToast(
            GEngine->GameViewport->GetWorld(),
            TEXT("提交成功"));   // ← 文案错位
    }
    FCompliancePersistence::MarkFirstPassTipShown(UserId);
}
```

文案复用了 `OnRealNameSubmit` 成功路径（同文件 :573）的"提交成功"toast，语义完全错位。

#### 修复

把字符串改为 `TEXT("已通过防沉迷校验，祝您游戏愉快！")`，并加注释说明。

#### 回归测试

文案变更，逻辑路径不变。已被现有 `TapCompliance.AuthFlow.Pure.*` 间接覆盖（不弹 toast 就不会触发该分支）。

---

### Bug 3 · 缺少 `Startup` 重入互斥保护 【曾修复，后续已撤回】

#### 问题

**Unity**（`ComplianceNewJob.cs:126`）：

```csharp
if (isCheckingUser) { TapLog.Log(...); return; }
isCheckingUser = true;
// ... 在 OnInvokeExternalCallback 终态码时复位
```

**UE5 修复前**：`FDesktopComplianceManager::StartUp` 完全没有锁，每次都先 `Logout(false)` 再开新 flow。

#### 后果

业务方误连点两次（UI 卡顿或网络慢时极易发生）→ 两条并发 AuthFlow 同时跑：
- 两条 flow 都会弹 Loading、弹 FastAuth 面板、写同一份 State
- 回调被触发 2 次、UI 重叠
- Verification / Session 数据被覆盖

#### 修复

`FDesktopComplianceManager` 加 `bIsStarting` 标志：

```cpp
// .h
private:
    void ClearStartupGuardIfTerminal(int32 Code);
    bool bIsStarting = false;

#if WITH_DEV_AUTOMATION_TESTS
    bool IsStartingForTest() const     { return bIsStarting; }
    void SetStartingForTest(bool bVal) { bIsStarting = bVal; }
#endif

// .cpp StartUp 开头
if (bIsStarting)
{
    UE_LOG(LogTapCompliance, Log,
        TEXT("[Manager] StartUp ignored: another startup in progress (newUser=%s)"),
        *UserId);
    return;
}
// ... (UserId 校验)
bIsStarting = true;

// DispatchCallback 入口
void FDesktopComplianceManager::DispatchCallback(int32 Code, const FString& ExtraJson)
{
    ClearStartupGuardIfTerminal(Code);   // 终态自动复位
    FComplianceState::Get().DispatchCallback(Code, ExtraJson);
}

void FDesktopComplianceManager::ClearStartupGuardIfTerminal(int32 Code)
{
    switch (Code)
    {
    case LOGIN_SUCCESS:
    case REAL_NAME_STOP:
    case EXITED:
    case AGE_LIMIT:
    case SWITCH_ACCOUNT:
    case INVALID_CLIENT_OR_NETWORK_ERROR:
        bIsStarting = false;
        break;
    // PERIOD_RESTRICT / DURATION_LIMIT 不复位:
    // 对齐 Unity ComplianceNewJob.cs:238-247.
    default:
        break;
    }
}
```

注意 **`PERIOD_RESTRICT` / `DURATION_LIMIT` 故意不复位** —— Unity 的设计：心跳禁玩态期望游戏走 `QuitGame`，不允许再开一轮 startup。UE5 严格对齐。

#### 回归测试

这些守卫测试随后随功能撤回而删除，当前测试集不包含 `StartupGuard*` 用例。

---

## 三、🟡 行为差异（待决策）

### Diff 1 · `GetCurrentAccessToken`（当前已恢复）

**Unity**（`TapTapCompliance.cs:68-78`）：

```csharp
public static Task<string> GetCurrentAccessToken()
{
    IComplianceJob Job = ComplianceJobManager.Job;
    if (Job != null) return Job.GetCurrentToken();   // 返回 anti_addiction_token
    ...
}
```

**历史差异**：UE5 曾移除该 API；当前版本已经恢复 `FTapCompliance::GetCurrentAccessToken()`，桌面端从 `FComplianceState.Verification.ComplianceToken` 读取。Unity PC 仍通过 `Job.GetCurrentToken()` 返回同一类 v2 token。

**当前状态**：该项不再是待修复差异。恢复 API 的历史决策可见 commit `d22154e087 feat(UE5): TapCompliance 桌面端补充 GetCurrentAccessToken 与缓存兜底策略`。

---

### Diff 2 · `ShowFastAuthPanel` 的"是否显示取消按钮"判定不全（已修复）

**Unity**（`ComplianceWorker.cs:396`）：

```csharp
bool justShowConfirmBtn = isTapUser && !haveCompliance || !Config.Current.useManual;
```

判定**两个条件**任一为真就只显示"确认"按钮：
1. 是 Tap 用户但 token 没 compliance scope（强制走快速认证升级 scope）
2. 全局配置不允许手动

**历史版本 UE5** 曾只判断 `bUseManual`，缺少 Tap 用户无 compliance scope 时隐藏取消按钮的条件。

**当前代码**已计算当前 Tap 账号和 scope：

```cpp
const bool bIsTapUserWithoutCompliance = bTapLoggedIn && !bHasComplianceScope;
const bool bShowCancel = bUseManual && !bIsTapUserWithoutCompliance;
```

因此当前行为与 Unity 对齐：Tap 用户但缺 compliance scope 时只显示"去使用"；非 Tap 用户且允许手动认证时显示"去使用 + 不使用"。

**修复建议**（历史记录，无需再改）：

```cpp
const bool bIsTapUser = FTapTapLogin::HasValidToken();   // 或同等判定
const bool bHasScope  = TapLoginHasComplianceScope();
const bool bShowCancel = FComplianceState::Get().RealNameConfig.bUseManual
                      && !(bIsTapUser && !bHasScope);
```

当前实现已完成对齐，不需要产品决策。

---

### Diff 3 · Step 2 用户配置对 4xx 业务错也用缓存（已修复）

**Unity**（`BaseComplianceWorker.cs:149-152`）：

```csharp
catch (ComplianceException e) {
    if (e.IsTokenExpired() || e.Code < 500) throw;   // 4xx 抛出 → INVALID
    if (cached != null) { ...return cached; }
    throw;
}
```

**UE5（历史版本）**（`ComplianceAuthFlow.cpp:597-621`）：

```cpp
if (bOk) { ... }
else if (Err.IsTokenExpired()) { ... EXITED }
else {
    FUserAntiConfig Cached;
    if (FCompliancePersistence::LoadUserAntiConfig(UserId, Cached)) {
        EffectiveCfg = Cached;   // ← 4xx 也会走这里
    }
    else { ... INVALID }
}
```

**问题（历史版本）**：UE5 曾把"4xx 业务错"和"5xx/network"用同样路径处理。当前代码已通过 `Err.IsRecoverableForCacheFallback()` 区分可缓存兜底的网络/5xx错误与不可兜底的 4xx 业务错误。

**修复记录**：Step2 当前复用 `IsRecoverableForCacheFallback()`，无需再按下方建议修改：

```cpp
const bool bRecoverable = (Err.Error == TEXT("network_error"))
    || (Err.Error == TEXT("empty_response"))
    || (Err.Error == TEXT("invalid_response"))
    || (Err.Error == TEXT("server_error") && (Err.ErrorCode == 0 || Err.ErrorCode >= 500));
if (!bRecoverable) { Finish(INVALID_CLIENT_OR_NETWORK_ERROR); return; }
```

属 P1 行为差异，未列入 P0 修复批次。

---

### Diff 4 · `GetRemainingTime` 默认值语义

**Unity**（`ComplianceNewJob.cs:62-69`）：

```csharp
if (CurrentRemainSeconds == null) time = 0;
else if (IsAdult) time = 9999;
else time = CurrentRemainSeconds.Value;
```

**UE5**（`DesktopComplianceManager.cpp:261-265`）：

```cpp
int32 V = FComplianceState::Get().RemainSeconds.GetValue();
return V < 0 ? TapComplianceConst::AGE_LIMIT_UNKNOWN : V;   // -1 时返回 -1
```

**差异**：
- Unity 未启动时返回 `0`；UE5 返回 `-1`
- Android 也是返回 `-1`，所以 UE5 实际**对齐 Android**而非 Unity
- `TapCompliance.h:153` 注释写的是 `"-1=未知"`

**结论**：行为正确；开发者门面已统一命名为 `GetRemainingTime`，底层平台实现保留旧内部方法名。

---

### Diff 5 · 心跳 fallback 范围（UE5 略胜）

**Unity**（`BaseComplianceWorker.cs:106-128`）：心跳异常时，仅 `policy.active == time_range` 才走 `CheckOfflinePlayable`，否则抛错。
**UE5**（`CompliancePoll.cpp:158-173`）：同样的逻辑，**完全一致**。

但 UE5 多了 `ShouldUseLocalOnly` 前置短路（`policy_model==local && active==time_range` 时直接跳过心跳走本地），这是**对齐 Android `PlayLogModel.checkUserState`** 的优化，Unity 没有。

**结论**：UE5 加分项，已列入 [§四](#四ue5-优于-unity)。

---

## 四、🟢 UE5 优于 Unity

1. **机器绑定缓存**：`FTapMachineBoundPrefs` 含机器指纹，复制游戏目录到别的机器会自动失效——Unity 的 `Persistence` 没这层保护。
2. **HttpClient 复用 + 重试策略**：`FTapHttpRetryStrategy::CreateDefault(...)` 把重试/退避标准化；Unity 是每个端点自己 try/catch。
3. **`AuthorizationService` 自动 MAC + invalid_time 重签**：Unity 是 `Network.FetchVerificationByTapToken` 内部手写一次重试。
4. **`ShouldUseLocalOnly` 短路**：对齐 Android，节省网络调用。
5. **`FCompliancePoll` 双 ticker 用 `FTSTicker`**：桌面端 `ApplicationWillDeactivateDelegate` 比 Unity 的 `OnApplicationPause` 更可靠。
6. **`AuthReviewing(Waiting/Failed)` 弹窗专门化**：Unity 是复用通用 `HealthPaymentPanel` 弹"认证中"提示，UE5 拆出独立面板，更清晰。
7. **`ComplianceAuthFlow.Finish` 集中埋点**：所有终态码统一在 `Finish` 里 TrackSuccess/Failure/Cancel，Unity 是散在 `Startup` switch 里。
8. **统一 Toast 使用纯 Slate**：当前成功、失败和首次通过提示不再依赖额外 WBP 资产。

---

## 五、🟢 边界细节一致

- ✅ Verification 三层 fallback（缓存 → TapToken → 服务端）—— `Step1_FetchVerification`
- ✅ AgeLimit 哨兵映射（100/110）—— `Step2_ValidateUserConfig:633`
- ✅ `CheckIsAdult = age==18 \|\| age==110` —— `FComplianceVerification::CheckIsAdult`
- ✅ 未成年禁玩态 → `QuitGame()` —— `HandleStep3Result` & `HandlePlayableUpdate`
- ✅ `showSwitchAccount` 仅在禁玩态绑定切号按钮 —— 两处都对齐
- ✅ FastAuth 取消授权回弹 FastAuth 面板（不直接降级手动）—— `OnFastAuthAccept.OnCancelled`
- ✅ 手动实名失败 → AuthReviewing(Failed) → OnConfirmed 重开 RealName 带 `bHasAuthRecord=true`
- ✅ 占位符 `# ${remaining} #` 替换 + 60 秒边界 + 分/秒切换 —— `ResolveHealthReminderText` 跟 Unity Controller 完全一致
- ✅ Heartbeat token 失效闭环 —— Manager / Poll / Step2 / Step3 四处都有
- ✅ `CurrentSessionId` 32 位（UE5 `GenerateSession32` 用 GUID Digits.ToLower）
- ✅ 心跳周期压缩（剩余 < 间隔 → 下轮缩短）
- ✅ 全局配置缓存兜底（带 5xx/network 分级）—— Step1 的 `FetchGlobalConfig`
- ✅ AuthFlow `Finish` 终态埋点（success/failure/cancel/带 `identity_verify_type`）
- ✅ `ClearUserCache` 等价：切 userId 时 `Logout(false)`

---

## 六、其他次要观察

| 项 | 状态 | 备注 |
|---|---|---|
| `SetTestEnvironment` | UE5 直接没实现 | 对的，Unity 已 `[Obsolete]` |
| `OnInvokeExternalCallback` 对外暴露 | UE5 不对外（`DispatchCallback` 是内部） | 更干净 |
| `Application.Quit` 强退 | UE5 用 `UKismetSystemLibrary::QuitGame` | OK，但同样没给业务"自己处理退出"的钩子，与 Unity 一致 |
| `ShowHealthMinorPaymentLimitPanel` 名字带 "Minor" | 但成年人不可支付也会调 | 命名建议改 `ShowHealthPaymentLimitPanel` |
| `RemainSeconds` 用 `FThreadSafeCounter` | 心跳和 1 秒倒计时都 GT，跨线程读时安全 | Unity 是普通 `int?` |

---

## 七、修复记录

### 本次（P0）已修复

| Bug | 文件 | 改动行数 | 测试 |
|---|---|---|---|
| Bug 1 · Exit 不派发 EXITED + 不清缓存 | `Platform/TapCompliancePlatformDesktop.cpp` | +6 / -1 | `TapCompliance.Manager.Pure.ExitDispatchesEXITED` |
| Bug 2 · 首次通过 Toast 文案错 | `Desktop/ComplianceAuthFlow.cpp` | +3 / -1 | （逻辑不变，无新增测试） |
| Bug 3 · Startup 重入互斥 | `Desktop/DesktopComplianceManager.{h,cpp}` | 后续已撤回 | 不适用于当前版本 |

修复策略遵循"对齐 Unity 而非自创"原则：
- Exit 同时做清缓存 + 派发回调，跟 Unity `Exit` 双动作完全一致。
- Startup 互斥曾按 Unity 规则实现，但后续明确撤回，当前行为以现有代码为准。

### 当前仍需决策

- Startup 重入互斥：Unity 使用 `isCheckingUser`，UE5 当前没有对应守卫；由于仓库有明确撤回记录，不在本次清理中擅自恢复。
- `OnComplianceUserChanged`：Unity 会在 `LOGIN_SUCCESS / EXITED / SWITCH_ACCOUNT` 时发布用户与 session 变化事件；UE5 当前没有等价事件。是否需要对齐取决于 UE5 OpenLog/业务方是否消费该契约。
- `GetRemainingTime` 未启动默认值：Unity PC 返回 `0`，UE5/Android 契约返回 `-1`；当前保留 UE5 的 `-1=未知`。

---

## 八、回归测试覆盖

修复后跑全量 `TapCompliance` automation suite：

```
Found 17 automation tests based on 'TapCompliance'
  TapCompliance.AuthFlow.Pure.ComputeOfflinePlayable.*  (3)  Success
  TapCompliance.AuthFlow.Pure.ResolveHealthReminderText.* (3) Success
  TapCompliance.AuthFlow.Pure.ShouldUseLocalOnly        (1)  Success
  TapCompliance.Manager.Pure.ExitDispatchesEXITED       (1)  Success  ← 新增
  TapCompliance.Network.Pure.*                          (3)  Success
  TapCompliance.Public.Pure.AsyncCallbackWhenNotInitialized (1) Success
  TapCompliance.State.Pure.*                            (5)  Success
**** TEST COMPLETE. EXIT CODE: 0 ****
```

执行命令：

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor" \
  UE5/TapSDKDemo.uproject \
  -NoSplash -Unattended -nullrhi \
  -ExecCmds="Automation RunTests TapCompliance;Quit" \
  -TestExit="Automation Test Queue Empty" \
  -Log -stdout -nopause
```

> 文档维护提示：本文档与 Unity 实现绑定。Unity 源码升级时，先看 `Unity_PC_Compliance_Flow.md` 的"已知坑"是否变化，再回来更新本文档的 Diff 项与行号引用。
