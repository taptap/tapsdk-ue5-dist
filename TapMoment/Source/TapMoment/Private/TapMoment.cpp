// Copyright TapTap. All Rights Reserved.
#include "TapMoment.h"
#include "TapMomentModule.h"
#include "Platform/ITapMomentPlatform.h"
#include "Internal/TapSdkChecker.h"
#include "TapMomentCallbackManager.h"

namespace { void Log(const TCHAR* Method) { UE_LOG(LogTapMoment, Log, TEXT("UTapMoment::%s"), Method); } }

void UTapMoment::Open()
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    Log(TEXT("Open"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->Open();
}

void UTapMoment::OpenScene(const FString& SceneId)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    Log(TEXT("OpenScene"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->OpenScene(SceneId);
}

void UTapMoment::Publish(const FTapMomentPostData& PostData)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    Log(TEXT("Publish"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->Publish(PostData);
}

void UTapMoment::Close()
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    Log(TEXT("Close"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->Close();
}

void UTapMoment::CloseWithTitle(const FString& Title, const FString& Content)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    Log(TEXT("CloseWithTitle"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->CloseWithTitle(Title, Content);
}

void UTapMoment::FetchNotification()
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    Log(TEXT("FetchNotification"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->FetchNotification();
}

void UTapMoment::SetCallback(TScriptInterface<ITapMomentCallback> Callback)
{
    if(!FTapSdkChecker::CheckInitialize())return;
    FTapMomentCallbackManager::Get().Set(Callback.GetObject());
    Log(TEXT("SetCallback"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform()) P->SetCallback();
}
