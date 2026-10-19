// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

struct FTapAccessToken;

/**
 * Desktop 合规模块启动编排
 *
 * 主流程 (对齐 Unity BaseComplianceWorker.StartUp / OnVerificationFetched / ValidatePlayableAsync)：
 *
 *   StartUp(userId)
 *     │
 *     ├ Step 1: FetchVerification
 *     │   1.1 优先读本地 verification_v2 缓存命中 → Step 2
 *     │   1.2 否则 GET /real-name/v1/anti-addiction-token
 *     │       - "pass"    → 持久化 → Step 2
 *     │       - "waiting" → 弹审核中提示 → REAL_NAME_STOP(9002)
 *     │       - "failed"  → goto 1.3
 *     │   1.3 弹 FastAuth 引导面板
 *     │       - 用户选「去使用」/「不使用」 → 弹 RealName 手动实名面板
 *     │           - 用户提交 (name+idCard) → 客户端 IdCard 校验 → POST /real-name/v1/anti-addiction-token-manual
 *     │               - pass → 持久化 → Step 2
 *     │               - 其它 → 面板内提示，让用户重试
 *     │       - 用户关闭     → REAL_NAME_STOP(9002)
 *     │
 *     ├ Step 2: ValidateUserConfig
 *     │   GET /anti-addiction/v1/get-config-by-token
 *     │     - token 失效  → Logout + EXITED(1000)
 *     │     - 网络失败 + 本地缓存可用 → 用缓存继续
 *     │     - 网络失败 无缓存 → INVALID_CLIENT_OR_NETWORK_ERROR(1200)
 *     │   生成 SessionId; 写回 Verification.AgeLimit/IsAdult
 *     │   ageCheckResult.allow != true (false / 缺失 / {}) → AGE_LIMIT(1100)  (对齐 Android)
 *     │
 *     └ Step 3: ValidatePlayable
 *         POST /anti-addiction/v1/heartbeat
 *           - 失败时按 policy 决定本地兜底 (本期仅在 policy_active==time_range 时启用)
 *         成年人        → 启动 Poll → LOGIN_SUCCESS(500)
 *         未成年 r > 0 → ShowHealthMinorTimeLimit(可玩态, 按钮“我知道了”)
 *                         用户点击 → 启动 Poll → LOGIN_SUCCESS(500)
 *         未成年 r ≤ 0 → ShowHealthMinorTimeLimit(禁玩态, 按钮“退出游戏”) + PERIOD_RESTRICT(1030)
 */
class FComplianceAuthFlow
{
public:
	/** 入口；外层 (Manager) 已写好 State.CurrentUserId */
	static void StartUp(const FString& UserId);

	/**
	 * 当前是否应跳过服务端心跳, 直接走本地可玩性计算.
	 * 对齐 Android PlayLogModel.checkUserState (PlayLogModel.kt:24):
	 * policy_model == "local" 且 policy_active == "time_range" 时,
	 * 服务端明确要求本地判断, 不应再发心跳.
	 * Poll 和 Step3 都需要在入口判一次.
	 */
	static bool ShouldUseLocalOnly();

	/** 网络失败时的本地兜底 (仅 policy_active==time_range, 或 ShouldUseLocalOnly 命中) */
	static struct FPlayableResult ComputeOfflinePlayable();

#if WITH_DEV_AUTOMATION_TESTS
	/** 测试辅助：使用指定时间计算本地可玩性，避免依赖真实当前时间。 */
	static struct FPlayableResult ComputeOfflinePlayableAt(const FDateTime& Now);
#endif

	/**
	 * 计算「游戏健康提醒」面板要显示的 Title / Content.
	 *
	 * 优先级 (对齐 Android TapComplianceSettings.generateAlertMessage + Unity Controller.Show):
	 *   1) 服务端 heartbeat 返回的 Playable.Title / Playable.Content (description_plain, 已渲染数值)
	 *   2) 若 ① 中任一字段为空 → 回退本地 UserAntiConfig.LocalConfig.TimeRangeConfig.UITipText:
	 *        RemainTime > 0  → Allow.{TipTitle, TipDescription}
	 *        RemainTime <= 0 → Reject.{TipTitle, TipDescription}
	 *   3) Content 含占位符 "# ${remaining} #" → 按 RemainTime 替换:
	 *        RemainTime >= 60 → 用 ceil 上取整的分钟数
	 *        RemainTime <  60 → 用秒数 + 把后半段 "分钟" 替换为 "秒"
	 *        (Substring 切分对齐 Android getRestTime, 避免把占位符前出现的 "60 分钟" 字样误改)
	 *
	 * Out 为空时由 Widget 层兜底为 WBP 默认文案 (比 Android/Unity 多一层保护).
	 */
	static void ResolveHealthReminderText(
		const struct FPlayableResult& Playable,
		const struct FUserAntiConfig& UserAntiConfig,
		FText& OutTitle,
		FText& OutContent);

private:
	// =========================================================
	// Step 1
	// =========================================================
	static void Step1_FetchVerification(const FString& UserId);

	/** 检查 TapLogin 当前 token 是否已包含 compliance scope（受 useAgeRange 控制选择具体 scope） */
	static bool TapLoginHasComplianceScope(const FString& UserId);

	/** 用 TapLogin token 调 FastAuth 端点；成功进 Step2，失败回调 OnFail */
	static void TryFetchByTapToken(const FString& UserId, TFunction<void()> OnFail);
	static void TryFetchByTapToken(
		const FString& UserId,
		const FTapAccessToken& Token,
		TFunction<void()> OnFail);
	static void OnTapTokenVerificationFetched(
		const FString& UserId,
		TFunction<void()> OnFail,
		bool bOk,
		const struct FComplianceVerification& Result,
		const struct FComplianceError& Err);

	/** 服务端 query 之后分支处理 */
	static void OnVerificationFromServer(const FString& UserId, bool bOk,
		const struct FComplianceVerification& Result, const struct FComplianceError& Err);

	/** Tier 4: FastAuth 引导面板 */
	static void ShowFastAuthThenManual(const FString& UserId);

	/** 对齐 Android TapAuthorizationDialog.isManualIdentify: 是否显示"不使用"按钮 */
	static bool ShouldShowFastAuthCancelButton();

	/** "去使用" 按钮回调：触发 OAuth 授权升级，成功后重试 TapToken FastAuth */
	static void OnFastAuthAccept(const FString& UserId);

	/** 弹 RealName 手动实名面板，进入提交 + 重试循环 */
	static void ShowRealNameManual(const FString& UserId);

	/** 提交实名表单 (含 IdCard 客户端校验) */
	static void OnRealNameSubmit(const FString& UserId, const FString& Name, const FString& IdCard);

	// =========================================================
	// Step 2
	// =========================================================
	static void Step2_ValidateUserConfig(const FString& UserId);

	// =========================================================
	// Step 3
	// =========================================================
	static void Step3_ValidatePlayable(const FString& UserId);

	/** Step3 拿到 (本地或心跳) 可玩态后的统一处理: 写 state, 分成年/未成年分支 */
	static void HandleStep3Result(const FString& UserId, const struct FPlayableResult& Effective);

	/** Per-user first LOGIN_SUCCESS toast, aligned with Unity ShowAntiAddictionTip. */
	static void ShowFirstPassTipOnce(const FString& UserId);

	/** 检查给定本地时间是否在节假日时段 */
	static bool IsInLocalGameTime();

	/** 终止流程：收 Loading + 埋点 + 通知回调码 */
	static void Finish(int32 Code, const FString& ExtraJson = FString());
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
