// Copyright TapTap. All Rights Reserved.

#include "Internal/Platform/TapOpenlogTracker.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Internal/Platform/TapOpenlog.h"

namespace
{
	FString BuildArgsJson(const FString& FuncName, const FString& SessionId,
	                      const TMap<FString, FString>& Extra = {})
	{
		FString ArgsJson = FString::Printf(TEXT("{\"func_name\":\"%s\",\"session_id\":\"%s\""),
			*FuncName, *SessionId);
		for (const auto& Pair : Extra)
		{
			ArgsJson += FString::Printf(TEXT(",\"%s\":\"%s\""), *Pair.Key, *Pair.Value);
		}
		ArgsJson += TEXT("}");
		return ArgsJson;
	}
}

FTapOpenlogTracker::FTapOpenlogTracker(const FString& InProjectName, const FString& InProjectVersion)
	: ProjectName(InProjectName)
	, ProjectVersion(InProjectVersion)
{
}

void FTapOpenlogTracker::TrackInit() const
{
	FTapOpenlog::LogBusiness(ProjectName, ProjectVersion, TEXT("init"));
}

void FTapOpenlogTracker::TrackStart(const FString& FuncName, const FString& SessionId,
                                    const TMap<FString, FString>& Extra) const
{
	TMap<FString, FString> Props;
	Props.Add(TEXT("args"), BuildArgsJson(FuncName, SessionId, Extra));
	FTapOpenlog::LogBusiness(ProjectName, ProjectVersion, TEXT("start"), Props);
}

void FTapOpenlogTracker::TrackSuccess(const FString& FuncName, const FString& SessionId,
                                      const TMap<FString, FString>& Extra) const
{
	TMap<FString, FString> Props;
	Props.Add(TEXT("args"), BuildArgsJson(FuncName, SessionId, Extra));
	FTapOpenlog::LogBusiness(ProjectName, ProjectVersion, TEXT("success"), Props);
}

void FTapOpenlogTracker::TrackFailure(const FString& FuncName, const FString& SessionId,
                                      const FString& ErrorCode, const FString& ErrorMsg,
                                      const TMap<FString, FString>& Extra) const
{
	TMap<FString, FString> FailureExtra = Extra;
	FailureExtra.Add(TEXT("error_code"), ErrorCode);
	FailureExtra.Add(TEXT("error_msg"), ErrorMsg);

	TMap<FString, FString> Props;
	Props.Add(TEXT("args"), BuildArgsJson(FuncName, SessionId, FailureExtra));
	FTapOpenlog::LogBusiness(ProjectName, ProjectVersion, TEXT("fail"), Props);
}

void FTapOpenlogTracker::TrackCancel(const FString& FuncName, const FString& SessionId) const
{
	TMap<FString, FString> Props;
	Props.Add(TEXT("args"), BuildArgsJson(FuncName, SessionId));
	FTapOpenlog::LogBusiness(ProjectName, ProjectVersion, TEXT("cancel"), Props);
}

#else

// Non-Desktop stub: provide empty implementations so other modules
// can unconditionally include the header.

FTapOpenlogTracker::FTapOpenlogTracker(const FString& InProjectName, const FString& InProjectVersion)
	: ProjectName(InProjectName), ProjectVersion(InProjectVersion) {}

void FTapOpenlogTracker::TrackInit() const {}
void FTapOpenlogTracker::TrackStart(const FString&, const FString&, const TMap<FString, FString>&) const {}
void FTapOpenlogTracker::TrackSuccess(const FString&, const FString&, const TMap<FString, FString>&) const {}
void FTapOpenlogTracker::TrackFailure(const FString&, const FString&, const FString&, const FString&, const TMap<FString, FString>&) const {}
void FTapOpenlogTracker::TrackCancel(const FString&, const FString&) const {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
