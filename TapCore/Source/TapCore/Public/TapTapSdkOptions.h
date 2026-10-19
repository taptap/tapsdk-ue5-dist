// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "TapBasicTypes.h"
#include "TapTapSdkOptions.generated.h"

/** Base type for module-specific initialization options. */
USTRUCT(BlueprintType)
struct TAPCORE_API FTapSdkBaseOptions
{
	GENERATED_BODY()
};

/**
 * TapTap Event initialization options.
 */
USTRUCT(BlueprintType)
struct TAPCORE_API FTapTapEventOptions : public FTapSdkBaseOptions
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	bool bEnableTapTapEvent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	FString Channel;

	/** JSON object added to the initial device_login event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	FString PropertiesJson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	bool bOverrideBuiltInParameters = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	bool bEnableAutoIAPEvent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	bool bDisableAutoLogDeviceLogin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	FString CAID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	bool bEnableAdvertiserIDCollection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	FString OAIDCert;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore|Event")
	bool bDisableReflectionOAID = true;
};

/** Type-safe holder for any module-specific initialization options. */
using FTapSdkOption = TInstancedStruct<FTapSdkBaseOptions>;

/** Core TapSDK initialization options, aligned with Unity TapTapSdkOptions. */
USTRUCT(BlueprintType)
struct TAPCORE_API FTapTapSdkOptions
{
	GENERATED_BODY()

public:
	/** Client ID */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	FString ClientID;

	/** Client Token */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	FString ClientToken;

	/** Client Public Key for PC launcher verification (Win64 only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	FString ClientPublicKey;

	/** Region Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	ETapRegion Region = ETapRegion::CN;

	/** Preferred Language */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	ETapLanguage PreferredLanguage = ETapLanguage::Auto;

	/** Application version. Empty uses the platform application version. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	FString GameVersion;

	/** Enable Debug Log */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	bool bEnableLog = false;

	/** Screen Orientation (0=Portrait, 1=Landscape) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCore")
	int32 ScreenOrientation = 1;

private:
	friend class FTapSdkConfigState;

	TArray<FTapSdkOption> OtherOptions;

};
