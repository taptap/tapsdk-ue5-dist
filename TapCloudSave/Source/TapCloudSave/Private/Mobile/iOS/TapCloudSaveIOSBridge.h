// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

// Thin C++ surface over the iOS `BridgeCloudSaveService` ObjC class
// (vendored in TapTapCloudSaveSDK.xcframework). Implemented in .mm so
// PlatformIOS.cpp can stay pure C++ if ever needed.
//
// Result-string convention (matches BridgeCloudSaveCallback.m):
//   Success: {"code":0,  "content":"<archiveJson | base64 | arrayJson>", "message":""}
//   Error  : {"code":-1, "content":"{\"errorCode\":N,\"errorMessage\":\"…\"}", "message":""}
// The bridge always forwards the raw JSON string up — PlatformIOS decides
// success/failure and decodes binary content.
namespace TapCloudSaveIOSBridge
{
	// Idempotent. Registers the state-callback bridge with
	// BridgeCloudSaveService once; subsequent calls replace the cached
	// raw-state callback (the iOS bridge itself replaces its previous
	// callback on every registerCloudSaveCallback: invocation).
	void Init();

	// `OnRawState` fires on the iOS callback thread; PlatformIOS marshals
	// to the GameThread. RawCode mirrors `TapCloudSaveResultCode` Unity
	// constants (e.g. 300001 NEED_LOGIN).
	void RegisterStateCallback(TFunction<void(int32 /*RawCode*/)> OnRawState);
	void UnregisterStateCallback();

	// Uniform result signature for every business API.
	//   bHasResult == true  → ResultJson holds the bridge JSON envelope.
	//   bHasResult == false → ResultJson is empty (iOS bridge passed nil).
	using FResultCallback = TFunction<void(bool /*bHasResult*/, const FString& /*ResultJson*/)>;

	void CreateArchive(const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult);
	void UpdateArchive(const FString& Uuid, const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult);
	void DeleteArchive(const FString& Uuid, FResultCallback OnResult);
	void GetArchiveList(FResultCallback OnResult);
	void GetArchiveData (const FString& Uuid, const FString& FileId, FResultCallback OnResult);
	void GetArchiveCover(const FString& Uuid, const FString& FileId, FResultCallback OnResult);
}
