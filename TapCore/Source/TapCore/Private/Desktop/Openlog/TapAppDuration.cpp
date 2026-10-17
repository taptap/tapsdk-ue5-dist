// Copyright TapTap. All Rights Reserved.

#include "Internal/Platform/TapAppDuration.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Desktop/Openlog/TapOpenlogNativeAPI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FTapAppDuration::Enable()
{
	FTapOpenlogNativeAPI::EnableModules("[\"app_duration\"]");
}

void FTapAppDuration::Disable()
{
	FTapOpenlogNativeAPI::DisableModules("[\"app_duration\"]");
}

void FTapAppDuration::OnLogin(const FString& OpenId)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("open_id"), OpenId);

	FString JsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	auto Utf8 = StringCast<ANSICHAR>(*JsonStr);
	FTapOpenlogNativeAPI::OnLogin(Utf8.Get());
}

void FTapAppDuration::OnLogout()
{
	FTapOpenlogNativeAPI::OnLogout();
}

void FTapAppDuration::SetExtraParams(const TMap<FString, FString>& Params)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	for (const auto& Pair : Params)
	{
		Obj->SetStringField(Pair.Key, Pair.Value);
	}

	FString JsonStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);

	auto Utf8 = StringCast<ANSICHAR>(*JsonStr);
	FTapOpenlogNativeAPI::SetExtraAppDurationParams(Utf8.Get());
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
