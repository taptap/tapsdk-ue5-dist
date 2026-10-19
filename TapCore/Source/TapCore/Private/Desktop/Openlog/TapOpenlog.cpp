// Copyright TapTap. All Rights Reserved.

#include "Internal/Platform/TapOpenlog.h"
#include "Internal/TapLogPrivacy.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Desktop/Openlog/TapOpenlogNativeAPI.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/Storage/TapDeviceId.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapOpenlog, Log, All);

// ── Static member definitions ─────────────────────────────────────────────────

bool           FTapOpenlog::bInitialized      = false;
FCriticalSection FTapOpenlog::DynamicVarsMutex;
FString        FTapOpenlog::CurrentOpenId;
FString        FTapOpenlog::CurrentGameUserId;
FString        FTapOpenlog::CurrentSdkLocale;
FString        FTapOpenlog::CachedCommonJson;
FString        FTapOpenlog::CachedStartJson;

// ── Init ──────────────────────────────────────────────────────────────────────

void FTapOpenlog::Init()
{
	if (bInitialized)
	{
		UE_LOG(LogTapOpenlog, Warning, TEXT("FTapOpenlog already initialized"));
		return;
	}

	// 1. Load native library
	if (!FTapOpenlogNativeAPI::Load())
	{
		UE_LOG(LogTapOpenlog, Warning, TEXT("tapsdkcore native library not available; OpenLog disabled"));
		return;
	}

	UE_LOG(LogTapOpenlog, Log, TEXT("tapsdkcore version: %s  commit: %s"),
		*FTapOpenlogNativeAPI::GetVersion(), *FTapOpenlogNativeAPI::GetGitCommit());

	// 2. Collect one-time general parameters
	TSharedRef<FJsonObject> CommonObj = MakeShared<FJsonObject>();
	CollectGeneralParameters(CommonObj);

	// 3. Build start-config JSON
	FString StartJson = BuildStartConfig(CommonObj);
	UE_LOG(LogTapOpenlog, Log, TEXT("OpenLog start config: %s"), *TapLogPrivacy::Body(StartJson));

	// 4. Set log level before calling OnAppStarted so that init logs are
	//    suppressed / shown correctly.
	const FTapTapSdkOptions& Cfg = FTapSdkConfigState::GetCurrentConfig();
	FTapOpenlogNativeAPI::SetLogLevel(1, Cfg.bEnableLog ? 1 : 0, 0);

	// 5. Initialise native SDK
	auto StartJsonUtf8 = StringCast<ANSICHAR>(*StartJson);
	int Result = FTapOpenlogNativeAPI::OnAppStarted(
		StartJsonUtf8.Get(),
		&FTapOpenlog::NativeGetCommonVariables,
		&FTapOpenlog::NativeFreeString
	);

	if (Result != 0)
	{
		UE_LOG(LogTapOpenlog, Warning, TEXT("TdkOnAppStarted returned %d; OpenLog may not function correctly"), Result);
	}
	else
	{
		UE_LOG(LogTapOpenlog, Log, TEXT("OpenLog initialized successfully"));
	}

	// 6. Bind lifecycle delegates
	BindLifecycle();

	bInitialized = true;
}

bool FTapOpenlog::IsInitialized()
{
	return bInitialized && FTapOpenlogNativeAPI::IsLoaded();
}

// ── Dynamic state ─────────────────────────────────────────────────────────────

void FTapOpenlog::SetOpenId(const FString& OpenId)
{
	FScopeLock Lock(&DynamicVarsMutex);
	CurrentOpenId = OpenId;
}

void FTapOpenlog::SetGameUserId(const FString& UserId)
{
	FScopeLock Lock(&DynamicVarsMutex);
	CurrentGameUserId = UserId;
}

// ── Parameter helpers ─────────────────────────────────────────────────────────

FString FTapOpenlog::LanguageToLocale()
{
	const FTapTapSdkOptions& Cfg = FTapSdkConfigState::GetCurrentConfig();
	switch (Cfg.PreferredLanguage)
	{
		case ETapLanguage::zh_Hans: return TEXT("zh_CN");
		case ETapLanguage::zh_Hant: return TEXT("zh_TW");
		case ETapLanguage::en:      return TEXT("en_US");
		case ETapLanguage::ja:      return TEXT("ja_JP");
		case ETapLanguage::ko:      return TEXT("ko_KR");
		case ETapLanguage::th:      return TEXT("th_TH");
		case ETapLanguage::id:      return TEXT("id_ID");
		case ETapLanguage::de:      return TEXT("de_DE");
		case ETapLanguage::es:      return TEXT("es_ES");
		case ETapLanguage::fr:      return TEXT("fr_FR");
		case ETapLanguage::pt:      return TEXT("pt_BR");
		case ETapLanguage::ru:      return TEXT("ru_RU");
		case ETapLanguage::tr:      return TEXT("tr_TR");
		case ETapLanguage::vi:      return TEXT("vi_VN");
		default:                    return TEXT("zh_CN");
	}
}

void FTapOpenlog::CollectGeneralParameters(TSharedRef<FJsonObject>& OutObj)
{
	// App info — 从项目设置读取包名，与 gatekeeper bundle_id 保持一致
	FString AppPackageName;
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("PackageName"), AppPackageName, GEngineIni);
		if (AppPackageName.IsEmpty())
		{
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("BundleIdentifier"), AppPackageName, GEngineIni);
		}
	}
	OutObj->SetStringField(TEXT("app_package_name"), AppPackageName);
	// Match Unity/Android OpenLog: use the application version, not SDK GameVersion.
	FString ProjectVersion;
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectVersion"), ProjectVersion, GGameIni);
	}
	OutObj->SetStringField(TEXT("app_version"),      ProjectVersion);
	OutObj->SetStringField(TEXT("app_version_code"), TEXT(""));
	OutObj->SetStringField(TEXT("pn"),               TEXT("TapSDK"));

	// Shared identity must remain available when TapTap Event is disabled.
	FString DeviceId = FTapDeviceId::Get();
	OutObj->SetStringField(TEXT("device_id"),    DeviceId);
	OutObj->SetStringField(TEXT("install_uuid"), DeviceId);

	// CPU / brand
	OutObj->SetStringField(TEXT("dv"),        TEXT(""));
	OutObj->SetStringField(TEXT("cpu"),       TEXT(""));
	OutObj->SetStringField(TEXT("cpu_abis"),  TEXT(""));
	OutObj->SetStringField(TEXT("hardware"),  FPlatformMisc::GetCPUBrand());

	// Device model
	FString DeviceModel = FPlatformMisc::GetDefaultDeviceProfileName();
	OutObj->SetStringField(TEXT("md"), DeviceModel);

	// OS
#if PLATFORM_WINDOWS
	const FString OsName = TEXT("Windows");
#elif PLATFORM_MAC
	const FString OsName = TEXT("Mac");
#elif PLATFORM_LINUX
	const FString OsName = TEXT("Linux");
#else
	const FString OsName = TEXT("Desktop");
#endif
	OutObj->SetStringField(TEXT("os"), OsName);
	OutObj->SetStringField(TEXT("sv"), FPlatformMisc::GetOSVersion());

	// Screen resolution
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	OutObj->SetStringField(TEXT("width"),  FString::FromInt(DisplayMetrics.PrimaryDisplayWidth));
	OutObj->SetStringField(TEXT("height"), FString::FromInt(DisplayMetrics.PrimaryDisplayHeight));

	// Memory
	const FPlatformMemoryConstants& MemConst = FPlatformMemory::GetConstants();
	OutObj->SetStringField(TEXT("total_rom"), TEXT(""));
	OutObj->SetStringField(TEXT("total_ram"), FString::Printf(TEXT("%llu"), (uint64)MemConst.TotalPhysical));

	// Locale
	FString Locale = LanguageToLocale();
	{
		FScopeLock Lock(&DynamicVarsMutex);
		CurrentSdkLocale = Locale;
	}
	OutObj->SetStringField(TEXT("sdk_locale"), Locale);

	// open_id is empty at init time; updated dynamically via NativeGetCommonVariables
	OutObj->SetStringField(TEXT("open_id"), TEXT(""));
}

FString FTapOpenlog::BuildStartConfig(const TSharedRef<FJsonObject>& CommonObj)
{
	const FTapTapSdkOptions& Cfg = FTapSdkConfigState::GetCurrentConfig();

	// Determine region integer: 0=CN, 1=Global, 2=RND
	int32 RegionInt = 0;
	if (FTapSdkConfigState::IsRNDEnvironment())
	{
		RegionInt = 2;
	}
	else if (Cfg.Region != ETapRegion::CN)
	{
		RegionInt = 1;
	}

	// data_dir
	FString DataDir = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		FString::Printf(TEXT("OpenlogData_%s"), *Cfg.ClientID));
	FPaths::NormalizeDirectoryName(DataDir);

	// UA string  (TapCore_VERSION macro defined in Build.cs from .uplugin)
	FString UA = FString::Printf(TEXT("TapSDK-UE5/%s"), TapCore_VERSION);

	// app_duration sub-object
	TSharedPtr<FJsonObject> AppDurationObj = MakeShared<FJsonObject>();
	AppDurationObj->SetStringField(TEXT("tapsdk_version"), TapCore_VERSION);

	// Root config object
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("region"),         RegionInt);
	Root->SetNumberField(TEXT("log_to_console"),  Cfg.bEnableLog ? 1 : 0);
	Root->SetNumberField(TEXT("log_level"),       1);
	Root->SetStringField(TEXT("data_dir"),        DataDir);
	Root->SetStringField(TEXT("env"),             TEXT("local"));
	Root->SetStringField(TEXT("platform"),        TEXT("PC"));
	Root->SetStringField(TEXT("ua"),              UA);
	Root->SetStringField(TEXT("client_id"),       Cfg.ClientID);
	Root->SetStringField(TEXT("client_token"),    Cfg.ClientToken);
	Root->SetObjectField(TEXT("common"),          CommonObj);
	Root->SetObjectField(TEXT("app_duration"),    AppDurationObj);

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return OutStr;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void FTapOpenlog::BindLifecycle()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda([]()
	{
		FTapOpenlogNativeAPI::OnBackground();
	});

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda([]()
	{
		FTapOpenlogNativeAPI::OnForeground();
	});

	// GetApplicationWillTerminateDelegate() returns a thread-safe multicast delegate
	FCoreDelegates::GetApplicationWillTerminateDelegate().AddLambda([]()
	{
		FTapOpenlogNativeAPI::OnAppStopped();
		FTapOpenlogNativeAPI::Unload();
	});
}

// ── C callbacks ───────────────────────────────────────────────────────────────

const char* FTapOpenlog::NativeGetCommonVariables()
{
	// Build a JSON object with dynamic properties and serialize it.
	// The result must stay alive until NativeFreeString is called.
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	FString OpenId, GameUserId, Locale;
	{
		FScopeLock Lock(&DynamicVarsMutex);
		OpenId      = CurrentOpenId;
		GameUserId  = CurrentGameUserId;
		Locale      = CurrentSdkLocale.IsEmpty() ? LanguageToLocale() : CurrentSdkLocale;
	}

	Obj->SetStringField(TEXT("timezone"),           TEXT(""));
	Obj->SetStringField(TEXT("tapsdk_artifact"),    TEXT("UE5"));
	Obj->SetStringField(TEXT("game_user_id"),       GameUserId);
	Obj->SetStringField(TEXT("open_id"),            OpenId);
	Obj->SetStringField(TEXT("gid"),                TEXT(""));
	Obj->SetStringField(TEXT("rom"),                TEXT("0"));
	Obj->SetStringField(TEXT("ram"),                TEXT("0"));
	Obj->SetStringField(TEXT("network_type"),       TEXT(""));
	Obj->SetStringField(TEXT("sdk_locale"),         Locale);

	FString JsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	// Convert to UTF-8 and allocate with FMemory::Malloc so that
	// NativeFreeString can use FMemory::Free.
	auto Utf8 = StringCast<UTF8CHAR>(*JsonStr);
	int32 Len  = Utf8.Length() + 1;   // include null terminator
	char* Buf  = static_cast<char*>(FMemory::Malloc(Len));
	FMemory::Memcpy(Buf, Utf8.Get(), Len);
	return Buf;
}

void FTapOpenlog::NativeFreeString(const char* Ptr)
{
	if (Ptr)
	{
		FMemory::Free(const_cast<char*>(Ptr));
	}
}

// ── Log upload ────────────────────────────────────────────────────────────────

void FTapOpenlog::LogBusiness(const FString& Project, const FString& Version,
                               const FString& Action, const TMap<FString, FString>& Props)
{
	if (!IsInitialized()) { return; }

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	for (const auto& Pair : Props)
	{
		Obj->SetStringField(Pair.Key, Pair.Value);
	}
	Obj->SetStringField(TEXT("tapsdk_project"), Project);
	Obj->SetStringField(TEXT("tapsdk_version"), Version);
	Obj->SetStringField(TEXT("action"),         Action);

	FString JsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	auto LogStoreUtf8  = StringCast<ANSICHAR>(TEXT("tapsdk"));
	auto JsonStrUtf8   = StringCast<ANSICHAR>(*JsonStr);
	FTapOpenlogNativeAPI::OpenLog(LogStoreUtf8.Get(), JsonStrUtf8.Get());
}

void FTapOpenlog::LogTechnology(const FString& Project, const FString& Version,
                                 const FString& Action, const TMap<FString, FString>& Props)
{
	if (!IsInitialized()) { return; }

	// CN region: only TapPayment reports technology logs
	const FTapTapSdkOptions& Cfg = FTapSdkConfigState::GetCurrentConfig();
	if (Cfg.Region == ETapRegion::CN && Project != TEXT("TapPayment"))
	{
		return;
	}

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	// Inflate dynamic properties
	{
		FString OpenId, GameUserId, Locale;
		{
			FScopeLock Lock(&DynamicVarsMutex);
			OpenId     = CurrentOpenId;
			GameUserId = CurrentGameUserId;
			Locale     = CurrentSdkLocale;
		}
		Obj->SetStringField(TEXT("timezone"),        TEXT(""));
		Obj->SetStringField(TEXT("tapsdk_artifact"), TEXT("UE5"));
		Obj->SetStringField(TEXT("game_user_id"),    GameUserId);
		Obj->SetStringField(TEXT("open_id"),         OpenId);
		Obj->SetStringField(TEXT("gid"),             TEXT(""));
		Obj->SetStringField(TEXT("rom"),             TEXT("0"));
		Obj->SetStringField(TEXT("ram"),             TEXT("0"));
		Obj->SetStringField(TEXT("network_type"),    TEXT(""));
		Obj->SetStringField(TEXT("sdk_locale"),      Locale);
	}

	Obj->SetStringField(TEXT("tapsdk_project"), Project);
	Obj->SetStringField(TEXT("tapsdk_version"), Version);
	Obj->SetStringField(TEXT("action"),         Action);

	if (Props.Num() > 0)
	{
		// Serialise Props as a nested JSON string under "args"
		TSharedPtr<FJsonObject> ArgsObj = MakeShared<FJsonObject>();
		for (const auto& Pair : Props)
		{
			ArgsObj->SetStringField(Pair.Key, Pair.Value);
		}
		FString ArgsJson;
		TSharedRef<TJsonWriter<>> ArgsWriter = TJsonWriterFactory<>::Create(&ArgsJson);
		FJsonSerializer::Serialize(ArgsObj.ToSharedRef(), ArgsWriter);
		Obj->SetStringField(TEXT("args"), ArgsJson);
	}

	FString JsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	auto LogStoreUtf8 = StringCast<ANSICHAR>(TEXT("tapsdk-apm"));
	auto JsonStrUtf8  = StringCast<ANSICHAR>(*JsonStr);
	FTapOpenlogNativeAPI::OpenLog(LogStoreUtf8.Get(), JsonStrUtf8.Get());
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
