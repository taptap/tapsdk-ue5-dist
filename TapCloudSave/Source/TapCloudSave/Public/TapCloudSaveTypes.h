// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapCloudSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct TAPCLOUDSAVE_API FTapArchiveMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapCloudSave")
	FString Name;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapCloudSave")
	FString Summary;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapCloudSave")
	FString Extra;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "TapCloudSave")
	int32 Playtime = 0;
};

USTRUCT(BlueprintType)
struct TAPCLOUDSAVE_API FTapArchiveData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString Uuid;
	UPROPERTY(BlueprintReadOnly) FString FileId;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Summary;
	UPROPERTY(BlueprintReadOnly) FString Extra;
	UPROPERTY(BlueprintReadOnly) int32 Playtime = 0;
	UPROPERTY(BlueprintReadOnly) int64 ArchiveFileSize = 0;
	UPROPERTY(BlueprintReadOnly) int64 ArchiveCoverSize = 0;
	UPROPERTY(BlueprintReadOnly) int64 CreatedTime = 0;
	UPROPERTY(BlueprintReadOnly) int64 ModifiedTime = 0;
};


// C++ delegates
DECLARE_DELEGATE_OneParam(FTapOnArchiveResult,       const FTapArchiveData& /*Archive*/);
DECLARE_DELEGATE_OneParam(FTapOnArchiveListResult,   const TArray<FTapArchiveData>& /*List*/);
DECLARE_DELEGATE_OneParam(FTapOnArchiveBinaryResult, const TArray<uint8>& /*Bytes*/);
DECLARE_DELEGATE_TwoParams(FTapOnArchiveError,       int32 /*Code*/, const FString& /*Message*/);

// Dynamic delegates (BP)
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapOnArchiveResultDynamic,       const FTapArchiveData&, Archive);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapOnArchiveListResultDynamic,   const TArray<FTapArchiveData>&, List);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapOnArchiveBinaryResultDynamic, const TArray<uint8>&, Bytes);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FTapOnArchiveErrorDynamic,       int32, Code, const FString&, Message);

// Standard raw codes (mirror Unity TapCloudSaveResultCode)
namespace TapCloudSaveResultCode
{
	inline constexpr int32 NEED_LOGIN             = 300001;
	inline constexpr int32 INIT_FAIL              = 300002;
	inline constexpr int32 INVALID_PARAM          = 100001;
	inline constexpr int32 PLATFORM_NOT_AVAILABLE = 100002;
}
