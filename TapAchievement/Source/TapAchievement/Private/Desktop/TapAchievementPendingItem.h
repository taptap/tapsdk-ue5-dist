#pragma once
#include "CoreMinimal.h"

// ── 离线队列条目（内部，不暴露 Blueprint） ─────────────────────────────────────
struct FTapAchievementPendingItem
{
    FString AchievementId;
    int32   Steps     = 0;
    int32   Type      = 0;  // 0=increment, 1=unlock
    FString UUID;
    int64   TimestampMs = 0;
    FString UserId;         // 产生该条目时的账号 OpenID，用于多账号隔离
};
