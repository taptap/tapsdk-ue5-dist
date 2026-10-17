// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapCloudSaveTypes.h"
#include "TapCloudSaveCallback.h"
#include "TapCloudSave.generated.h"

/**
 * TapCloudSave 静态门面 API（C++ + Blueprint 双轨）
 * 与 Unity TapTapCloudSave 对齐
 */
UCLASS()
class TAPCLOUDSAVE_API UTapCloudSave : public UObject
{
	GENERATED_BODY()

public:
	static FString GetVersion();

	// State callback（全局广播，NEED_LOGIN/INIT_FAIL 等）
	UFUNCTION(BlueprintCallable,Category="TapCloudSave") static void RegisterCloudSaveCallback(TScriptInterface<ITapCloudSaveCallback> Callback);
	UFUNCTION(BlueprintCallable,Category="TapCloudSave") static void UnregisterCloudSaveCallback(TScriptInterface<ITapCloudSaveCallback> Callback);

	// CRUD
	static void CreateArchive(
		const FTapArchiveMetadata& Metadata,
		const FString& ArchiveFilePath,
		const FString& CoverFilePath,
		const FTapOnArchiveResult& OnSuccess,
		const FTapOnArchiveError&  OnError);

	static void UpdateArchive(
		const FString& Uuid,
		const FTapArchiveMetadata& Metadata,
		const FString& ArchiveFilePath,
		const FString& CoverFilePath,
		const FTapOnArchiveResult& OnSuccess,
		const FTapOnArchiveError&  OnError);

	static void DeleteArchive(
		const FString& Uuid,
		const FTapOnArchiveResult& OnSuccess,
		const FTapOnArchiveError&  OnError);

	static void GetArchiveList(
		const FTapOnArchiveListResult& OnSuccess,
		const FTapOnArchiveError&      OnError);

	static void GetArchiveData(
		const FString& Uuid,
		const FString& FileId,
		const FTapOnArchiveBinaryResult& OnSuccess,
		const FTapOnArchiveError&        OnError);

	static void GetArchiveCover(
		const FString& Uuid,
		const FString& FileId,
		const FTapOnArchiveBinaryResult& OnSuccess,
		const FTapOnArchiveError&        OnError);
};
