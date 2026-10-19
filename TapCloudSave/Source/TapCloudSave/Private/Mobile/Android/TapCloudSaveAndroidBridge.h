// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

// Thin C++ surface over the Android TapCloudSaveUERouter (Kotlin object in
// com.taptap.sdk.cloudsave.internal.ue). Mirrors the iOS bridge's namespace
// shape so the platform layer can stay symmetric across mobile platforms.
//
// Wire model (one JNI method on GameActivity, dispatch by JSON method name):
//   PlatformAndroid → AndroidBridge → JNI routeToTapCloudSave(reqJson, cb)
//                                       ↓ (Java)
//                                  TapCloudSaveUERouter.route(...)
//                                       ↓
//                                  TapTapCloudSave.<api>(... callback)
//                                       ↓ (later, on Android callback thread)
//                                  EngineBridgeCallbackProxy.nativeOnResult
//                                       ↓
//                                  TapCore's per-call callback registry
//                                       ↓
//                                  this bridge's FResultCallback fires
//
// Result-string convention (matches the Router's generateBridgeSuccess/Failure):
//   Success: {"code":0,  "content":"<archiveJson | base64 | arrayJson | int>", "message":"success"}
//   Error  : {"code":-1, "content":"{\"errorCode\":N,\"errorMessage\":\"…\"}", "message":"fail"}
namespace TapCloudSaveAndroidBridge
{
	// Idempotent. Registers the per-call dispatcher with TapCore so this
	// module's callbacks resolve from the global EngineBridgeCallbackProxy
	// callback registry.
	void Init();

	// `OnRawState` fires on whatever Android thread the SDK callback uses;
	// PlatformAndroid marshals to the GameThread. RawCode matches the
	// values from com.taptap.sdk.cloudsave.internal.TapCloudSaveResultCode.
	// Returns true only when the native route accepts registration.
	bool RegisterStateCallback(TFunction<void(int32 /*RawCode*/)> OnRawState);
	void UnregisterStateCallback();

	// Uniform result signature for every business API.
	//   bHasResult == true  → ResultJson holds the bridge JSON envelope.
	//   bHasResult == false → ResultJson is empty (route failed at JNI level,
	//                         or sync validation error was returned).
	using FResultCallback = TFunction<void(bool /*bHasResult*/, const FString& /*ResultJson*/)>;

	void CreateArchive(const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult);
	void UpdateArchive(const FString& Uuid, const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult);
	void DeleteArchive(const FString& Uuid, FResultCallback OnResult);
	void GetArchiveList(FResultCallback OnResult);
	void GetArchiveData (const FString& Uuid, const FString& FileId, FResultCallback OnResult);
	void GetArchiveCover(const FString& Uuid, const FString& FileId, FResultCallback OnResult);
}
