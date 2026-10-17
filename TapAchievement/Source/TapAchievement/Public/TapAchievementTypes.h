// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapAchievementTypes.generated.h"

// ── 错误码（对齐 Unity TapTapAchievementConstants） ──────────────────────────
namespace ETapAchievementError
{
    constexpr int32 UnlockSuccess      = 70001;
    constexpr int32 IncrementSuccess   = 70002;
    constexpr int32 NotInitialized     = 80000;
    constexpr int32 RegionNotSupported = 80001;
    constexpr int32 NotLogged          = 80002;
    constexpr int32 AccessDenied       = 80010;
    constexpr int32 InvalidRequest     = 80020;
    constexpr int32 NetworkError       = 80030;
    constexpr int32 UnknownError       = 80100;
}

// ── 成就类型 ─────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class ETapAchievementType : uint8
{
    Normal   = 0 UMETA(DisplayName = "Normal"),
    Platinum = 1 UMETA(DisplayName = "Platinum")
};

// ── 回调结果（对齐 Unity TapAchievementResult） ───────────────────────────────
USTRUCT(BlueprintType)
struct TAPACHIEVEMENT_API FTapAchievementResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString AchievementId;
    UPROPERTY(BlueprintReadOnly) FString AchievementName;
    UPROPERTY(BlueprintReadOnly) ETapAchievementType AchievementType = ETapAchievementType::Normal;
    UPROPERTY(BlueprintReadOnly) int64 CurrentSteps = 0;
};

// ── 委托（对齐 Unity ITapAchievementCallback） ────────────────────────────────
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAchievementSuccess,
    int32 /*Code*/, const FTapAchievementResult& /*Result*/);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnAchievementFailure,
    const FString& /*AchievementId*/, int32 /*ErrorCode*/, const FString& /*ErrorMsg*/);
