// Copyright TapTap. All Rights Reserved.

#include "Internal/TapCloudSaveInternal.h"

#include "TapCloudSave.h"
#include "TapCloudSaveModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FTapOnArchiveBinaryResult SaveDownloadedFile(const FString& Path,
		const FTapOnArchiveFileResultInternal& OnSuccess, const FTapOnArchiveError& OnError)
	{
		return FTapOnArchiveBinaryResult::CreateLambda([Path, OnSuccess, OnError](const TArray<uint8>& Bytes)
		{
			const FString Directory = FPaths::GetPath(Path);
			if ((!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true)) ||
				!FFileHelper::SaveArrayToFile(Bytes, *Path))
			{
				OnError.ExecuteIfBound(-1, FString::Printf(TEXT("Failed to save downloaded file: %s"), *Path));
				return;
			}
			OnSuccess.ExecuteIfBound(Bytes, Path);
		});
	}

	bool ValidateOutputPath(const FString& OutFilePath, const FTapOnArchiveError& OnError)
	{
		if (!OutFilePath.IsEmpty())
		{
			return true;
		}
		UE_LOG(LogTapCloudSave, Warning, TEXT("[TapCloudSave] Input check failed: OutFilePath required"));
		OnError.ExecuteIfBound(TapCloudSaveResultCode::INVALID_PARAM, TEXT("OutFilePath required"));
		return false;
	}
}

void FTapCloudSaveInternal::GetArchiveDataToFile(const FString& Uuid, const FString& FileId,
	const FString& OutFilePath, const FTapOnArchiveFileResultInternal& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	if (!ValidateOutputPath(OutFilePath, OnError)) return;
	UTapCloudSave::GetArchiveData(Uuid, FileId, SaveDownloadedFile(OutFilePath, OnSuccess, OnError), OnError);
}

void FTapCloudSaveInternal::GetArchiveCoverToFile(const FString& Uuid, const FString& FileId,
	const FString& OutFilePath, const FTapOnArchiveFileResultInternal& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	if (!ValidateOutputPath(OutFilePath, OnError)) return;
	UTapCloudSave::GetArchiveCover(Uuid, FileId, SaveDownloadedFile(OutFilePath, OnSuccess, OnError), OnError);
}
