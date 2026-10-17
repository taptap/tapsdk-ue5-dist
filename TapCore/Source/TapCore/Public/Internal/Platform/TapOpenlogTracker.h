// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Base Tracker class for a specific TapSDK sub-module.
 *
 * Wraps FTapOpenlog::LogBusiness / LogTechnology and associates every
 * log entry with the module's project name and version string.
 *
 * Usage:
 *   // In your module's singleton / init code:
 *   FTapOpenlogTracker Tracker("TapSDKCore", TapCore_VERSION);
 *   Tracker.TrackInit();
 */
class TAPCORE_API FTapOpenlogTracker
{
public:
	FTapOpenlogTracker(const FString& InProjectName, const FString& InProjectVersion);

	/** Report an "init" business event. */
	void TrackInit() const;

	/**
	 * Report a "start" business event.
	 * @param FuncName   Name of the function / operation that started.
	 * @param SessionId  Unique identifier for this invocation session.
	 * @param Extra      Optional extra key/value pairs.
	 */
	void TrackStart(const FString& FuncName, const FString& SessionId,
	                const TMap<FString, FString>& Extra = {}) const;

	/**
	 * Report a "success" business event.
	 * @param FuncName   Name of the function.
	 * @param SessionId  Session identifier.
	 * @param Extra      Optional extra key/value pairs.
	 */
	void TrackSuccess(const FString& FuncName, const FString& SessionId,
	                  const TMap<FString, FString>& Extra = {}) const;

	/**
	 * Report a "fail" business event.
	 * @param FuncName   Name of the function.
	 * @param SessionId  Session identifier.
	 * @param ErrorCode  Error code string.
	 * @param ErrorMsg   Human-readable error message.
	 * @param Extra      Optional extra key/value pairs.
	 */
	void TrackFailure(const FString& FuncName, const FString& SessionId,
	                  const FString& ErrorCode, const FString& ErrorMsg,
	                  const TMap<FString, FString>& Extra = {}) const;

	/**
	 * Report a "cancel" business event.
	 * @param FuncName   Name of the function.
	 * @param SessionId  Session identifier.
	 */
	void TrackCancel(const FString& FuncName, const FString& SessionId) const;

	const FString& GetProjectName()    const { return ProjectName;    }
	const FString& GetProjectVersion() const { return ProjectVersion; }

private:
	FString ProjectName;
	FString ProjectVersion;
};
