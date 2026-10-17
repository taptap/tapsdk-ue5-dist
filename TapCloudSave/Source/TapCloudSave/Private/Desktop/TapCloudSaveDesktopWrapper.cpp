// Copyright TapTap. All Rights Reserved.

#include "Desktop/TapCloudSaveDesktopWrapper.h"
#include "TapCloudSaveModule.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

extern "C"
{
	typedef void (*FnInitLogger)(void (*)(int, const char*, const char*, const char*));
	typedef int  (*FnInit)(const char*);
	typedef void (*FnFreeMem)(void*);
	typedef int  (*FnUpdateToken)(const char*);
	typedef void (*FnSetLogLevel)(int, int);

	typedef char*          (*FnCreate)   (const char*, const unsigned char*, int, const unsigned char*, int, const char*);
	typedef char*          (*FnUpdate)   (const char*, const char*, const unsigned char*, int, const unsigned char*, int, const char*);
	typedef char*          (*FnDelete)   (const char*, const char*);
	typedef char*          (*FnList)     (const char*);
	typedef unsigned char* (*FnGetData)  (const char*, const char*, int64*, const char*);
	typedef unsigned char* (*FnGetCover) (const char*, const char*, int64*, const char*);

	typedef const char*    (*FnVersion)  (void);
	typedef const char*    (*FnGitCommit)(void);
}

namespace
{
	void* GDllHandle = nullptr;

	FnInitLogger   PfnInitLogger   = nullptr;
	FnInit         PfnInit         = nullptr;
	FnFreeMem      PfnFreeMem      = nullptr;
	FnUpdateToken  PfnUpdateToken  = nullptr;
	FnSetLogLevel  PfnSetLogLevel  = nullptr;
	FnCreate       PfnCreate       = nullptr;
	FnUpdate       PfnUpdate       = nullptr;
	FnDelete       PfnDelete       = nullptr;
	FnList         PfnList         = nullptr;
	FnGetData      PfnGetData      = nullptr;
	FnGetCover     PfnGetCover     = nullptr;
	FnVersion      PfnVersion      = nullptr;
	FnGitCommit    PfnGitCommit    = nullptr;

	FString DllName()
	{
#if PLATFORM_WINDOWS
		return TEXT("cloudsave_sdk.dll");
#elif PLATFORM_MAC
		return TEXT("libcloudsave_sdk.dylib");
#elif PLATFORM_LINUX
		return TEXT("libcloudsave_sdk.so");
#else
		return TEXT("");
#endif
	}

	FString DllAbsPath()
	{
		const FString Plugin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("TapCloudSave"));
#if PLATFORM_WINDOWS
		return FPaths::Combine(Plugin, TEXT("Source/ThirdParty/Desktop/Win64"), DllName());
#elif PLATFORM_MAC
		return FPaths::Combine(Plugin, TEXT("Source/ThirdParty/Desktop/Mac"),   DllName());
#elif PLATFORM_LINUX
		return FPaths::Combine(Plugin, TEXT("Source/ThirdParty/Desktop/Linux"), DllName());
#else
		return TEXT("");
#endif
	}

	template <typename T>
	T Resolve(const char* Symbol)
	{
		void* Sym = FPlatformProcess::GetDllExport(GDllHandle, ANSI_TO_TCHAR(Symbol));
		if (!Sym)
		{
			UE_LOG(LogTapCloudSave, Error, TEXT("Failed to resolve cloudsave_sdk symbol: %s"), ANSI_TO_TCHAR(Symbol));
		}
		return reinterpret_cast<T>(Sym);
	}

	// Takes ownership of a UTF-8 buffer returned by the native SDK and frees it.
	FString TakeStringAndFree(char* Raw)
	{
		if (!Raw) { return FString(); }
		FString Out = UTF8_TO_TCHAR(Raw);
		if (PfnFreeMem) { PfnFreeMem(Raw); }
		return Out;
	}

	void TakeBytesAndFree(unsigned char* Raw, int64 Size, TArray<uint8>& Out)
	{
		Out.Reset();
		if (!Raw) { return; }
		if (Size > 0) { Out.Append(reinterpret_cast<uint8*>(Raw), static_cast<int32>(Size)); }
		// Always free a non-null Raw — early-returning on Size<=0 before the
		// previous fix leaked the allocation when the SDK handed back an
		// empty-but-allocated buffer.
		if (PfnFreeMem) { PfnFreeMem(Raw); }
	}
}

bool TapCloudSaveDesktopWrapper::IsReady()
{
	return GDllHandle != nullptr && PfnInit != nullptr;
}

bool TapCloudSaveDesktopWrapper::Initialize()
{
	if (IsReady()) { return true; }

	// Try the short name first — in packaged builds the .dll/.dylib/.so is
	// staged into $(BinaryOutputDir) by RuntimeDependencies (see Build.cs),
	// so the OS loader will find it via the standard search path.
	const FString ShortName = DllName();
	if (!ShortName.IsEmpty())
	{
		GDllHandle = FPlatformProcess::GetDllHandle(*ShortName);
	}

	// Fallback to the absolute ThirdParty/ path for editor/PIE runs where the
	// library hasn't been staged next to the binary yet.
	FString AbsPath;
	if (!GDllHandle)
	{
		AbsPath = DllAbsPath();
		GDllHandle = FPlatformProcess::GetDllHandle(*AbsPath);
	}

	if (!GDllHandle)
	{
		UE_LOG(LogTapCloudSave, Error,
			TEXT("cloudsave_sdk native library failed to load (short=%s, abs=%s)"),
			*ShortName, *AbsPath);
		return false;
	}

	PfnInitLogger  = Resolve<FnInitLogger> ("TapSdkCloudSaveInitLogger");
	PfnInit        = Resolve<FnInit>       ("TapSdkCloudSaveInitialize");
	PfnFreeMem     = Resolve<FnFreeMem>    ("TapSdkCloudSaveFreeMemory");
	PfnUpdateToken = Resolve<FnUpdateToken>("TapSdkCloudSaveUpdateAccessToken");
	PfnSetLogLevel = Resolve<FnSetLogLevel>("TapSdkCloudSaveSetLogLevel");
	PfnCreate      = Resolve<FnCreate>     ("TapSdkCloudSaveCreateArchive");
	PfnUpdate      = Resolve<FnUpdate>     ("TapSdkCloudSaveUpdateArchive");
	PfnDelete      = Resolve<FnDelete>     ("TapSdkCloudSaveDeleteArchive");
	PfnList        = Resolve<FnList>       ("TapSdkCloudSaveGetArchiveList");
	PfnGetData     = Resolve<FnGetData>    ("TapSdkCloudSaveGetArchiveData");
	PfnGetCover    = Resolve<FnGetCover>   ("TapSdkCloudSaveGetArchiveCover");
	PfnVersion     = Resolve<FnVersion>    ("TapSdkCloudSaveVersion");
	PfnGitCommit   = Resolve<FnGitCommit>  ("TapSdkCloudSaveGitCommit");

	return IsReady();
}

void TapCloudSaveDesktopWrapper::Shutdown()
{
	if (GDllHandle)
	{
		FPlatformProcess::FreeDllHandle(GDllHandle);
		GDllHandle = nullptr;
	}

	PfnInitLogger  = nullptr;
	PfnInit        = nullptr;
	PfnFreeMem     = nullptr;
	PfnUpdateToken = nullptr;
	PfnSetLogLevel = nullptr;
	PfnCreate      = nullptr;
	PfnUpdate      = nullptr;
	PfnDelete      = nullptr;
	PfnList        = nullptr;
	PfnGetData     = nullptr;
	PfnGetCover    = nullptr;
	PfnVersion     = nullptr;
	PfnGitCommit   = nullptr;
}

int TapCloudSaveDesktopWrapper::CallInitialize(const FString& ConfigJson)
{
	if (!PfnInit) { return -1; }
	return PfnInit(TCHAR_TO_UTF8(*ConfigJson));
}

void TapCloudSaveDesktopWrapper::SetLogLevel(int TargetLog, int LogToConsole)
{
	if (PfnSetLogLevel) { PfnSetLogLevel(TargetLog, LogToConsole); }
}

int TapCloudSaveDesktopWrapper::UpdateAccessToken(const FString& TokenJson)
{
	if (!PfnUpdateToken) { return -1; }
	return PfnUpdateToken(TCHAR_TO_UTF8(*TokenJson));
}

FString TapCloudSaveDesktopWrapper::GetSdkVersion()
{
	if (!PfnVersion) { return FString(); }
	const char* V = PfnVersion();
	return V ? FString(UTF8_TO_TCHAR(V)) : FString();
}

FString TapCloudSaveDesktopWrapper::GetSdkGitCommit()
{
	if (!PfnGitCommit) { return FString(); }
	const char* V = PfnGitCommit();
	return V ? FString(UTF8_TO_TCHAR(V)) : FString();
}

FString TapCloudSaveDesktopWrapper::CallCreateArchive(const FString& MetaJson, const TArray<uint8>& FileBytes, const TArray<uint8>& CoverBytes)
{
	if (!PfnCreate) { return FString(); }
	return TakeStringAndFree(PfnCreate(
		TCHAR_TO_UTF8(*MetaJson),
		FileBytes.GetData(),  FileBytes.Num(),
		CoverBytes.GetData(), CoverBytes.Num(),
		"" /*extraParams: UE5 layer does not use Miniapp knobs*/));
}

FString TapCloudSaveDesktopWrapper::CallUpdateArchive(const FString& Uuid, const FString& MetaJson, const TArray<uint8>& FileBytes, const TArray<uint8>& CoverBytes)
{
	if (!PfnUpdate) { return FString(); }
	return TakeStringAndFree(PfnUpdate(
		TCHAR_TO_UTF8(*Uuid),
		TCHAR_TO_UTF8(*MetaJson),
		FileBytes.GetData(),  FileBytes.Num(),
		CoverBytes.GetData(), CoverBytes.Num(),
		""));
}

FString TapCloudSaveDesktopWrapper::CallDeleteArchive(const FString& Uuid)
{
	if (!PfnDelete) { return FString(); }
	return TakeStringAndFree(PfnDelete(TCHAR_TO_UTF8(*Uuid), ""));
}

FString TapCloudSaveDesktopWrapper::CallGetArchiveList()
{
	if (!PfnList) { return FString(); }
	return TakeStringAndFree(PfnList(""));
}

void TapCloudSaveDesktopWrapper::CallGetArchiveData(const FString& Uuid, const FString& FileId, bool& bSucceeded, TArray<uint8>& OutBytes, FString& OutErrorJson)
{
	bSucceeded = false;
	OutBytes.Reset();
	OutErrorJson.Reset();

	if (!PfnGetData)
	{
		OutErrorJson = TEXT("{\"success\":false,\"data\":{\"code\":-1,\"msg\":\"wrapper not ready\"}}");
		return;
	}

	int64 Size = 0;
	unsigned char* Raw = PfnGetData(TCHAR_TO_UTF8(*Uuid), TCHAR_TO_UTF8(*FileId), &Size, "");
	if (Size >= 0)
	{
		bSucceeded = true;
		TakeBytesAndFree(Raw, Size, OutBytes);
	}
	else
	{
		// SDK returns UTF-8 JSON in the same buffer on failure.
		if (Raw) { OutErrorJson = FString(UTF8_TO_TCHAR(reinterpret_cast<char*>(Raw))); }
		if (PfnFreeMem) { PfnFreeMem(Raw); }
	}
}

void TapCloudSaveDesktopWrapper::CallGetArchiveCover(const FString& Uuid, const FString& FileId, bool& bSucceeded, TArray<uint8>& OutBytes, FString& OutErrorJson)
{
	bSucceeded = false;
	OutBytes.Reset();
	OutErrorJson.Reset();

	if (!PfnGetCover)
	{
		OutErrorJson = TEXT("{\"success\":false,\"data\":{\"code\":-1,\"msg\":\"wrapper not ready\"}}");
		return;
	}

	int64 Size = 0;
	unsigned char* Raw = PfnGetCover(TCHAR_TO_UTF8(*Uuid), TCHAR_TO_UTF8(*FileId), &Size, "");
	if (Size >= 0)
	{
		bSucceeded = true;
		TakeBytesAndFree(Raw, Size, OutBytes);
	}
	else
	{
		if (Raw) { OutErrorJson = FString(UTF8_TO_TCHAR(reinterpret_cast<char*>(Raw))); }
		if (PfnFreeMem) { PfnFreeMem(Raw); }
	}
}
