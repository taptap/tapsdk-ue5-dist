// Copyright TapTap. All Rights Reserved.

#include "TapLoginCryptoUtils.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Misc/Base64.h"
#include "Misc/SecureHash.h"

#if PLATFORM_MAC || PLATFORM_IOS
#include <CommonCrypto/CommonDigest.h>
#elif PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#endif

// SHA256 纯平台实现，不依赖 FPlatformMisc::GetSHA256Signature
static bool ComputeSHA256(const uint8* Data, int32 DataLen, uint8 OutHash[32])
{
#if PLATFORM_MAC || PLATFORM_IOS
	CC_SHA256(Data, DataLen, OutHash);
	return true;
#elif PLATFORM_WINDOWS
	BCRYPT_ALG_HANDLE hAlg = nullptr;
	BCRYPT_HASH_HANDLE hHash = nullptr;
	bool bSuccess = false;

	if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
	{
		if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0)))
		{
			if (BCRYPT_SUCCESS(BCryptHashData(hHash, const_cast<uint8*>(Data), DataLen, 0)))
			{
				if (BCRYPT_SUCCESS(BCryptFinishHash(hHash, OutHash, 32, 0)))
				{
					bSuccess = true;
				}
			}
			BCryptDestroyHash(hHash);
		}
		BCryptCloseAlgorithmProvider(hAlg, 0);
	}
	return bSuccess;
#else
	SHA256(Data, DataLen, OutHash);
	return true;
#endif
}

FString FTapLoginCryptoUtils::GenerateCodeVerifier()
{
	const FString BaseChars = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~");
	FString Result;
	Result.Reserve(128);

	for (int32 i = 0; i < 128; i++)
	{
		int32 Index = FMath::RandRange(0, BaseChars.Len() - 1);
		Result.AppendChar(BaseChars[Index]);
	}
	return Result;
}

FString FTapLoginCryptoUtils::GenerateCodeChallenge(const FString& Verifier)
{
	FTCHARToUTF8 Utf8Converter(*Verifier);
	const uint8* Data = reinterpret_cast<const uint8*>(Utf8Converter.Get());
	int32 DataLen = Utf8Converter.Length();

	uint8 Hash[32];
	ComputeSHA256(Data, DataLen, Hash);

	TArray<uint8> HashBytes;
	HashBytes.Append(Hash, 32);

	return Base64UrlEncodeNoPadding(HashBytes);
}

FString FTapLoginCryptoUtils::Base64UrlEncodeNoPadding(const TArray<uint8>& Data)
{
	FString Base64 = FBase64::Encode(Data.GetData(), Data.Num());
	Base64.ReplaceInline(TEXT("+"), TEXT("-"));
	Base64.ReplaceInline(TEXT("/"), TEXT("_"));
	Base64.ReplaceInline(TEXT("="), TEXT(""));
	return Base64;
}

FString FTapLoginCryptoUtils::GenerateMacSignature(
	const FString& Kid,
	const FString& InMacKey,
	const FString& InMacAlgorithm,
	const FString& Method,
	const FString& Uri,
	const FString& Host,
	const FString& Port,
	int64 Timestamp)
{
	FString Nonce = FString::FromInt(FMath::Rand());

	FString NormalizedString = FString::Printf(TEXT("%lld\n%s\n%s\n%s\n%s\n%s\n\n"),
		Timestamp, *Nonce, *Method, *Uri, *Host, *Port);

	FTCHARToUTF8 KeyConverter(*InMacKey);
	FTCHARToUTF8 DataConverter(*NormalizedString);

	uint8 HmacDigest[20]; // SHA1 = 20 bytes
	FSHA1::HMACBuffer(
		KeyConverter.Get(), KeyConverter.Length(),
		DataConverter.Get(), DataConverter.Length(),
		HmacDigest
	);

	FString Mac = FBase64::Encode(HmacDigest, 20);

	return FString::Printf(TEXT("id=\"%s\",ts=\"%lld\",nonce=\"%s\",mac=\"%s\""),
		*Kid, Timestamp, *Nonce, *Mac);
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
