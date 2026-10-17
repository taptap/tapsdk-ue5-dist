// Copyright TapTap. All Rights Reserved.
#include "Internal/Launcher/TapLauncherDllWrapper.h"

#if PLATFORM_WINDOWS

#include "HAL/PlatformProcess.h"

typedef bool (*FnRestartAppIfNecessary)(const char*);
typedef int (*FnInit)(char*, const char*);
typedef void (*FnShutdown)();
typedef bool (*FnGetClientID)(char*);
typedef bool (*FnGetOpenID)(char*);
typedef void (*FnRunCallbacks)();
typedef void (*FnRegisterCallback)(int, void*);
typedef void (*FnUnregisterCallback)(int, void*);
typedef bool (*FnAppsIsOwned)();
typedef bool (*FnDLCIsOwned)(const char*);
typedef bool (*FnDLCShowStore)(const char*);
typedef int (*FnAsyncAuthorizeInternal)(
	const char*, const char*, const char*, const char*, const char*,
	const char*, const char*, const char*, const char*);

namespace
{
	void* GDllHandle = nullptr;
	FnRestartAppIfNecessary PfnRestart = nullptr;
	FnInit PfnInit = nullptr;
	FnShutdown PfnShutdown = nullptr;
	FnGetClientID PfnGetClientId = nullptr;
	FnGetOpenID PfnGetOpenId = nullptr;
	FnRunCallbacks PfnRunCallbacks = nullptr;
	FnRegisterCallback PfnRegister = nullptr;
	FnUnregisterCallback PfnUnregister = nullptr;
	FnAppsIsOwned PfnHasLicense = nullptr;
	FnDLCIsOwned PfnDLCOwned = nullptr;
	FnDLCShowStore PfnDLCStore = nullptr;
	FnAsyncAuthorizeInternal PfnAsyncAuthorize = nullptr;

	template <typename T>
	T Resolve(const TCHAR* Symbol)
	{
		void* Ptr = FPlatformProcess::GetDllExport(GDllHandle, Symbol);
		if (!Ptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[TapLauncher] Failed to resolve: %s"), Symbol);
		}
		return reinterpret_cast<T>(Ptr);
	}
}

bool TapLauncherDllWrapper::Load()
{
	if (GDllHandle)
	{
		return true;
	}

	GDllHandle = FPlatformProcess::GetDllHandle(TEXT("taptap_api.dll"));
	if (!GDllHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("[TapLauncher] Failed to load taptap_api.dll"));
		return false;
	}

	PfnRestart = Resolve<FnRestartAppIfNecessary>(TEXT("TapSDK_RestartAppIfNecessary"));
	PfnInit = Resolve<FnInit>(TEXT("TapSDK_Init"));
	PfnShutdown = Resolve<FnShutdown>(TEXT("TapSDK_Shutdown"));
	PfnGetClientId = Resolve<FnGetClientID>(TEXT("TapSDK_GetClientID"));
	PfnGetOpenId = Resolve<FnGetOpenID>(TEXT("TapUser_GetOpenID"));
	PfnRunCallbacks = Resolve<FnRunCallbacks>(TEXT("TapSDK_RunCallbacks"));
	PfnRegister = Resolve<FnRegisterCallback>(TEXT("TapSDK_RegisterCallback"));
	PfnUnregister = Resolve<FnUnregisterCallback>(TEXT("TapSDK_UnregisterCallback"));
	PfnHasLicense = Resolve<FnAppsIsOwned>(TEXT("TapApps_IsOwned"));
	PfnDLCOwned = Resolve<FnDLCIsOwned>(TEXT("TapDLC_IsOwned"));
	PfnDLCStore = Resolve<FnDLCShowStore>(TEXT("TapDLC_ShowStore"));
	// This is part of the same TapTap PC DLL as the launcher APIs.
	PfnAsyncAuthorize = Resolve<FnAsyncAuthorizeInternal>(TEXT("TapUser_AsyncAuthorize_internal"));

	return IsLoaded();
}

void TapLauncherDllWrapper::Unload()
{
	if (PfnShutdown)
	{
		PfnShutdown();
	}
	if (GDllHandle)
	{
		FPlatformProcess::FreeDllHandle(GDllHandle);
		GDllHandle = nullptr;
	}

	PfnRestart = nullptr;
	PfnInit = nullptr;
	PfnShutdown = nullptr;
	PfnGetClientId = nullptr;
	PfnGetOpenId = nullptr;
	PfnRunCallbacks = nullptr;
	PfnRegister = nullptr;
	PfnUnregister = nullptr;
	PfnHasLicense = nullptr;
	PfnDLCOwned = nullptr;
	PfnDLCStore = nullptr;
	PfnAsyncAuthorize = nullptr;
}

bool TapLauncherDllWrapper::IsLoaded()
{
	return GDllHandle && PfnRestart && PfnInit && PfnRunCallbacks;
}

bool TapLauncherDllWrapper::HasAsyncAuthorizeInternal()
{
	return GDllHandle && PfnAsyncAuthorize;
}

bool TapLauncherDllWrapper::HasCallbackExports()
{
	return GDllHandle && PfnRegister && PfnUnregister;
}

int32 TapLauncherDllWrapper::AsyncAuthorizeInternal(
	const FString& ScopeStrings,
	const FString& ResponseType,
	const FString& RedirectUri,
	const FString& CodeChallenge,
	const FString& State,
	const FString& CodeChallengeMethod,
	const FString& VersionCode,
	const FString& SdkUa,
	const FString& Info)
{
	if (!PfnAsyncAuthorize)
	{
		return -1;
	}

	return PfnAsyncAuthorize(
		TCHAR_TO_UTF8(*ScopeStrings),
		TCHAR_TO_UTF8(*ResponseType),
		TCHAR_TO_UTF8(*RedirectUri),
		TCHAR_TO_UTF8(*CodeChallenge),
		TCHAR_TO_UTF8(*State),
		TCHAR_TO_UTF8(*CodeChallengeMethod),
		TCHAR_TO_UTF8(*VersionCode),
		TCHAR_TO_UTF8(*SdkUa),
		TCHAR_TO_UTF8(*Info));
}

bool TapLauncherDllWrapper::RestartAppIfNecessary(const FString& ClientId)
{
	return PfnRestart ? PfnRestart(TCHAR_TO_UTF8(*ClientId)) : false;
}

int32 TapLauncherDllWrapper::Init(FString& OutErrMsg, const FString& PubKey)
{
	if (!PfnInit)
	{
		OutErrMsg = TEXT("DLL not loaded");
		return -1;
	}

	char Buffer[1024] = {};
	const int32 Result = PfnInit(Buffer, TCHAR_TO_UTF8(*PubKey));
	OutErrMsg = UTF8_TO_TCHAR(Buffer);
	return Result;
}

void TapLauncherDllWrapper::Shutdown()
{
	Unload();
}

bool TapLauncherDllWrapper::GetClientId(FString& OutClientId)
{
	if (!PfnGetClientId)
	{
		return false;
	}
	char Buffer[256] = {};
	const bool Result = PfnGetClientId(Buffer);
	OutClientId = UTF8_TO_TCHAR(Buffer);
	return Result;
}

bool TapLauncherDllWrapper::GetOpenId(FString& OutOpenId)
{
	if (!PfnGetOpenId)
	{
		return false;
	}
	char Buffer[256] = {};
	const bool Result = PfnGetOpenId(Buffer);
	OutOpenId = UTF8_TO_TCHAR(Buffer);
	return Result;
}

void TapLauncherDllWrapper::RunCallbacks()
{
	if (PfnRunCallbacks)
	{
		PfnRunCallbacks();
	}
}

void TapLauncherDllWrapper::RegisterCallback(int32 EventId, FNativeCallbackFn Fn)
{
	if (PfnRegister)
	{
		PfnRegister(EventId, reinterpret_cast<void*>(Fn));
	}
}

void TapLauncherDllWrapper::UnregisterCallback(int32 EventId, FNativeCallbackFn Fn)
{
	if (PfnUnregister)
	{
		PfnUnregister(EventId, reinterpret_cast<void*>(Fn));
	}
}

bool TapLauncherDllWrapper::HasLicense()
{
	return PfnHasLicense ? PfnHasLicense() : false;
}

bool TapLauncherDllWrapper::IsDLCOwned(const FString& SkuId)
{
	return PfnDLCOwned ? PfnDLCOwned(TCHAR_TO_UTF8(*SkuId)) : false;
}

bool TapLauncherDllWrapper::ShowDLCStore(const FString& SkuId)
{
	return PfnDLCStore ? PfnDLCStore(TCHAR_TO_UTF8(*SkuId)) : false;
}

#endif // PLATFORM_WINDOWS
