// Copyright TapTap. All Rights Reserved.
#include "TapAchievementToastManager.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "TapAchievementModule.h"
#include "TapAchievementToastWidget.h"
#include "Engine/GameViewportClient.h"
#include "Blueprint/UserWidget.h"

#include "Engine/Engine.h"
#include "Engine/AssetManager.h"

const FSoftObjectPath FTapAchievementToastManager::ToastWidgetPath(
    TEXT("/TapAchievement/UI/WBP_TapAchievementToast.WBP_TapAchievementToast"));

static TUniquePtr<FTapAchievementToastManager> GToastManager;

FTapAchievementToastManager& FTapAchievementToastManager::Get()
{
    if (!GToastManager.IsValid())
        GToastManager = MakeUnique<FTapAchievementToastManager>();
    return *GToastManager;
}

void FTapAchievementToastManager::Enqueue(const FTapAchievementResult& Result)
{
    ToastQueue.Enqueue(Result);
    if (!bIsShowing)
        ShowNext();
}

void FTapAchievementToastManager::ShowNext()
{
    FTapAchievementResult Result;
    if (!ToastQueue.Dequeue(Result))
    {
        bIsShowing = false;
        return;
    }
    bIsShowing = true;

    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
    WidgetHandle = Streamable.RequestAsyncLoad(
        ToastWidgetPath,
        FStreamableDelegate::CreateLambda([this, Result]()
        {
            // Blueprint 生成类的路径是对象路径 + "_C"。
            // ResolveObject() 对蓝图路径返回 UWidgetBlueprint，Cast<UClass> 会得到 nullptr。
            // 需要通过 _C 后缀找到生成的 UClass。
            const FString ClassPath = ToastWidgetPath.GetAssetPathString() + TEXT("_C");
            UClass* WidgetClass = FindObject<UClass>(nullptr, *ClassPath);
            if (!WidgetClass)
            {
                // Fallback: 内存中未找到则同步加载（首次进入时兜底）
                WidgetClass = LoadObject<UClass>(nullptr, *ClassPath);
            }
            if (!WidgetClass || !GEngine || !GEngine->GameViewport)
            {
                UE_LOG(LogTapAchievement, Warning,
                    TEXT("Toast: cannot resolve widget class '%s' or no viewport"), *ClassPath);
                OnToastFinished();
                return;
            }
            UWorld* World = GEngine->GameViewport->GetWorld();

            // 要求 WBP 继承自 UTapAchievementToastWidget
            UTapAchievementToastWidget* Widget =
                CreateWidget<UTapAchievementToastWidget>(World, WidgetClass);
            if (!Widget)
            {
                UE_LOG(LogTapAchievement, Warning,
                    TEXT("Toast: WBP_TapAchievementToast must inherit UTapAchievementToastWidget"));
                OnToastFinished();
                return;
            }

            Widget->AddToViewport(100);
            Widget->SetAchievementData(Result);  // 触发 OnSetAchievementData BP 事件
            CurrentWidget = Widget;
        }));
}

void FTapAchievementToastManager::OnToastFinished()
{
    bIsShowing = false;
    if (CurrentWidget.IsValid())
    {
        CurrentWidget->RemoveFromParent();
        CurrentWidget = nullptr;
    }
    ShowNext();
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC
