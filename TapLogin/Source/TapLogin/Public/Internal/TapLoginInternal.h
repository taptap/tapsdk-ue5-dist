#pragma once
#include "TapLogin.h"

struct TAPLOGIN_API FTapAccessTokenResult
{
	bool bSuccess = false;
	bool bCancelled = false;
	int32 ErrorCode = 0;
	FString ErrorMessage;
	FTapAccessToken Token;
};

using FTapAccessTokenCallback = TFunction<void(const FTapAccessTokenResult& Result)>;


/** Internal module-to-module and test support. Not part of the game developer facade. */
class TAPLOGIN_API FTapLoginInternal
{
public:
	static void Initialize(){FTapTapLogin::Initialize();}
	static bool IsInitialized(){return FTapTapLogin::IsInitialized();}
	static void BroadcastLoginStateChanged(const TOptional<FTapAccount>& Account){FTapTapLogin::BroadcastLoginStateChanged(Account);}
	static FTapLoginStateChangedDelegate& OnLoginStateChanged(){return FTapTapLogin::OnLoginStateChanged();}
	/** Same account source as GetCurrentTapAccount. Uses CheckLocalInitialize so InitProgress does not toast. */
	static TOptional<FTapAccount> PeekCachedAccount(){return FTapTapLogin::PeekCachedAccount();}
	static FString GetVersion(){return FTapTapLogin::GetVersion();}
	static bool HasScope(const FString&S){return FTapTapLogin::HasScope(S);}
	static TArray<FString> GetCurrentScopes(){return FTapTapLogin::GetCurrentScopes();}
	static FString BuildAuthorizationHeader(const FString&M,const FString&U,const FString&H,const FString&P,int64 T){return FTapTapLogin::BuildAuthorizationHeader(M,U,H,P,T);}
	static FString BuildAuthorizationHeaderForToken(const FTapAccessToken&Token,const FString&M,const FString&U,const FString&H,const FString&P,int64 T){return FTapTapLogin::BuildAuthorizationHeaderForToken(Token,M,U,H,P,T);}
	static void AuthorizeForAccessToken(const TArray<ETapLoginScope>& Scopes, FTapAccessTokenCallback Callback);
	static void Authorize(const TArray<ETapLoginScope>&S,TScriptInterface<ITapLoginCallback>C){FTapTapLogin::Authorize(S,C);}
#if PLATFORM_WINDOWS
	static void HandleWindowsDeepLinkURL(const FString& URL);
#endif
#if WITH_DEV_AUTOMATION_TESTS
	static void InjectTokenForTest(const FString&K,const FString&A,const FString&M,const FString&Algo,const TArray<FString>&S){FTapTapLogin::InjectTokenForTest(K,A,M,Algo,S);}
#endif
};
