// Copyright TapTap. All Rights Reserved.
#include "TapCloudSaveModule.h"

#include "Platform/ITapCloudSavePlatform.h"
#include "TapCloudSaveCallbackManager.h"

DEFINE_LOG_CATEGORY(LogTapCloudSave);

#define LOCTEXT_NAMESPACE "FTapCloudSaveModule"

void FTapCloudSaveModule::StartupModule()
{
	UE_LOG(LogTapCloudSave, Log, TEXT("TapCloudSave module started"));
}

void FTapCloudSaveModule::ShutdownModule()
{
	// Stop native notifications before invalidating developer callbacks.
	// This prevents a late native event from racing with registry teardown.
	FTapCloudSavePlatformFactory::Reset();
	FTapCloudSaveCallbackManager::Get().Reset();
	// Drop the cached platform instance so its dtor frees the native DLL
	// handle (~FTapCloudSavePlatformDesktop → TapCloudSaveDesktopWrapper::
	// Shutdown). Without this the handle leaks across Editor live-coding
	// cycles / plugin disable→enable, leading to stale function pointers.
	UE_LOG(LogTapCloudSave, Log, TEXT("TapCloudSave module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTapCloudSaveModule, TapCloudSave)
