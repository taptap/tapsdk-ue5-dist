// Copyright TapTap. All Rights Reserved.

#include "Platform/TapLoginTracker.h"
#include "Internal/Platform/TapOpenlog.h"

// TapLogin_VERSION is defined by Build.cs from the .uplugin file.
#ifndef TapLogin_VERSION
#define TapLogin_VERSION TEXT("0.0.0")
#endif

const FString FTapLoginTracker::LOGIN_TYPE_CODE    = TEXT("pc_code");
const FString FTapLoginTracker::LOGIN_TYPE_BROWSER = TEXT("pc_browser");

FTapLoginTracker::FTapLoginTracker()
	: Tracker(TEXT("TapLogin"), TapLogin_VERSION)
{
}

FTapLoginTracker& FTapLoginTracker::Get()
{
	static FTapLoginTracker Instance;
	return Instance;
}

void FTapLoginTracker::TrackInit()
{
	Tracker.TrackInit();
}

void FTapLoginTracker::TrackStart(const FString& FuncName, const FString& SessionId, const FString& LoginType)
{
	if (LoginType.IsEmpty())
	{
		Tracker.TrackStart(FuncName, SessionId);
	}
	else
	{
		TMap<FString, FString> Props;
		FString ArgsJson = FString::Printf(
			TEXT("{\"func_name\":\"%s\",\"session_id\":\"%s\",\"login_type\":\"%s\"}"),
			*FuncName, *SessionId, *LoginType);
		Props.Add(TEXT("args"), ArgsJson);
		FTapOpenlog::LogBusiness(Tracker.GetProjectName(), Tracker.GetProjectVersion(), TEXT("start"), Props);
	}
}

void FTapLoginTracker::TrackSuccess(const FString& FuncName, const FString& SessionId, const FString& LoginType)
{
	TMap<FString, FString> Extra;
	Extra.Add(TEXT("login_type"), LoginType);
	Tracker.TrackSuccess(FuncName, SessionId, Extra);
}

void FTapLoginTracker::TrackFailure(const FString& FuncName, const FString& SessionId,
                                    const FString& LoginType, int32 ErrorCode, const FString& ErrorMsg)
{
	TMap<FString, FString> Props;
	FString ArgsJson = FString::Printf(
		TEXT("{\"func_name\":\"%s\",\"session_id\":\"%s\",\"login_type\":\"%s\",\"error_code\":\"%d\",\"error_msg\":\"%s\"}"),
		*FuncName, *SessionId, *LoginType, ErrorCode, *ErrorMsg);
	Props.Add(TEXT("args"), ArgsJson);
	FTapOpenlog::LogBusiness(Tracker.GetProjectName(), Tracker.GetProjectVersion(), TEXT("fail"), Props);
}

void FTapLoginTracker::TrackCancel(const FString& FuncName, const FString& SessionId, const FString& LoginType)
{
	if (LoginType.IsEmpty())
	{
		Tracker.TrackCancel(FuncName, SessionId);
	}
	else
	{
		TMap<FString, FString> Props;
		FString ArgsJson = FString::Printf(
			TEXT("{\"func_name\":\"%s\",\"session_id\":\"%s\",\"login_type\":\"%s\"}"),
			*FuncName, *SessionId, *LoginType);
		Props.Add(TEXT("args"), ArgsJson);
		FTapOpenlog::LogBusiness(Tracker.GetProjectName(), Tracker.GetProjectVersion(), TEXT("cancel"), Props);
	}
}
