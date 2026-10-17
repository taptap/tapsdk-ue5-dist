// Copyright TapTap. All Rights Reserved.

#include "TapLoginModule.h"
#include "TapLoginAuthorizationServiceImpl.h"
#include "Internal/Http/TapHttp.h"
#if PLATFORM_ANDROID
#include "Platform/TapLoginPlatformAndroid.h"
#endif

// 定义 TapLogin 日志类别
DEFINE_LOG_CATEGORY(LogTapLogin);

#define LOCTEXT_NAMESPACE "FTapLoginModule"

void FTapLoginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin module started"));
	FTapHttp::SetAuthorizationService(MakeShared<FTapLoginAuthorizationServiceImpl>());
}

void FTapLoginModule::ShutdownModule()
{
#if PLATFORM_ANDROID
	FTapLoginPlatformAndroid::ShutdownCallbacks();
#endif
	FTapHttp::SetAuthorizationService(nullptr);
	// This function may be called during shutdown to clean up your module
	UE_LOG(LogTapLogin, Log, TEXT("TapLogin module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTapLoginModule, TapLogin)
