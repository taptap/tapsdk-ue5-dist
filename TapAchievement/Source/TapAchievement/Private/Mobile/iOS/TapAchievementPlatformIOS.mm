// Copyright TapTap. All Rights Reserved.
#include "Mobile/iOS/TapAchievementPlatformIOS.h"

#if PLATFORM_IOS

#include "Mobile/iOS/TapAchievementIOSBridge.h"
#include "TapAchievementResultParser.h"
#include "TapAchievementCallbackManager.h"
#include "TapAchievementModule.h"

void FTapAchievementPlatformIOS::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    // 移动端无需 config：原生 SDK 由 TapCore InitTask 初始化。此处注册持久回调。
    TapAchievementIOSBridge::Init([](const FString& Json)
    {
        // 已 marshal 到 GameThread。
        const TapAchievementResultParser::FParsed P = TapAchievementResultParser::Parse(Json);
        if (!P.bParsed) { return; }
        if (P.bIsSuccess)
        {
            FTapAchievementCallbackManager::Get().Success(P.Code, P.Result);
        }
        else
        {
            FTapAchievementCallbackManager::Get().Failure(P.AchievementId, P.ErrorCode, P.ErrorMsg);
        }
    });
}

void FTapAchievementPlatformIOS::Unlock(const FString& AchievementId)
{
    TapAchievementIOSBridge::Unlock(AchievementId);
}

void FTapAchievementPlatformIOS::Increment(const FString& AchievementId, int32 Step)
{
    TapAchievementIOSBridge::Increment(AchievementId, Step);
}

void FTapAchievementPlatformIOS::ShowAchievements()
{
    TapAchievementIOSBridge::ShowAchievements();
}

void FTapAchievementPlatformIOS::SetToastEnable(bool bEnable)
{
    TapAchievementIOSBridge::SetToastEnable(bEnable);
}

#endif // PLATFORM_IOS
