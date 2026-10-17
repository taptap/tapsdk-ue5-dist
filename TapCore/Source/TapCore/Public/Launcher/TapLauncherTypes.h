// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapLauncherTypes.generated.h"

/** 对齐 Unity TapSDKInitResult。BlueprintType 枚举必须用 uint8 基类型。
 *  Unity 侧 Unknown = -1 映射为 255（uint8 无符号最大值）。 */
UENUM(BlueprintType)
enum class ETapLauncherInitResult : uint8
{
	OK                       = 0,
	FailedGeneric            = 1,
	NoPlatform               = 2,    // TapTap 客户端未安装
	NotLaunchedByPlatform    = 3,    // 未通过 TapTap 启动
	PlatformVersionMismatch  = 4,    // 版本不匹配，需升级
	Unknown                  = 255,  // 对齐 Unity Unknown(-1)，uint8 用 255 表示
};

/** 对齐 Unity SystemState */
UENUM(BlueprintType)
enum class ETapClientState : uint8
{
	Unknown  = 0,
	Online   = 1,
	Offline  = 2,
	Shutdown = 3,
};

// 启动校验成功，携带当前登录用户的 OpenId
DECLARE_DELEGATE_OneParam(FOnLauncherCheckSuccess, const FString& /*OpenId*/);
// 启动校验失败
DECLARE_DELEGATE_TwoParams(FOnLauncherCheckError, ETapLauncherInitResult /*Code*/, const FString& /*Message*/);

// TapTap 客户端运行状态变化（支持多监听者）
DECLARE_MULTICAST_DELEGATE_OneParam(FOnClientStateChanged, ETapClientState);

namespace TapLauncherResultCode
{
	constexpr int32 NotInitialized = -1000;  // Launcher check 未通过
}

namespace TapLauncherEventId
{
	constexpr int32 SystemStateChanged   = 1;     // 对齐 Unity TapEventID.SystemStateChanged
	constexpr int32 AuthorizeFinishedInternal = 2001; // 对齐 Unity TapEventID.AuthorizeFinished_internal
	constexpr int32 GamePlayableChanged  = 4001;  // 对齐 Unity TapEventID.GamePlayableStatusChanged
	constexpr int32 DLCPlayableChanged   = 4002;  // 对齐 Unity TapEventID.DLCPlayableStatusChanged
}
