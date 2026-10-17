// Copyright TapTap. All Rights Reserved.
#include "TapAchievement.h"
#include "TapAchievementModule.h"
#include "Platform/ITapAchievementPlatform.h"
#include "Internal/TapSdkChecker.h"
#include "TapAchievementCallbackManager.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"
#endif

UTapAchievement::UTapAchievement()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
    // CDO 硬引用：让 cooker 静态分析发现 WBP_TapAchievementToast(仅桌面 toast 需要)。
    // 见 docs/ue5/knowledge-base/packaging-plugin-content.md。
    static ConstructorHelpers::FClassFinder<UUserWidget> ToastBPClass(
        TEXT("/TapAchievement/UI/WBP_TapAchievementToast"));
    if (ToastBPClass.Succeeded())
    {
        ToastWidgetClass = ToastBPClass.Class;
    }
#endif
}

void UTapAchievement::Unlock(const FString& AchievementId)
{
    if (!FTapSdkChecker::CheckInitialize()) { FTapAchievementCallbackManager::Get().Failure(AchievementId,ETapAchievementError::NotInitialized,TEXT("TapSDK not initialized")); return; }
    if (TSharedPtr<ITapAchievementPlatform> P = FTapAchievementPlatformFactory::CreatePlatform())
    {
        P->Unlock(AchievementId);
    }
}

void UTapAchievement::Increment(const FString& AchievementId, int32 Step)
{
    if (!FTapSdkChecker::CheckInitialize()) { FTapAchievementCallbackManager::Get().Failure(AchievementId,ETapAchievementError::NotInitialized,TEXT("TapSDK not initialized")); return; }
    if (TSharedPtr<ITapAchievementPlatform> P = FTapAchievementPlatformFactory::CreatePlatform())
    {
        P->Increment(AchievementId, Step);
    }
}

void UTapAchievement::ShowAchievements()
{
    if (!FTapSdkChecker::CheckInitialize()) { FTapAchievementCallbackManager::Get().Failure(TEXT(""),ETapAchievementError::NotInitialized,TEXT("TapSDK not initialized")); return; }
    if (TSharedPtr<ITapAchievementPlatform> P = FTapAchievementPlatformFactory::CreatePlatform())
    {
        P->ShowAchievements();
    }
}

void UTapAchievement::SetToastEnable(bool bEnable)
{
    if (!FTapSdkChecker::CheckInitialize()) { FTapAchievementCallbackManager::Get().Failure(TEXT(""),ETapAchievementError::NotInitialized,TEXT("TapSDK not initialized")); return; }
    if (TSharedPtr<ITapAchievementPlatform> P = FTapAchievementPlatformFactory::CreatePlatform())
    {
        P->SetToastEnable(bEnable);
    }
}
void UTapAchievement::RegisterCallback(TScriptInterface<ITapAchievementCallback>C){if(!FTapSdkChecker::CheckInitialize())return;FTapAchievementCallbackManager::Get().Register(C.GetObject());}
void UTapAchievement::UnregisterCallback(TScriptInterface<ITapAchievementCallback>C){FTapAchievementCallbackManager::Get().Unregister(C.GetObject());}
