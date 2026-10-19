// Copyright TapTap. All Rights Reserved.

#include "TapComplianceTracker.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "Internal/Platform/TapOpenlog.h"

FTapComplianceTracker::FTapComplianceTracker()
	: Tracker(TEXT("TapCompliance"), TapComplianceConst::MODULE_VERSION)
{
}

FTapComplianceTracker& FTapComplianceTracker::Get()
{
	static FTapComplianceTracker Instance;
	return Instance;
}

void FTapComplianceTracker::TrackInit() const
{
	Tracker.TrackInit();
}

void FTapComplianceTracker::TrackStart(const FString& FuncName, const FString& SessionId) const
{
	Tracker.TrackStart(FuncName, SessionId);
}

void FTapComplianceTracker::TrackSuccess(const FString& FuncName, const FString& SessionId,
                                         const TMap<FString, FString>& Extra) const
{
	Tracker.TrackSuccess(FuncName, SessionId, Extra);
}

void FTapComplianceTracker::TrackFailure(const FString& FuncName, const FString& SessionId,
                                         const FString& ErrorCode, const FString& ErrorMsg,
                                         const TMap<FString, FString>& Extra) const
{
	TMap<FString, FString> Props;
	FString ArgsJson = FString::Printf(
		TEXT("{\"func_name\":\"%s\",\"session_id\":\"%s\",\"error_code\":\"%s\",\"error_msg\":\"%s\""),
		*FuncName, *SessionId, *ErrorCode, *ErrorMsg);
	for (const auto& Pair : Extra)
	{
		ArgsJson += FString::Printf(TEXT(",\"%s\":\"%s\""), *Pair.Key, *Pair.Value);
	}
	ArgsJson += TEXT("}");
	Props.Add(TEXT("args"), ArgsJson);
	FTapOpenlog::LogBusiness(Tracker.GetProjectName(), Tracker.GetProjectVersion(), TEXT("fail"), Props);
}

void FTapComplianceTracker::TrackCancel(const FString& FuncName, const FString& SessionId,
                                        const TMap<FString, FString>& Extra) const
{
	TMap<FString, FString> Props;
	FString ArgsJson = FString::Printf(TEXT("{\"func_name\":\"%s\",\"session_id\":\"%s\""), *FuncName, *SessionId);
	for (const auto& Pair : Extra)
	{
		ArgsJson += FString::Printf(TEXT(",\"%s\":\"%s\""), *Pair.Key, *Pair.Value);
	}
	ArgsJson += TEXT("}");
	Props.Add(TEXT("args"), ArgsJson);
	FTapOpenlog::LogBusiness(Tracker.GetProjectName(), Tracker.GetProjectVersion(), TEXT("cancel"), Props);
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
