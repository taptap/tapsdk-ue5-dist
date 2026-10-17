// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "UObject/Interface.h"
#include "TapLogin.generated.h"

/**
 * 登录权限范围
 */
UENUM(BlueprintType)
enum class ETapLoginScope : uint8
{
	BasicInfo,          // 基本信息
	PublicProfile,      // 公开资料
	Email,              // 邮箱
	UserFriends,        // 好友列表
	Compliance,         // 合规
	ComplianceBasic     // 基础合规
};

/**
 * TapTap AccessToken data used for MAC signing.
 * This mirrors Unity Standalone IAuthorizationProvider.Authorize: callers get a
 * token for a single authorization flow without creating/updating login state.
 */
USTRUCT(BlueprintType)
struct TAPLOGIN_API FTapAccessToken
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Kid;

	UPROPERTY(BlueprintReadOnly)
	FString TokenType;

	UPROPERTY(BlueprintReadOnly)
	FString MacKey;

	UPROPERTY(BlueprintReadOnly)
	FString MacAlgorithm;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Scopes;

	// MAC signing only needs kid + mac credentials, matching Unity's AccessToken model.
	// The raw OAuth access_token is not part of this struct.
	bool IsValidForMac() const
	{
		return !Kid.IsEmpty() && !MacKey.IsEmpty() && !MacAlgorithm.IsEmpty();
	}

	bool HasScope(const FString& Scope) const
	{
		if (Scope.IsEmpty()) return false;
		for (const FString& S : Scopes)
		{
			if (S.Equals(Scope))
			{
				return true;
			}
		}
		return false;
	}
};

struct FTapAccount;
DECLARE_MULTICAST_DELEGATE_OneParam(FTapLoginStateChangedDelegate, const TOptional<FTapAccount>&);

/**
 * 登录回调接口
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UTapLoginCallback : public UInterface
{
	GENERATED_BODY()
};

class ITapLoginCallback
{
	GENERATED_BODY()

public:
	/**
	 * 登录成功回调。账号对象与 Unity、Android、iOS 的登录结果语义对齐。
	 * 需要兼容 MAC Token 或完整用户资料时，直接使用 Account 中的字段，
	 * 不需要再调用 GetCurrentTapAccount()。
	 */
	virtual void OnLoginSuccess(const FTapAccount& Account) = 0;

	/**
	 * 登录失败回调
	 * @param ErrorCode - 错误码；登录失败统一使用 -1，具体原因通过 ErrorMessage 表达
	 * @param ErrorMessage - 与 Unity/Android Exception message 对齐的错误信息
	 */
	virtual void OnLoginFailed(int32 ErrorCode, const FString& ErrorMessage) = 0;

	/**
	 * 登录取消回调
	 */
	virtual void OnLoginCancelled() = 0;
};

/**
 * TapTap 登录账号信息
 */
USTRUCT(BlueprintType)
struct TAPLOGIN_API FTapAccount
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FTapAccessToken AccessToken;

	UPROPERTY(BlueprintReadOnly)
	FString OpenID;

	UPROPERTY(BlueprintReadOnly)
	FString UnionID;

	UPROPERTY(BlueprintReadOnly)
	FString Name;

	UPROPERTY(BlueprintReadOnly)
	FString Avatar;

	UPROPERTY(BlueprintReadOnly)
	FString Email;

	FTapAccount()
		: AccessToken()
		, OpenID(TEXT(""))
		, UnionID(TEXT(""))
		, Name(TEXT(""))
		, Avatar(TEXT(""))
		, Email(TEXT(""))
	{
	}
};

/**
 * TapTap 登录 API
 */
class TAPLOGIN_API FTapTapLogin
{
public:
	/**
	 * 使用指定权限登录
	 * 对应 Android SDK: TapTapLogin.loginWithScopes()
	 * @param Scopes - 权限列表
	 * @param Callback - 登录回调；成功时通过 OnLoginSuccess 返回完整 FTapAccount
	 */
	static void LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback);

	/**
	 * 登出
	 * 对应 Android SDK: TapTapLogin.logout()
	 */
	static void Logout();

	/**
	 * 获取当前登录账号
	 * 对应 Android SDK: TapTapLogin.getCurrentTapAccount()
	 * @return 当前账号信息，如果未登录返回空
	 */
	static TOptional<FTapAccount> GetCurrentTapAccount();

	/**
	 * 用当前 AccessToken 的 MAC 字段构造 MAC 授权头（含 "MAC " 前缀，无 "Authorization: " 前缀）
	 * @param Method     HTTP 方法
	 * @param Uri        请求 URI 含 query
	 * @param Host       主机名
	 * @param Port       端口字符串
	 * @param Timestamp  Unix 秒；传 0 时使用本地时间
	 * @return 已签名头值；未登录或无密钥时返回空字符串
	 */
	/**
	 * Build a MAC Authorization header from an explicit AccessToken.
	 * Used by Compliance FastAuth to match Unity's token-only authorization path.
	 */
	/**
	 * Token-only authorization.
	 * Unlike LoginWithScopes/Authorize, this returns AccessToken fields only and
	 * must not create/update CurrentAccount or persisted login state.
	 */
	/**
	 * 升级 scope（不重复弹 UI）
	 * 已登录且 token 已包含全部 Scopes → 立刻 OnLoginSuccess；否则等价 LoginWithScopes。
	 */
private:
	friend class FTapLoginInternal;
	static FTapLoginStateChangedDelegate& OnLoginStateChanged();
	static bool IsInitialized();
	static FString GetVersion();
	static void BroadcastLoginStateChanged(const TOptional<FTapAccount>& Account);
	static void Initialize();
	static TOptional<FTapAccount> ReadCurrentAccount();
	static TOptional<FTapAccount> PeekCachedAccount();
	static bool HasScope(const FString& Scope);
	static TArray<FString> GetCurrentScopes();
	static FString BuildAuthorizationHeader(const FString&,const FString&,const FString&,const FString&,int64);
	static FString BuildAuthorizationHeaderForToken(const FTapAccessToken&,const FString&,const FString&,const FString&,const FString&,int64);
	static void Authorize(const TArray<ETapLoginScope>&,TScriptInterface<ITapLoginCallback>);
#if WITH_DEV_AUTOMATION_TESTS
	static void InjectTokenForTest(const FString&,const FString&,const FString&,const FString&,const TArray<FString>&);
#endif
	// 当前登录账号
	static TOptional<FTapAccount> CurrentAccount;
	static FCriticalSection CurrentAccountLock;

	// 初始化状态
	static bool bIsInitialized;

	// 平台实现实例
	static TSharedPtr<class ITapLoginPlatform> PlatformImpl;
};
