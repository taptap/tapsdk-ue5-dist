// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * TapSDK HTTP Common Parameters Manager
 * 管理 HTTP 请求的公共参数（请求头和查询参数）
 */
class TAPCORE_API FTapHttpCommonParams
{
public:
	/**
	 * Initialize common parameters with client configuration
	 * @param InClientID Client ID from TapConfig
	 * @param InClientToken Client Token for signing
	 */
	static void Initialize(const FString& InClientID, const FString& InClientToken);

	/**
	 * Get common HTTP headers
	 * @param ModuleName SDK module name (e.g., "TapCore", "TapLogin")
	 * @param ModuleVersion SDK module version
	 * @param bEnableAuthorization Whether to include Authorization header
	 * @return Map of common headers
	 */
	static TMap<FString, FString> GetCommonHeaders(
		const FString& ModuleName,
		const FString& ModuleVersion,
		bool bEnableAuthorization = false
	);

	/**
	 * Get common query parameters
	 * @return Map of common query parameters
	 */
	static TMap<FString, FString> GetCommonQueryParams();

	/**
	 * Generate and add signature to headers
	 * @param Method HTTP method (GET, POST, etc.)
	 * @param URL Request URL
	 * @param Headers Request headers (will be modified to add signature)
	 * @param Body Request body (optional)
	 */
	static void AddSignature(
		const FString& Method,
		const FString& URL,
		TMap<FString, FString>& Headers,
		const FString& Body = TEXT("")
	);

	/**
	 * Set game user ID (optional)
	 * @param UserID Game user ID
	 */
	static void SetGameUserID(const FString& UserID);

	/**
	 * Clear game user ID
	 */
	static void ClearGameUserID();

	/**
	 * Set authorization token (for authenticated requests)
	 * @param Token Authorization token
	 */
	static void SetAuthorizationToken(const FString& Token);

	/**
	 * Clear authorization token
	 */
	static void ClearAuthorizationToken();

	/**
	 * Get the SDK-wide device id (delegates to FTapDeviceId).
	 */
	static FString GetDeviceID();

	/**
	 * Get SDK version
	 * @return SDK version string
	 */
	static FString GetSDKVersion();

	/** SDK User-Agent shared by HTTP requests and native module initialization. */
	static FString GenerateUserAgent();

	/**
	 * Get the client ID configured at Initialize() time.
	 * Used by modules (e.g. TapCompliance) that need to inject client_id
	 * manually into request URLs without enabling global common params.
	 */
	static const FString& GetClientID();

private:
	/** Client ID */
	static FString ClientID;

	/** Client Token (for signature) */
	static FString ClientToken;

	/** Game user ID (optional) */
	static FString GameUserID;

	/** Authorization token (optional) */
	static FString AuthorizationToken;

	/**
	 * Generate nonce (10-character random string)
	 * @return Random nonce string
	 */
	static FString GenerateNonce();

	/**
	 * Generate timestamp (seconds since epoch)
	 * @return Timestamp string
	 */
	static FString GenerateTimestamp();

	/**
	 * Get platform string
	 * @return Platform name (PC, iOS, Android)
	 */
	static FString GetPlatform();

	/**
	 * Get language code
	 * @return Language code (e.g., "zh_CN", "en_US")
	 */
	static FString GetLanguage();

	/**
	 * Generate HMAC-SHA256 signature
	 * @param SignData Data to sign
	 * @param Key Secret key
	 * @return Base64 encoded signature
	 */
	static FString GenerateHMACSHA256(const FString& SignData, const FString& Key);
};
