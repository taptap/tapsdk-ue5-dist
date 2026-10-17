// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace TapCloudSaveDesktopWrapper
{
	// Load / unload the native cloudsave_sdk shared library and resolve all exports.
	bool Initialize();
	void Shutdown();
	bool IsReady();

	int  CallInitialize(const FString& ConfigJson);
	void SetLogLevel(int TargetLog, int LogToConsole);
	int  UpdateAccessToken(const FString& TokenJson);

	FString GetSdkVersion();
	FString GetSdkGitCommit();

	FString CallCreateArchive(const FString& MetaJson, const TArray<uint8>& FileBytes, const TArray<uint8>& CoverBytes);
	FString CallUpdateArchive(const FString& Uuid, const FString& MetaJson, const TArray<uint8>& FileBytes, const TArray<uint8>& CoverBytes);
	FString CallDeleteArchive(const FString& Uuid);
	FString CallGetArchiveList();

	// On success: bSucceeded=true, OutBytes populated. On failure: bSucceeded=false,
	// OutErrorJson holds the SDK-emitted error payload (or a synthetic one).
	void CallGetArchiveData (const FString& Uuid, const FString& FileId, bool& bSucceeded, TArray<uint8>& OutBytes, FString& OutErrorJson);
	void CallGetArchiveCover(const FString& Uuid, const FString& FileId, bool& bSucceeded, TArray<uint8>& OutBytes, FString& OutErrorJson);
}
