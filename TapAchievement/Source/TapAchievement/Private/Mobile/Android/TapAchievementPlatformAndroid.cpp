// Copyright TapTap. All Rights Reserved.
#include "Mobile/Android/TapAchievementPlatformAndroid.h"

#if PLATFORM_ANDROID

#include "Mobile/Android/TapAchievementAndroidBridge.h"
#include "TapAchievementResultParser.h"
#include "TapAchievementCallbackManager.h"
#include "TapAchievementModule.h"

void FTapAchievementPlatformAndroid::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    // 移动端无需 config：原生 SDK 由 TapCore InitTask 用 TapTapSdkOptions 初始化。
    // 此处只需注册 service + 持久回调。
    TapAchievementAndroidBridge::Init([](const FString& Json)
    {
        // 已在 GameThread(Bridge 的 nativeOnResult 已 marshal)。
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
    TapAchievementAndroidBridge::RegisterCallback();
}

void FTapAchievementPlatformAndroid::Unlock(const FString& AchievementId)
{
    TapAchievementAndroidBridge::Unlock(AchievementId);
}

void FTapAchievementPlatformAndroid::Increment(const FString& AchievementId, int32 Step)
{
    TapAchievementAndroidBridge::Increment(AchievementId, Step);
}

void FTapAchievementPlatformAndroid::ShowAchievements()
{
    TapAchievementAndroidBridge::ShowAchievements();
}

void FTapAchievementPlatformAndroid::SetToastEnable(bool bEnable)
{
    TapAchievementAndroidBridge::SetToastEnable(bEnable);
}

#endif // PLATFORM_ANDROID
