// Copyright TapTap. All Rights Reserved.
#include "TapShare.h"
#include "TapShareModule.h"
#include "Platform/ITapSharePlatform.h"
#include "Internal/TapSdkChecker.h"

FOnShareResult UTapShare::OnShareResult;

void UTapShare::Share(const FTapShareContent& Content)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    UE_LOG(LogTapShare, Log, TEXT("UTapShare::Share title=%s"), *Content.Title);

    if (auto P = FTapSharePlatformFactory::CreatePlatform())
    {
        P->Share(Content);
    }
    else
    {
        UE_LOG(LogTapShare, Warning, TEXT("UTapShare::Share is only supported on Android / iOS"));
    }
}

void UTapShare::PickImage()
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    if (auto P = FTapSharePlatformFactory::CreatePlatform())
    {
        P->PickImage();
    }
    else
    {
        UE_LOG(LogTapShare, Warning, TEXT("UTapShare::PickImage is only supported on Android"));
    }
}

TArray<FString> UTapShare::GetPickedImagePaths()
{
    if (auto P = FTapSharePlatformFactory::CreatePlatform())
    {
        return P->GetPickedImagePaths();
    }
    return TArray<FString>();
}

void UTapShare::ClearPickedImages()
{
    if (auto P = FTapSharePlatformFactory::CreatePlatform())
    {
        P->ClearPickedImages();
    }
}
