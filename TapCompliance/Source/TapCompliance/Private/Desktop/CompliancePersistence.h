// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceModels.h"

class FTapMachineBoundPrefs;

/**
 * Desktop 合规模块本地持久化 (基于 FTapMachineBoundPrefs)
 *
 * 存储后端：单个 FTapMachineBoundPrefs KV 文件
 *   <ProjectSavedDir>/TapCompliance/<clientId>/cache.json
 *
 * Key 布局：
 *   realname_config                 ← 序列化 FRealNameConfigResult JSON
 *   verification:<md5(userId)>      ← 序列化 FComplianceVerification JSON
 *   user_config:<md5(userId)>       ← 序列化 FUserAntiConfig JSON
 *
 * 两层隔离：
 *   - 机器：FTapMachineBoundPrefs 内部写入 tap_machine_fp; 读时若指纹不匹配
 *           (例如游戏目录拷到别机器), 整个 KV 文件被清空, 上层重新走 server 拉取.
 *   - 用户：userId 经 md5 哈希后挂在 key 上, 不同账号自然走不同 key,
 *           DeleteVerification/DeleteUserAntiConfig 只删该 userId 对应 key.
 *
 * 历史路径 <ClientId>/verification_v2/{hash}.json 等遗留文件不再读写,
 * 升级后会以孤儿态留在磁盘上, 不影响业务 (cache miss 自然 refetch).
 */
class FCompliancePersistence
{
public:
	// =========================================================
	// FComplianceVerification (per-user)
	// =========================================================
	static bool LoadVerification(const FString& UserId, FComplianceVerification& OutResult);
	static bool SaveVerification(const FString& UserId, const FComplianceVerification& Result);
	static bool DeleteVerification(const FString& UserId);

	// =========================================================
	// FUserAntiConfig (per-user)
	// =========================================================
	static bool LoadUserAntiConfig(const FString& UserId, FUserAntiConfig& OutResult);
	static bool SaveUserAntiConfig(const FString& UserId, const FUserAntiConfig& Result);
	static bool DeleteUserAntiConfig(const FString& UserId);

	// =========================================================
	// FRealNameConfigResult (全局, 与 userId 无关)
	// =========================================================
	static bool LoadRealNameConfig(FRealNameConfigResult& OutResult);
	static bool SaveRealNameConfig(const FRealNameConfigResult& Result);

	// =========================================================
	// 一次性 UI 标记 (per-user)
	//
	// 对齐 Unity TapTapComplianceManager.cs:220 DataStorage.LoadString/SaveString
	// "compliance_tip_{md5(userId)}" 用于首次防沉迷通过 toast 只弹一次.
	// =========================================================
	static bool HasShownFirstPassTip(const FString& UserId);
	static void MarkFirstPassTipShown(const FString& UserId);

private:
	/**
	 * 进程内单例 FTapMachineBoundPrefs.
	 * 首次访问时按当前 ClientId 懒加载. ClientId 在 SDK Init 阶段写入 FComplianceState,
	 * 之后不变, 因此单例缓存 ClientId 校验后可安全复用.
	 */
	static FTapMachineBoundPrefs& GetPrefs();

	/** userId → 32 位小写 MD5; 空 userId 统一映射成 "anonymous" 槽 */
	static FString HashUserId(const FString& UserId);

	// Key 拼接 (与文档 Key 布局对应)
	static FString KeyForVerification(const FString& UserId);
	static FString KeyForUserAntiConfig(const FString& UserId);
	static FString KeyForRealNameConfig();
	static FString KeyForFirstPassTip(const FString& UserId);

	// JSON 序列化辅助 (FTapMachineBoundPrefs 只支持 string/int64, 复杂结构走字符串)
	static FString SerializeJsonObject(const TSharedPtr<class FJsonObject>& Obj);
	static TSharedPtr<class FJsonObject> ParseJsonString(const FString& Json);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
