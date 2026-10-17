// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * PKCE + MAC 加密工具（提取自 TapLoginPlatformDesktop）
 * 参照 Unity CodeUtil.cs + LoginService.GetAuthorizationHeader
 */
class FTapLoginCryptoUtils
{
public:
	// PKCE: 128 chars URL-safe random string (参照 Unity CodeUtil.GenerateCodeVerifier)
	static FString GenerateCodeVerifier();

	// PKCE: SHA256 + Base64URL (参照 Unity CodeUtil.GetCodeChallenge)
	static FString GenerateCodeChallenge(const FString& Verifier);

	// Base64 → Base64URL (no padding)
	static FString Base64UrlEncodeNoPadding(const TArray<uint8>& Data);

	// MAC Token 签名 (参照 Unity LoginService.GetAuthorizationHeader)
	static FString GenerateMacSignature(
		const FString& Kid,
		const FString& InMacKey,
		const FString& InMacAlgorithm,
		const FString& Method,
		const FString& Uri,
		const FString& Host,
		const FString& Port,
		int64 Timestamp
	);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
