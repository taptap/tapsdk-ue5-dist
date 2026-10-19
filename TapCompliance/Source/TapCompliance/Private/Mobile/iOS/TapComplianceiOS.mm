// Copyright TapTap. All Rights Reserved.

#include "Platform/TapComplianceiOS.h"

#if PLATFORM_IOS

#include "TapCompliance.h"
#include "Async/Async.h"
#include "IOS/IOSAppDelegate.h"

// Suppress Objective-C warnings
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-property-no-attribute"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wnonnull"

// Import Pure Objective-C bridge
#import "TapComplianceIOSBridge.h"

#pragma clang diagnostic pop

DEFINE_LOG_CATEGORY_STATIC(LogTapComplianceiOS, Log, All);

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapComplianceiOS::FTapComplianceiOS()
	: CallbackAlive(MakeShared<TAtomic<bool>>(true))
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] FTapComplianceiOS created"));
}

FTapComplianceiOS::~FTapComplianceiOS()
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] FTapComplianceiOS destroyed"));
	InvalidateCallbacks();
	UnregisterComplianceCallback();
}

// =============================================================================
// ITapCompliancePlatform Interface Implementation
// =============================================================================

void FTapComplianceiOS::ApplyInitOptions(const FTapComplianceOptions& Options)
{
	const BOOL bShowSwitchAccount = Options.bShowSwitchAccount ? YES : NO;
	const BOOL bUseAgeRange = Options.bUseAgeRange ? YES : NO;
	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge setShowSwitchAccount:bShowSwitchAccount];
		[TapComplianceIOSBridge setUseAgeRange:bUseAgeRange];
	});
}

void FTapComplianceiOS::RegisterComplianceCallback(TFunction<void(int32, const FString&)> Callback)
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] RegisterComplianceCallback called"));
	CallbackAlive->Store(true);

	// 1. 存储 C++ 回调
	{
		FScopeLock Lock(&ComplianceCallbackLock);
		ComplianceEventCallback = Callback;
	}

	// 2. 向 iOS SDK 注册 ObjC 回调。只捕获共享存活标记和回调副本，避免持有裸 this。
	TSharedRef<TAtomic<bool>> CallbackAliveRef = CallbackAlive;
	TFunction<void(int32, const FString&)> RegisteredCallback = Callback;

	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge registerComplianceCallback:^(NSInteger code, NSString* extraJson) {
			FString ExtraJsonStr = extraJson ? FString(extraJson) : FString();
			UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] Compliance event received: Code=%d, Extra=%s"),
				(int32)code, *ExtraJsonStr);

			// 在 Game Thread 上执行 C++ 回调
			AsyncTask(ENamedThreads::GameThread, [CallbackAliveRef, RegisteredCallback, code, ExtraJsonStr]()
			{
				if (CallbackAliveRef->Load() && RegisteredCallback)
				{
					RegisteredCallback((int32)code, ExtraJsonStr);
				}
			});
		}];
	});

	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] Compliance callback registered"));
}

void FTapComplianceiOS::UnregisterComplianceCallback()
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] UnregisterComplianceCallback called"));

	// 1. 清除 C++ 侧持久回调
	{
		FScopeLock Lock(&ComplianceCallbackLock);
		ComplianceEventCallback = nullptr;
	}

	// 2. 注销 iOS SDK 回调
	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge unregisterComplianceCallback];
	});
}

void FTapComplianceiOS::Startup(const FString& UserId)
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("========================================"));
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] Startup: UserId=%s"), *UserId);

	NSString* nsUserId = UserId.GetNSString();

	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge startupWithUserId:nsUserId];
	});

	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] Startup initiated, compliance events will arrive via delegate callback"));
	UE_LOG(LogTapComplianceiOS, Log, TEXT("========================================"));
}

void FTapComplianceiOS::Exit()
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] Exit called"));

	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge exit];
	});
}

void FTapComplianceiOS::CheckPaymentLimit(
	int64 Amount,
	TFunction<void(int32, const FString&, const FString&)> Callback,
	TFunction<void(const FString&)> OnError)
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("========================================"));
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] CheckPaymentLimit: Amount=%lld"), Amount);

	NSInteger nsAmount = (NSInteger)Amount;

	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge checkPaymentLimit:nsAmount completion:^(BOOL allowed, NSString* _Nullable title, NSString* _Nullable description) {

			FString TitleStr = title ? FString(title) : FString();
			FString DescStr = description ? FString(description) : FString();

			UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] CheckPaymentLimit callback: Allowed=%s, Title=%s"),
				allowed ? TEXT("true") : TEXT("false"), *TitleStr);

			// 在 Game Thread 上执行 C++ 回调
			AsyncTask(ENamedThreads::GameThread, [Callback, allowed, TitleStr, DescStr]()
			{
				if (Callback) Callback(allowed ? 1 : 0, TitleStr, DescStr);
			});
		} failure:^(NSString* _Nullable error) {
			const FString Message = error ? FString(error) : TEXT("Payment check failed");
			AsyncTask(ENamedThreads::GameThread, [OnError, Message]()
			{
				if (OnError) OnError(Message);
			});
		}];
	});

	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] CheckPaymentLimit request sent, waiting for callback..."));
	UE_LOG(LogTapComplianceiOS, Log, TEXT("========================================"));
}

void FTapComplianceiOS::SubmitPayment(
	int64 Amount,
	TFunction<void(bool, const FString&)> Callback)
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] SubmitPayment: Amount=%lld"), Amount);

	NSInteger nsAmount = (NSInteger)Amount;

	dispatch_async(dispatch_get_main_queue(), ^{
		[TapComplianceIOSBridge submitPayment:nsAmount completion:^(BOOL success, NSString* message) {

			FString MessageStr = message ? FString(message) : FString();

			UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] SubmitPayment callback: Success=%s"),
				success ? TEXT("true") : TEXT("false"));

			AsyncTask(ENamedThreads::GameThread, [Callback, success, MessageStr]()
			{
				Callback(success ? true : false, MessageStr);
			});
		}];
	});
}

int32 FTapComplianceiOS::GetAgeRange()
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] GetAgeRange called"));

	NSInteger result = [TapComplianceIOSBridge ageRange];
	int32 AgeRange = (int32)result;

	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] GetAgeRange: %d"), AgeRange);
	return AgeRange;
}

int32 FTapComplianceiOS::GetUserRemainTime()
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] GetUserRemainTime called"));

	NSInteger result = [TapComplianceIOSBridge remainingTime];
	int32 RemainTime = (int32)result;

	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] GetUserRemainTime: %d seconds"), RemainTime);
	return FMath::Max(0, RemainTime);
}

FString FTapComplianceiOS::GetCurrentAccessToken()
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] GetCurrentAccessToken called"));
	NSString* Token = [TapComplianceIOSBridge currentAccessToken];
	return Token ? FString(Token) : FString();
}

// =============================================================================
// Event Dispatcher - called from ObjC block on Game Thread
// =============================================================================

void FTapComplianceiOS::InvalidateCallbacks()
{
	CallbackAlive->Store(false);
}

void FTapComplianceiOS::DispatchComplianceEvent(int32 Code, const FString& ExtraJson)
{
	UE_LOG(LogTapComplianceiOS, Log, TEXT("========================================"));
	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] DispatchComplianceEvent: Code=%d, Extra=%s"), Code, *ExtraJson);

	TFunction<void(int32, const FString&)> Callback;
	{
		FScopeLock Lock(&ComplianceCallbackLock);
		Callback = ComplianceEventCallback;
	}

	if (!Callback)
	{
		UE_LOG(LogTapComplianceiOS, Warning,
			TEXT("[iOS] No compliance callback registered, event dropped: Code=%d"), Code);
		return;
	}

	UE_LOG(LogTapComplianceiOS, Log, TEXT("[iOS] Executing compliance callback: Code=%d"), Code);
	Callback(Code, ExtraJson);

	UE_LOG(LogTapComplianceiOS, Log, TEXT("========================================"));
}

#endif // PLATFORM_IOS
