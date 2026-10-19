// Copyright TapTap. All Rights Reserved.
#include "TapRep.h"
#include "TapRepModule.h"
#include "Platform/ITapRepPlatform.h"
#include "Internal/TapSdkChecker.h"

void UTapRep::Open(const FString& OpenUrl, const FTapRepCallback& Callback)
{
    if (!FTapSdkChecker::CheckInitialize()) { Callback.ExecuteIfBound(ETapRepError::NOT_INITIALIZED,TEXT("TapSDK not initialized")); return; }
    UE_LOG(LogTapRep, Log, TEXT("UTapRep::Open url=%s"), *OpenUrl);

    if (OpenUrl.IsEmpty())
    {
        UE_LOG(LogTapRep, Warning, TEXT("UTapRep::Open openUrl is empty"));
        Callback.ExecuteIfBound(ETapRepError::PARAM_ERROR, TEXT("openUrl cannot be null or empty"));
        return;
    }

    if (auto P = FTapRepPlatformFactory::CreatePlatform())
    {
        P->Open(OpenUrl, [Callback](int32 C,const FString&M){Callback.ExecuteIfBound(C,M);});
    }
    else
    {
        UE_LOG(LogTapRep, Warning, TEXT("UTapRep::Open is only supported on Android / iOS"));
        Callback.ExecuteIfBound(ETapRepError::PLATFORM_NOT_AVAILABLE,TEXT("platform not available"));
    }
}
