// Copyright TapTap. All Rights Reserved.

#include "TapCloudSaveAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapCloudSaveModule.h"
#include "Internal/Platform/TapCoreAndroid.h"
#include "Internal/TapAndroidCallbackId.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

namespace
{
	// -----------------------------------------------------------------------
	// Module-local state
	// -----------------------------------------------------------------------

	// Per-call (one-shot) callback registry for CRUD methods. Keeps callback
	// objects alive (global ref) and routes nativeOnResult deliveries back
	// to the C++ lambdas. CloudSave uses its own registry instead of TapCore's
	// because we also need a separate, never-removed entry for the persistent
	// state callback (registerCloudSaveCallback fires on every state change).
	struct FCloudSaveCallbackEntry
	{
		TapCloudSaveAndroidBridge::FResultCallback OneShot;     // for CRUD (consumed, then removed)
		TFunction<void(int32)>                     State;       // for register state (persistent)
		jobject                                    ProxyGlobal = nullptr;
		bool                                       bPersistent = false;
	};

	FCriticalSection                                 GCallbackLock;
	TMap<int64, FCloudSaveCallbackEntry>             GCallbackRegistry;

	// Single persistent state callback ID. The Router only allows one
	// registered TapCloudSaveCallback at a time (registeredCallback property
	// in TapCloudSaveUERouter is a single var, not a set), so we mirror that
	// 1-callback invariant here.
	int64                                            GStateCallbackId = 0;

	bool                                             GInitialized = false;

	// Cached routeToTapCloudSave jmethodID. jmethodID is process-stable per
	// JVM specification, so caching across calls is safe. Multiple threads
	// may race the initial resolve in InvokeRoute, but GetMethodID is
	// idempotent so the race is benign; we still use std::atomic to make the
	// pointer publication well-defined across threads (avoids torn reads on
	// platforms where pointer-sized writes are not implicitly atomic).
	std::atomic<jmethodID>                           GRouteMethodID{ nullptr };

	// -----------------------------------------------------------------------
	// JSON helpers
	// -----------------------------------------------------------------------

	FString CreateRouteRequest(const FString& MethodName, const FString& InnerParamsJson)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("method"), MethodName);
		if (!InnerParamsJson.IsEmpty())
		{
			// Router expects params as a JSON-string (string-valued field), not a nested object.
			Root->SetStringField(TEXT("params"), InnerParamsJson);
		}
		FString Out;
		auto Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root, Writer);
		return Out;
	}

	FString SerializeObject(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		auto Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	// -----------------------------------------------------------------------
	// Callback registry management
	// -----------------------------------------------------------------------

	int64 RegisterOneShotCallback(jobject ProxyGlobal, TapCloudSaveAndroidBridge::FResultCallback Cb)
	{
		FScopeLock Lock(&GCallbackLock);
		const int64 Id = FTapAndroidCallbackId::Allocate();
		FCloudSaveCallbackEntry E;
		E.OneShot      = MoveTemp(Cb);
		E.ProxyGlobal  = ProxyGlobal;
		E.bPersistent  = false;
		GCallbackRegistry.Add(Id, MoveTemp(E));
		return Id;
	}

	int64 RegisterPersistentStateCallback(jobject ProxyGlobal, TFunction<void(int32)> Cb)
	{
		FScopeLock Lock(&GCallbackLock);
		const int64 Id = FTapAndroidCallbackId::Allocate();
		FCloudSaveCallbackEntry E;
		E.State        = MoveTemp(Cb);
		E.ProxyGlobal  = ProxyGlobal;
		E.bPersistent  = true;
		GCallbackRegistry.Add(Id, MoveTemp(E));
		return Id;
	}

	// Returns true if we handled this ID (and removes one-shot entries).
	// Persistent entries stay registered and keep firing.
	bool DispatchCallback(int64 CallbackId, const FString& ResultJson)
	{
		// Extract a snapshot under the lock so the callback fires unlocked.
		bool                                              bIsPersistent = false;
		TapCloudSaveAndroidBridge::FResultCallback        OneShot;
		TFunction<void(int32)>                            State;
		jobject                                           ProxyToRelease = nullptr;
		{
			FScopeLock Lock(&GCallbackLock);
			FCloudSaveCallbackEntry* Entry = GCallbackRegistry.Find(CallbackId);
			if (Entry == nullptr) { return false; }

			bIsPersistent = Entry->bPersistent;
			if (bIsPersistent)
			{
				State = Entry->State;
			}
			else
			{
				OneShot        = MoveTemp(Entry->OneShot);
				ProxyToRelease = Entry->ProxyGlobal;
				GCallbackRegistry.Remove(CallbackId);
			}
		}

		if (bIsPersistent)
		{
			// Parse envelope to extract the int32 raw code from content.
			// State envelope is {"code":0,"content":<int>,"message":"success"}.
			TSharedPtr<FJsonObject> Root;
			auto Reader = TJsonReaderFactory<>::Create(ResultJson);
			int32 RawCode = -1;
			if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
			{
				// content may be a number or string depending on serializer choice — try both.
				double NumberContent = 0.0;
				FString StringContent;
				if (Root->TryGetNumberField(TEXT("content"), NumberContent))
				{
					RawCode = static_cast<int32>(NumberContent);
				}
				else if (Root->TryGetStringField(TEXT("content"), StringContent) && !StringContent.IsEmpty())
				{
					RawCode = FCString::Atoi(*StringContent);
				}
			}
			if (State)
			{
				State(RawCode);
			}
			return true;
		}

		// One-shot: invoke and release global ref.
		if (OneShot)
		{
			OneShot(true, ResultJson);
		}
		if (ProxyToRelease != nullptr)
		{
			if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
			{
				Env->DeleteGlobalRef(ProxyToRelease);
			}
		}
		return true;
	}

	// -----------------------------------------------------------------------
	// JNI routeToTapCloudSave invocation
	// -----------------------------------------------------------------------

	// Calls GameActivity#routeToTapCloudSave(String, EngineBridgeCallback)
	// with the supplied callback proxy. Returns the sync response JSON (which
	// is typically {"success":true,"async":true} on success, or a validation
	// error string on failure). If sync indicates failure, the caller should
	// surface that to the C++ result callback rather than wait forever.
	FString InvokeRoute(JNIEnv* Env, const FString& MethodName, const FString& InnerParamsJson, jobject CallbackProxy)
	{
		const FString RequestJson = CreateRouteRequest(MethodName, InnerParamsJson);

		jobject GameActivity = FAndroidApplication::GetGameActivityThis();
		if (GameActivity == nullptr)
		{
			return TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}");
		}

		jclass ActivityClass = Env->GetObjectClass(GameActivity);
		if (ActivityClass == nullptr || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
				UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at GetObjectClass(GameActivity)"));
			}
			if (ActivityClass != nullptr) { Env->DeleteLocalRef(ActivityClass); }
			return TEXT("{\"success\":false,\"message\":\"GetObjectClass(GameActivity) failed\"}");
		}

		// Cache routeToTapCloudSave jmethodID. jmethodIDs are process-stable;
		// a benign race on the first call is acceptable (GetMethodID is
		// idempotent and yields the same pointer). std::atomic ensures the
		// pointer publication is well-defined across threads.
		jmethodID RouteMethod = GRouteMethodID.load(std::memory_order_acquire);
		if (RouteMethod == nullptr)
		{
			RouteMethod = Env->GetMethodID(
				ActivityClass,
				"routeToTapCloudSave",
				"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
			);

			if (RouteMethod == nullptr || Env->ExceptionCheck())
			{
				if (Env->ExceptionCheck())
				{
					Env->ExceptionDescribe();
					Env->ExceptionClear();
					UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at GetMethodID(routeToTapCloudSave)"));
				}
				Env->DeleteLocalRef(ActivityClass);
				return TEXT("{\"success\":false,\"message\":\"routeToTapCloudSave method not found\"}");
			}
			GRouteMethodID.store(RouteMethod, std::memory_order_release);
		}

		jstring jRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
		if (jRequestJson == nullptr || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
				UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at NewStringUTF(RequestJson)"));
			}
			if (jRequestJson != nullptr) { Env->DeleteLocalRef(jRequestJson); }
			Env->DeleteLocalRef(ActivityClass);
			return TEXT("{\"success\":false,\"message\":\"NewStringUTF failed\"}");
		}

		jstring jResult = static_cast<jstring>(Env->CallObjectMethod(
			GameActivity, RouteMethod, jRequestJson, CallbackProxy));

		FString Out;
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at CallObjectMethod(routeToTapCloudSave)"));
			// jResult may be partially-set; drop any local ref defensively.
			if (jResult != nullptr) { Env->DeleteLocalRef(jResult); jResult = nullptr; }
			Out = TEXT("{\"success\":false,\"message\":\"JNI exception during routeToTapCloudSave\"}");
		}
		else if (jResult != nullptr)
		{
			const char* Chars = Env->GetStringUTFChars(jResult, nullptr);
			Out = FString(UTF8_TO_TCHAR(Chars));
			Env->ReleaseStringUTFChars(jResult, Chars);
			Env->DeleteLocalRef(jResult);
		}
		else
		{
			Out = TEXT("{\"success\":false,\"message\":\"routeToTapCloudSave returned null\"}");
		}

		Env->DeleteLocalRef(jRequestJson);
		Env->DeleteLocalRef(ActivityClass);
		return Out;
	}

	// Build a new EngineBridgeCallbackProxy bound to `CallbackId`. Returns a
	// new global ref. Caller is responsible for releasing the global ref via
	// DeleteGlobalRef when the corresponding registry entry is removed.
	jobject CreateProxyForId(JNIEnv* Env, int64 CallbackId)
	{
		jclass ProxyClass = FAndroidApplication::FindJavaClass(
			"com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy");
		if (ProxyClass == nullptr)
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
				UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at FindJavaClass(EngineBridgeCallbackProxy)"));
			}
			return nullptr;
		}

		jmethodID Ctor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
		if (Ctor == nullptr || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
				UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at GetMethodID(EngineBridgeCallbackProxy.<init>)"));
			}
			Env->DeleteLocalRef(ProxyClass);
			return nullptr;
		}

		jobject ProxyLocal = Env->NewObject(ProxyClass, Ctor, static_cast<jlong>(CallbackId));
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] JNI exception at NewObject(EngineBridgeCallbackProxy)"));
			if (ProxyLocal != nullptr) { Env->DeleteLocalRef(ProxyLocal); }
			Env->DeleteLocalRef(ProxyClass);
			return nullptr;
		}
		Env->DeleteLocalRef(ProxyClass);
		if (ProxyLocal == nullptr) { return nullptr; }

		jobject ProxyGlobal = Env->NewGlobalRef(ProxyLocal);
		Env->DeleteLocalRef(ProxyLocal);
		return ProxyGlobal;
	}

	// Returns true if the sync response indicates success (typical async kick-off).
	// `OutMessage` populated on failure for diagnostics.
	bool ParseSyncResponse(const FString& Json, FString& OutMessage)
	{
		TSharedPtr<FJsonObject> Root;
		auto Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutMessage = TEXT("Failed to parse route response JSON");
			return false;
		}
		bool bSuccess = false;
		if (!Root->TryGetBoolField(TEXT("success"), bSuccess))
		{
			OutMessage = TEXT("Response missing 'success' field");
			return false;
		}
		if (!bSuccess)
		{
			Root->TryGetStringField(TEXT("message"), OutMessage);
			if (OutMessage.IsEmpty()) { OutMessage = TEXT("Route returned success=false"); }
			return false;
		}
		return true;
	}

	// Common dispatch: build proxy + register one-shot + invoke route. If sync
	// fails, drop the registration and notify the caller with bHasResult=false.
	void DispatchOneShotRoute(
		const FString& MethodName,
		const FString& InnerParamsJson,
		TapCloudSaveAndroidBridge::FResultCallback OnResult)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (Env == nullptr)
		{
			if (OnResult) { OnResult(false, TEXT("")); }
			return;
		}

		// Reserve ID + register entry first so the Java proxy can be built with the right ID.
		// We'll patch in the global ref once it exists.
		int64 CallbackId;
		{
			FScopeLock Lock(&GCallbackLock);
			CallbackId = FTapAndroidCallbackId::Allocate();
			FCloudSaveCallbackEntry E;
			E.OneShot     = OnResult;
			E.bPersistent = false;
			GCallbackRegistry.Add(CallbackId, MoveTemp(E));
		}

		jobject ProxyGlobal = CreateProxyForId(Env, CallbackId);
		if (ProxyGlobal == nullptr)
		{
			FScopeLock Lock(&GCallbackLock);
			GCallbackRegistry.Remove(CallbackId);
			if (OnResult) { OnResult(false, TEXT("")); }
			return;
		}
		{
			FScopeLock Lock(&GCallbackLock);
			if (FCloudSaveCallbackEntry* E = GCallbackRegistry.Find(CallbackId))
			{
				E->ProxyGlobal = ProxyGlobal;
			}
		}

		const FString Sync = InvokeRoute(Env, MethodName, InnerParamsJson, ProxyGlobal);

		// Race guard: the SDK could (theoretically) fire the async callback
		// synchronously before InvokeRoute returns. In that case
		// DispatchCallback has already consumed the registry entry and
		// DeleteGlobalRef'd ProxyGlobal. Atomically test-and-remove the
		// entry here; if it's gone, async ownership has already cleaned
		// up — we must NOT touch ProxyGlobal nor fire OnResult again.
		bool bOwnsCleanup = false;
		{
			FScopeLock Lock(&GCallbackLock);
			if (GCallbackRegistry.Contains(CallbackId))
			{
				bOwnsCleanup = true;
			}
		}
		if (!bOwnsCleanup)
		{
			UE_LOG(LogTapCloudSave, Verbose,
				TEXT("[CloudSave-Android] %s async callback raced sync result; sync handling skipped"),
				*MethodName);
			return;
		}

		FString Message;
		if (!ParseSyncResponse(Sync, Message))
		{
			// We own cleanup: drop the registration, release the global ref,
			// and notify the caller synchronously.
			{
				FScopeLock Lock(&GCallbackLock);
				GCallbackRegistry.Remove(CallbackId);
			}
			Env->DeleteGlobalRef(ProxyGlobal);
			UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-Android] %s sync error: %s"),
				*MethodName, *Message);
			if (OnResult)
			{
				// Synthesize a failure envelope so the platform layer's parser
				// produces a usable error message. Build via FJsonObject so
				// quotes / backslashes in `Message` are escaped correctly.
				TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
				Inner->SetNumberField(TEXT("errorCode"), -1);
				Inner->SetStringField(TEXT("errorMessage"), Message);
				FString InnerStr;
				auto InnerWriter = TJsonWriterFactory<>::Create(&InnerStr);
				FJsonSerializer::Serialize(Inner, InnerWriter);

				TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
				Envelope->SetNumberField(TEXT("code"), -1);
				Envelope->SetStringField(TEXT("message"), TEXT("fail"));
				Envelope->SetStringField(TEXT("content"), InnerStr);
				FString EnvStr;
				auto EnvWriter = TJsonWriterFactory<>::Create(&EnvStr);
				FJsonSerializer::Serialize(Envelope, EnvWriter);

				OnResult(true, EnvStr);
			}
			return;
		}
		// Successful async kick-off: the registry entry stays alive until DispatchCallback fires.
	}
}

// =============================================================================
// Public API
// =============================================================================

void TapCloudSaveAndroidBridge::Init()
{
	if (GInitialized) { return; }
	GInitialized = true;

	// Register a handler with TapCore's global callback dispatcher. TapCore
	// owns the JNI native method (EngineBridgeCallbackProxy.nativeOnResult)
	// and will offer unknown callback IDs to every registered module until
	// one claims the ID.
	FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
	{
		// Quick check: is this an ID we own?
		{
			FScopeLock Lock(&GCallbackLock);
			if (!GCallbackRegistry.Find(CallbackId)) { return false; }
		}
		return DispatchCallback(CallbackId, ResultJson);
	});

	UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-Android] Bridge initialized"));
}

bool TapCloudSaveAndroidBridge::RegisterStateCallback(TFunction<void(int32)> OnRawState)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] RegisterStateCallback: JNIEnv null"));
		return false;
	}

	// If there is already a state callback, unregister it first so the Router
	// (which holds only one registeredCallback) replaces cleanly.
	UnregisterStateCallback();

	// Reserve ID + create proxy bound to it.
	int64 CallbackId;
	{
		FScopeLock Lock(&GCallbackLock);
		CallbackId = FTapAndroidCallbackId::Allocate();
		FCloudSaveCallbackEntry E;
		E.State        = MoveTemp(OnRawState);
		E.bPersistent  = true;
		GCallbackRegistry.Add(CallbackId, MoveTemp(E));
	}

	jobject ProxyGlobal = CreateProxyForId(Env, CallbackId);
	if (ProxyGlobal == nullptr)
	{
		FScopeLock Lock(&GCallbackLock);
		GCallbackRegistry.Remove(CallbackId);
		UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Android] RegisterStateCallback: proxy creation failed"));
		return false;
	}
	{
		FScopeLock Lock(&GCallbackLock);
		if (FCloudSaveCallbackEntry* E = GCallbackRegistry.Find(CallbackId))
		{
			E->ProxyGlobal = ProxyGlobal;
		}
		GStateCallbackId = CallbackId;
	}

	const FString Sync = InvokeRoute(Env, TEXT("registerCloudSaveCallback"), TEXT(""), ProxyGlobal);
	FString Message;
	if (!ParseSyncResponse(Sync, Message))
	{
		UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-Android] registerCloudSaveCallback sync error: %s"), *Message);
		{
			FScopeLock Lock(&GCallbackLock);
			GCallbackRegistry.Remove(CallbackId);
			GStateCallbackId = 0;
		}
		Env->DeleteGlobalRef(ProxyGlobal);
		return false;
	}

	UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-Android] State callback registered, ID=%lld"), CallbackId);
	return true;
}

void TapCloudSaveAndroidBridge::UnregisterStateCallback()
{
	// Cheap guard: avoid JNI work and a needless route call when no state
	// callback is currently registered (e.g. repeated unregister, or
	// unregister-before-register).
	{
		FScopeLock Lock(&GCallbackLock);
		if (GStateCallbackId == 0) { return; }
	}

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr) { return; }

	int64 IdToRelease = 0;
	jobject ProxyToRelease = nullptr;
	{
		FScopeLock Lock(&GCallbackLock);
		if (GStateCallbackId == 0) { /* raced with another unregister */ }
		else
		{
			IdToRelease = GStateCallbackId;
			if (FCloudSaveCallbackEntry* E = GCallbackRegistry.Find(IdToRelease))
			{
				ProxyToRelease = E->ProxyGlobal;
			}
			GCallbackRegistry.Remove(IdToRelease);
			GStateCallbackId = 0;
		}
	}

	// Ask the Router to unregister the underlying TapCloudSaveCallback.
	// (No callback proxy needed — unregisterCloudSaveCallback is sync.)
	const FString Sync = InvokeRoute(Env, TEXT("unregisterCloudSaveCallback"), TEXT(""), nullptr);
	FString Message;
	if (!ParseSyncResponse(Sync, Message))
	{
		UE_LOG(LogTapCloudSave, Verbose, TEXT("[CloudSave-Android] unregisterCloudSaveCallback returned: %s"), *Message);
	}

	if (ProxyToRelease != nullptr)
	{
		Env->DeleteGlobalRef(ProxyToRelease);
	}
	if (IdToRelease != 0)
	{
		UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-Android] State callback unregistered, ID=%lld"), IdToRelease);
	}
}

// -----------------------------------------------------------------------------
// CRUD wrappers — each serializes its inner params JSON, then defers to
// DispatchOneShotRoute. Field names mirror what TapCloudSaveUERouter expects.
// -----------------------------------------------------------------------------

void TapCloudSaveAndroidBridge::CreateArchive(const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("archiveMetadata"),   MetaJson);
	P->SetStringField(TEXT("archiveFilePath"),   FilePath);
	P->SetStringField(TEXT("archiveCoverPath"),  CoverPath);
	DispatchOneShotRoute(TEXT("createArchive"), SerializeObject(P), MoveTemp(OnResult));
}

void TapCloudSaveAndroidBridge::UpdateArchive(const FString& Uuid, const FString& MetaJson, const FString& FilePath, const FString& CoverPath, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("archiveUUID"),       Uuid);
	P->SetStringField(TEXT("archiveMetadata"),   MetaJson);
	P->SetStringField(TEXT("archiveFilePath"),   FilePath);
	P->SetStringField(TEXT("archiveCoverPath"),  CoverPath);
	DispatchOneShotRoute(TEXT("updateArchive"), SerializeObject(P), MoveTemp(OnResult));
}

void TapCloudSaveAndroidBridge::DeleteArchive(const FString& Uuid, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("archiveUUID"), Uuid);
	DispatchOneShotRoute(TEXT("deleteArchive"), SerializeObject(P), MoveTemp(OnResult));
}

void TapCloudSaveAndroidBridge::GetArchiveList(FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("getArchiveList"), TEXT(""), MoveTemp(OnResult));
}

void TapCloudSaveAndroidBridge::GetArchiveData(const FString& Uuid, const FString& FileId, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("archiveUUID"),  Uuid);
	P->SetStringField(TEXT("archiveFileID"), FileId);
	DispatchOneShotRoute(TEXT("getArchiveData"), SerializeObject(P), MoveTemp(OnResult));
}

void TapCloudSaveAndroidBridge::GetArchiveCover(const FString& Uuid, const FString& FileId, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("archiveUUID"),  Uuid);
	P->SetStringField(TEXT("archiveFileID"), FileId);
	DispatchOneShotRoute(TEXT("getArchiveCover"), SerializeObject(P), MoveTemp(OnResult));
}

#endif // PLATFORM_ANDROID
