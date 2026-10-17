// Copyright TapTap. All Rights Reserved.

#include "TapCloudSaveIOSBridge.h"

#import <Foundation/Foundation.h>
#import "BridgeCloudSaveService.h"

#include "TapCloudSaveModule.h"
#include "HAL/CriticalSection.h"

// =============================================================================
// Upstream compatibility shim — TapSdkCloudSaveInitLogger
// =============================================================================
// Some static cloudsave_sdk distributions omit TapSdkCloudSaveInitLogger.
// This weak fallback keeps those distributions linkable. Native log forwarding
// remains disabled until a distribution exporting the logger entry point is used.
extern "C" {
	typedef void (*TapSdkCppLogWriter_Shim)(int32_t logLevel,
											 const char* codeLocation,
											 const char* logTag,
											 const char* logMessage);
	__attribute__((visibility("default")))
	__attribute__((weak))
	void TapSdkCloudSaveInitLogger(TapSdkCppLogWriter_Shim /*writer*/)
	{
		// no-op
	}
}

namespace
{
	// --- Marshalling helpers -------------------------------------------------

	NSString* ToNSString(const FString& In)
	{
		return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*In)];
	}

	// Returns nil for empty paths so the iOS SDK can treat "no cover" as
	// nullable (the bridge signature marks coverPath as _Nullable).
	NSString* ToNSStringOrNil(const FString& In)
	{
		return In.IsEmpty() ? nil : ToNSString(In);
	}

	FString FromNSString(NSString* In)
	{
		return In ? FString(UTF8_TO_TCHAR([In UTF8String])) : FString();
	}

	// Explicit block typedef ensures ARC strong-retains and heap-copies the
	// returned stack block (auto deduction would keep it on the stack and
	// trigger -Wreturn-stack-address under iOS clang).
	typedef void (^FBridgeBlock)(NSString* _Nullable);

	// Single point where we materialise the C++ FResultCallback from an
	// ObjC block. The iOS bridge promises the block runs on a callback
	// queue (not necessarily the main thread), so PlatformIOS must marshal
	// any UE delegate dispatch onto the GameThread itself.
	FBridgeBlock MakeBridgeBlock(TapCloudSaveIOSBridge::FResultCallback OnResult)
	{
		// Move into a __block-captured shared ptr so the TFunction outlives
		// any potential ARC retain cycle and is destroyed once after firing.
		__block TapCloudSaveIOSBridge::FResultCallback Cb = MoveTemp(OnResult);
		FBridgeBlock StackBlock = ^(NSString* _Nullable Result)
		{
			if (!Cb) { return; }
			const bool bHas = (Result != nil);
			Cb(bHas, FromNSString(Result));
			Cb = nullptr; // release after first invocation; matches OnceTime=true on Unity side
		};
		// Explicit -copy moves the block from stack to heap; required when
		// returning by value from a function (some clang versions don't
		// auto-promote even with an explicit block typedef return type).
		return [StackBlock copy];
	}

	// --- State-callback plumbing --------------------------------------------

	FCriticalSection                 GStateLock;
	TFunction<void(int32)>           GStateCallback;
	bool                             GInitialized = false;
}

namespace TapCloudSaveIOSBridge
{
	void Init()
	{
		FScopeLock Lock(&GStateLock);
		if (GInitialized) { return; }
		GInitialized = true;

		// Register a permanent native callback that forwards into whatever
		// state callback PlatformIOS currently has registered. The native
		// bridge replaces its previous callback every time, but we only
		// hand it ours once.
		//
		// Result JSON (from BridgeCloudSaveService.m onResult:):
		//   {"code":0, "content":"<resultCodeAsString>", "message":"Success"}
		[BridgeCloudSaveService registerCloudSaveCallback:^(NSString* _Nonnull Result)
		{
			const FString Json = FromNSString(Result);
			int32 RawCode = -1;

			// Cheap parse: look for "content":"<int>". A full FJsonObject
			// parse is also fine but adds a header dependency to this .mm
			// for no real benefit — the format is fixed by the iOS SDK.
			const FString Marker = TEXT("\"content\":\"");
			const int32 Start = Json.Find(Marker);
			if (Start != INDEX_NONE)
			{
				const int32 ValStart = Start + Marker.Len();
				const int32 ValEnd   = Json.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValStart);
				if (ValEnd != INDEX_NONE)
				{
					const FString CodeStr = Json.Mid(ValStart, ValEnd - ValStart);
					RawCode = FCString::Atoi(*CodeStr);
				}
			}

			TFunction<void(int32)> Snapshot;
			{
				FScopeLock Inner(&GStateLock);
				Snapshot = GStateCallback;
			}
			if (Snapshot) { Snapshot(RawCode); }
		}];

		UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-iOS] BridgeCloudSaveService registered"));
	}

	void RegisterStateCallback(TFunction<void(int32)> OnRawState)
	{
		FScopeLock Lock(&GStateLock);
		GStateCallback = MoveTemp(OnRawState);
	}

	void UnregisterStateCallback()
	{
		FScopeLock Lock(&GStateLock);
		GStateCallback = nullptr;
	}

	// --- Business APIs -------------------------------------------------------

	void CreateArchive(const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult)
	{
		[BridgeCloudSaveService
			archiveMetadata:ToNSString(MetaJson)
			archiveFilePath:ToNSString(FilePath)
			archiveCoverPath:ToNSStringOrNil(CoverPath)
			bridgeCallback:MakeBridgeBlock(MoveTemp(OnResult))];
	}

	void UpdateArchive(const FString& Uuid, const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult)
	{
		[BridgeCloudSaveService
			archiveUUIDForUpdate:ToNSString(Uuid)
			archiveMetadataForUpdate:ToNSString(MetaJson)
			archiveFilePathForUpdate:ToNSString(FilePath)
			archiveCoverPathForUpdate:ToNSStringOrNil(CoverPath)
			bridgeCallback:MakeBridgeBlock(MoveTemp(OnResult))];
	}

	void DeleteArchive(const FString& Uuid, FResultCallback OnResult)
	{
		[BridgeCloudSaveService
			archiveUUID:ToNSString(Uuid)
			bridgeCallback:MakeBridgeBlock(MoveTemp(OnResult))];
	}

	void GetArchiveList(FResultCallback OnResult)
	{
		[BridgeCloudSaveService getArchiveList:MakeBridgeBlock(MoveTemp(OnResult))];
	}

	void GetArchiveData(const FString& Uuid, const FString& FileId, FResultCallback OnResult)
	{
		[BridgeCloudSaveService
			archiveUUID:ToNSString(Uuid)
			archiveFileID:ToNSString(FileId)
			bridgeCallback:MakeBridgeBlock(MoveTemp(OnResult))];
	}

	void GetArchiveCover(const FString& Uuid, const FString& FileId, FResultCallback OnResult)
	{
		[BridgeCloudSaveService
			archiveUUIDForCover:ToNSString(Uuid)
			archiveFileIDForCover:ToNSString(FileId)
			bridgeCallback:MakeBridgeBlock(MoveTemp(OnResult))];
	}
}
