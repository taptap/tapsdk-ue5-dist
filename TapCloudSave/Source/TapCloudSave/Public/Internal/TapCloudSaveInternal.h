// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapCloudSaveTypes.h"

DECLARE_DELEGATE_TwoParams(FTapOnArchiveFileResultInternal,
	const TArray<uint8>& /*Bytes*/, const FString& /*SavedFilePath*/);

/**
 * TapCloudSave UE file helpers for SDK-owned code and samples.
 * This is not a game-facing API and carries no public compatibility guarantee.
 */
class TAPCLOUDSAVE_API FTapCloudSaveInternal
{
public:
	static void GetArchiveDataToFile(
		const FString& Uuid, const FString& FileId, const FString& OutFilePath,
		const FTapOnArchiveFileResultInternal& OnSuccess, const FTapOnArchiveError& OnError);

	static void GetArchiveCoverToFile(
		const FString& Uuid, const FString& FileId, const FString& OutFilePath,
		const FTapOnArchiveFileResultInternal& OnSuccess, const FTapOnArchiveError& OnError);
};
