// Copyright TapTap. All Rights Reserved.

// Use the compiler-native __APPLE__ macro as the outermost guard.
// PLATFORM_IOS is a header-defined macro (requires CoreMinimal.h), so it cannot
// safely appear in #if before CoreMinimal.h is included.  __APPLE__ is set by
// Apple clang (Mac/iOS) but NOT by the Android NDK clang used for Android
// cross-compilation, making this file produce truly empty output on Android
// without pulling in CoreMinimal.h (which in ObjC++ mode activates
// NSString/CFStringRef blocks in UnrealString.h.inl).
#ifdef __APPLE__
#include "TapLoginIOSBridge.h" // pulls in CoreMinimal.h, which defines PLATFORM_IOS

#if PLATFORM_IOS

#include "Async/Async.h"
#include "IOS/IOSAppDelegate.h"
#include "TapLoginModule.h"

THIRD_PARTY_INCLUDES_START
#import <TapTapLoginSDK/TapTapLoginSDK-Swift.h>
THIRD_PARTY_INCLUDES_END

namespace
{

FDelegateHandle GTapLoginOpenURLDelegateHandle;

FString ToFString(NSString* Value)
{
	return Value ? FString(Value) : FString();
}

FString MakeAccessTokenString(AccessToken* Token)
{
	if (Token == nil) {
		return FString();
	}

	TArray<FString> QueryParts;

	auto AppendPart = [&QueryParts](const TCHAR* Key, NSString* Value)
	{
		if (Value != nil && Value.length > 0) {
			QueryParts.Add(FString::Printf(TEXT("%s=%s"), Key, *FString(Value)));
		}
	};

	AppendPart(TEXT("kid"), Token.kid);
	AppendPart(TEXT("token_type"), Token.tokenType);
	AppendPart(TEXT("mac_key"), Token.macKey);
	AppendPart(TEXT("mac_algorithm"), Token.macAlgorithm);
	AppendPart(TEXT("scope"), Token.scope);

	if (Token.expiresIn >= 0) {
		QueryParts.Add(FString::Printf(TEXT("expires_in=%ld"), (long)Token.expiresIn));
	}

	return FString::Join(QueryParts, TEXT("&"));
}

bool HasValidAccount(TapTapAccount* Account)
{
	if (Account == nil || Account.userInfo == nil) {
		return false;
	}

	NSString* OpenID = Account.userInfo.openId;
	NSString* UnionID = Account.userInfo.unionId;
	return (OpenID != nil && OpenID.length > 0) || (UnionID != nil && UnionID.length > 0);
}

FTapAccount MakeAccount(TapTapAccount* NativeAccount)
{
	FTapAccount Account;
	AccessToken* Token = NativeAccount.accessToken;
	if (Token != nil) {
		Account.AccessToken.Kid = ToFString(Token.kid);
		Account.AccessToken.MacKey = ToFString(Token.macKey);
		Account.AccessToken.MacAlgorithm = ToFString(Token.macAlgorithm);
		Account.AccessToken.TokenType = ToFString(Token.tokenType);
		if (Token.scopeArray != nil) {
			for (NSString* Scope in Token.scopeArray) {
				const FString ScopeValue = ToFString(Scope);
				if (!ScopeValue.IsEmpty()) {
					Account.AccessToken.Scopes.Add(ScopeValue);
				}
			}
		} else {
			const FString ScopeString = ToFString(Token.scope);
			if (!ScopeString.IsEmpty()) {
				ScopeString.ParseIntoArray(Account.AccessToken.Scopes, TEXT(" "), true);
			}
		}
	}
	Account.OpenID = ToFString(NativeAccount.userInfo.openId);
	Account.UnionID = ToFString(NativeAccount.userInfo.unionId);
	Account.Name = ToFString(NativeAccount.userInfo.name);
	Account.Avatar = ToFString(NativeAccount.userInfo.avatar);
	Account.Email = NativeAccount.userInfo != nil ? ToFString(NativeAccount.userInfo.email) : TEXT("");
	return Account;
}

template <typename CallbackType>
void RunOnMainThread(CallbackType&& Callback)
{
	if ([NSThread isMainThread]) {
		Callback();
		return;
	}

	TSharedRef<TFunction<void()>, ESPMode::ThreadSafe> CallbackRef =
		MakeShared<TFunction<void()>, ESPMode::ThreadSafe>(Forward<CallbackType>(Callback));
	dispatch_async(dispatch_get_main_queue(), ^{
		(*CallbackRef)();
	});
}

template <typename CallbackType>
void RunSyncOnMainThread(CallbackType&& Callback)
{
	if ([NSThread isMainThread]) {
		Callback();
		return;
	}

	TFunction<void()> CallbackCopy(Forward<CallbackType>(Callback));
	dispatch_sync(dispatch_get_main_queue(), ^{
		CallbackCopy();
	});
}

void HandleOpenURL(UIApplication* Application, NSURL* URL, NSString* SourceApplication, id Annotation)
{
	(void)Application;
	(void)SourceApplication;
	(void)Annotation;

	if (URL == nil) {
		return;
	}

	const bool bHandled = [TapTapLogin openWithUrl:URL];
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] TapTapLogin openURL handled=%s url=%s"),
		bHandled ? TEXT("true") : TEXT("false"),
		*FString(URL.absoluteString));
}

} // namespace

void FTapLoginIOSBridge::RegisterOpenURLHandler()
{
	if (GTapLoginOpenURLDelegateHandle.IsValid()) {
		return;
	}

	GTapLoginOpenURLDelegateHandle = FIOSCoreDelegates::OnOpenURL.AddStatic(&HandleOpenURL);
}

void FTapLoginIOSBridge::UnregisterOpenURLHandler()
{
	if (!GTapLoginOpenURLDelegateHandle.IsValid()) {
		return;
	}

	FIOSCoreDelegates::OnOpenURL.Remove(GTapLoginOpenURLDelegateHandle);
	GTapLoginOpenURLDelegateHandle.Reset();
}

void FTapLoginIOSBridge::LoginWithScopes(const TArray<FString>& Scopes, TFunction<void(FTapLoginIOSResult)> Completion)
{
	TArray<FString> ScopeCopy = Scopes;
	TSharedRef<TFunction<void(FTapLoginIOSResult)>, ESPMode::ThreadSafe> CompletionRef =
		MakeShared<TFunction<void(FTapLoginIOSResult)>, ESPMode::ThreadSafe>(MoveTemp(Completion));

	RunOnMainThread([ScopeCopy = MoveTemp(ScopeCopy), CompletionRef]()
	{
		NSMutableArray<NSString*>* NativeScopes = [NSMutableArray arrayWithCapacity:ScopeCopy.Num()];
		for (const FString& Scope : ScopeCopy) {
			if (!Scope.IsEmpty()) {
				[NativeScopes addObject:Scope.GetNSString()];
			}
		}

		[TapTapLogin LoginWithScopes:NativeScopes handler:^(BOOL bIsCanceled, NSError* Error, TapTapAccount* Account) {
			FTapLoginIOSResult Result;
			Result.bCancelled = bIsCanceled;

			if (bIsCanceled) {
				Result.ErrorMessage = TEXT("Login cancelled");
			} else if (Error != nil) {
				Result.ErrorMessage = ToFString(Error.localizedDescription);
			} else if (HasValidAccount(Account)) {
				Result.bSuccess = true;
				Result.Account = MakeAccount(Account);
			} else {
				Result.ErrorMessage = TEXT("TapTapLogin returned an empty account");
			}

			AsyncTask(ENamedThreads::GameThread, [CompletionRef, Result = MoveTemp(Result)]() mutable
			{
				if (*CompletionRef) {
					(*CompletionRef)(MoveTemp(Result));
				}
			});
		}];
	});
}

bool FTapLoginIOSBridge::Logout(FString& OutErrorMessage)
{
	RunSyncOnMainThread(^{
		[TapTapLogin logout];
	});

	OutErrorMessage.Reset();
	return true;
}

TOptional<FTapAccount> FTapLoginIOSBridge::GetCurrentAccount(FString& OutErrorMessage)
{
	__block TOptional<FTapAccount> Account;

	RunSyncOnMainThread(^{
		TapTapAccount* NativeAccount = [TapTapLogin getCurrentTapAccount];
		if (HasValidAccount(NativeAccount)) {
			Account = MakeAccount(NativeAccount);
		}
	});

	OutErrorMessage.Reset();
	return Account;
}

#endif // PLATFORM_IOS
#endif // __APPLE__
