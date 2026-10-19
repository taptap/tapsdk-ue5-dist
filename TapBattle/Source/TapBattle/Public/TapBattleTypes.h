// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapBattleTypes.generated.h"

// ============================================================================
// Configs (request payloads)
// ============================================================================

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleRoomConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	int32 MaxPlayerCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FString Type;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FString CustomProperties;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	TMap<FString, FString> MatchParams;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattlePlayerConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	int32 CustomStatus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FString CustomProperties;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleCreateRoomConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FTapBattleRoomConfig RoomConfig;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FTapBattlePlayerConfig PlayerConfig;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleMatchRoomConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FTapBattleRoomConfig RoomConfig;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapBattle")
	FTapBattlePlayerConfig PlayerConfig;
};

// ============================================================================
// Entities (response payloads)
// ============================================================================

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattlePlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) int32 Status = 0;
	UPROPERTY(BlueprintReadOnly) int32 CustomStatus = 0;
	UPROPERTY(BlueprintReadOnly) FString CustomProperties;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Type;
	UPROPERTY(BlueprintReadOnly) FString OwnerId;
	UPROPERTY(BlueprintReadOnly) int32 Status = 0;
	UPROPERTY(BlueprintReadOnly) FString CustomProperties;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayerCount = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FTapBattlePlayerInfo> Players;
	UPROPERTY(BlueprintReadOnly) FString CreateTime;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleRoomBasicInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Type;
	UPROPERTY(BlueprintReadOnly) FString CustomProperties;
	UPROPERTY(BlueprintReadOnly) int32 MaxPlayerCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 PlayerCount = 0;
	UPROPERTY(BlueprintReadOnly) FString CreateTime;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleRoomListData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TArray<FTapBattleRoomBasicInfo> Rooms;
	UPROPERTY(BlueprintReadOnly) int32 Offset = 0;
	UPROPERTY(BlueprintReadOnly) bool bHasMore = false;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattlePlayerInputInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString PlayerId;
	UPROPERTY(BlueprintReadOnly) FString Data;
	UPROPERTY(BlueprintReadOnly) FString ServerTime;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleFrameData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 Id = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FTapBattlePlayerInputInfo> Inputs;
};

USTRUCT(BlueprintType)
struct TAPBATTLE_API FTapBattleFrameSyncInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FTapBattleRoomInfo RoomInfo;
	UPROPERTY(BlueprintReadOnly) int32 FrameSyncId = 0;
	UPROPERTY(BlueprintReadOnly) int32 Seed = 0;
	UPROPERTY(BlueprintReadOnly) FString ServerTime;
};

// ============================================================================
// Delegates
// ============================================================================

// 通用无返回成功
DECLARE_DELEGATE(FTapOnBattleSuccess);

// Connect 返回 playerId
DECLARE_DELEGATE_OneParam(FTapOnBattlePlayerIdResult, const FString& /*PlayerId*/);

// 房间相关接口的结果（CreateRoom / MatchRoom / JoinRoom）
DECLARE_DELEGATE_OneParam(FTapOnBattleRoomResult, const FTapBattleRoomInfo& /*RoomInfo*/);

// 房间列表
DECLARE_DELEGATE_OneParam(FTapOnBattleRoomListResult, const FTapBattleRoomListData& /*Rooms*/);

// 失败
DECLARE_DELEGATE_TwoParams(FTapOnBattleError, int32 /*Code*/, const FString& /*Message*/);

// Dynamic delegates (BP)
DECLARE_DYNAMIC_DELEGATE(FTapOnBattleSuccessDynamic);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapOnBattlePlayerIdResultDynamic, const FString&, PlayerId);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapOnBattleRoomResultDynamic,     const FTapBattleRoomInfo&, RoomInfo);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapOnBattleRoomListResultDynamic, const FTapBattleRoomListData&, Rooms);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FTapOnBattleErrorDynamic,         int32, Code, const FString&, Message);

// ============================================================================
// Result codes — 对齐 Unity TapOnlineBattleConstant + 通用 TapSDK 平台/初始化码
// ============================================================================
namespace TapBattleResultCode
{
	// UE5 平台公共码（与 TapCloudSaveResultCode 取齐，便于业务方统一处理）
	inline constexpr int32 NEED_LOGIN             = 300001;
	inline constexpr int32 INIT_FAIL              = 300002;
	inline constexpr int32 INVALID_PARAM          = 100001;
	inline constexpr int32 PLATFORM_NOT_AVAILABLE = 100002;

	// Unity TapOnlineBattleConstant 业务码（透传）
	inline constexpr int32 ERROR_INVALID_PARAMS              = -1;
	inline constexpr int32 ERROR_INIT_INTERNAL_FAILED        = -2;
	inline constexpr int32 ERROR_UNKNOWN                     = 100;
	inline constexpr int32 ERROR_NOT_INIT_OR_LOGIN           = 101;
	inline constexpr int32 ERROR_NOT_LAUNCHED_BY_TAP_CLIENT  = 200;

	inline constexpr int32 ERROR_SYSTEM_ERROR                = 1;
	inline constexpr int32 ERROR_SDK_ERROR                   = 2;
	inline constexpr int32 ERROR_REQUEST_RATE_LIMIT_EXCEEDED = 3;
	inline constexpr int32 ERROR_MALICIOUS_USER              = 4;
	inline constexpr int32 ERROR_TOO_MANY_CONNECTIONS        = 5;
	inline constexpr int32 ERROR_NETWORK_ERROR               = 6;
	inline constexpr int32 ERROR_INVALID_REQUEST             = 11;
	inline constexpr int32 ERROR_INVALID_AUTHORIZATION       = 12;
	inline constexpr int32 ERROR_UNAUTHORIZED                = 13;
	inline constexpr int32 ERROR_ALREADY_CONNECTED           = 14;
	inline constexpr int32 ERROR_PREVIOUS_REQUEST_IN_PROGRESS= 15;
	inline constexpr int32 ERROR_UNIMPLEMENTED               = 16;
	inline constexpr int32 ERROR_FORBIDDEN                   = 17;
	inline constexpr int32 ERROR_ROOM_TEMPLATE_NOT_FOUND     = 18;
	inline constexpr int32 ERROR_ROOM_COUNT_LIMIT_EXCEEDED   = 19;
	inline constexpr int32 ERROR_NOT_IN_ROOM                 = 20;
	inline constexpr int32 ERROR_ALREADY_IN_ROOM             = 21;
	inline constexpr int32 ERROR_NOT_ROOM_OWNER              = 22;
	inline constexpr int32 ERROR_ROOM_FULL                   = 23;
	inline constexpr int32 ERROR_ROOM_NOT_EXIST              = 24;
	inline constexpr int32 ERROR_FRAME_SYNC_NOT_STARTED      = 25;
	inline constexpr int32 ERROR_FRAME_SYNC_ALREADY_STARTED  = 26;
	inline constexpr int32 ERROR_FRAME_INPUT_SIZE_LIMIT_EXCEEDED  = 28;
	inline constexpr int32 ERROR_FRAME_INPUT_COUNT_LIMIT_EXCEEDED = 29;
	inline constexpr int32 ERROR_PLAYER_NOT_FOUND            = 30;
}
