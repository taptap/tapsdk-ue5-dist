// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpCommonParams.h"
#include "Internal/TapLogPrivacy.h"
#include "Internal/Http/TapHttpTime.h"
#include "Internal/Storage/TapDeviceId.h"
#include "Misc/SecureHash.h"
#include "Misc/Base64.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Culture.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "EncryptionContextOpenSSL.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapCommonParams, Log, All);

// Static member initialization
FString FTapHttpCommonParams::ClientID;
FString FTapHttpCommonParams::ClientToken;
FString FTapHttpCommonParams::GameUserID;
FString FTapHttpCommonParams::AuthorizationToken;

void FTapHttpCommonParams::Initialize(const FString& InClientID, const FString& InClientToken)
{
	ClientID = InClientID;
	ClientToken = InClientToken;

	UE_LOG(LogTapCommonParams, Log, TEXT("Initialized common params with ClientID: %s"), *ClientID);
}

const FString& FTapHttpCommonParams::GetClientID()
{
	return ClientID;
}

TMap<FString, FString> FTapHttpCommonParams::GetCommonHeaders(
	const FString& ModuleName,
	const FString& ModuleVersion,
	bool bEnableAuthorization)
{
	TMap<FString, FString> Headers;

	// 固定参数
	Headers.Add(TEXT("X-Tap-PN"), TEXT("TapSDK"));
	Headers.Add(TEXT("X-Tap-Device-Id"), GetDeviceID());
	Headers.Add(TEXT("X-Tap-Platform"), GetPlatform());
	Headers.Add(TEXT("X-Tap-SDK-Module"), ModuleName);
	Headers.Add(TEXT("X-Tap-SDK-Module-Version"), ModuleVersion);
	Headers.Add(TEXT("X-Tap-SDK-Artifact"), TEXT("UE5"));
	Headers.Add(TEXT("X-Tap-Ts"), GenerateTimestamp());
	Headers.Add(TEXT("X-Tap-Nonce"), GenerateNonce());
	Headers.Add(TEXT("X-Tap-Lang"), GetLanguage());
	Headers.Add(TEXT("User-Agent"), GenerateUserAgent());

	// 可选参数 - 游戏用户 ID
	if (!GameUserID.IsEmpty())
	{
		Headers.Add(TEXT("X-Tap-SDK-Game-User-Id"), GameUserID);
	}

	// 可选参数 - Authorization
	if (bEnableAuthorization && !AuthorizationToken.IsEmpty())
	{
		Headers.Add(TEXT("Authorization"), AuthorizationToken);
	}

	return Headers;
}

TMap<FString, FString> FTapHttpCommonParams::GetCommonQueryParams()
{
	TMap<FString, FString> Params;

	if (!ClientID.IsEmpty())
	{
		Params.Add(TEXT("client_id"), ClientID);
	}

	return Params;
}

void FTapHttpCommonParams::AddSignature(
	const FString& Method,
	const FString& URL,
	TMap<FString, FString>& Headers,
	const FString& Body)
{
	if (ClientToken.IsEmpty())
	{
		UE_LOG(LogTapCommonParams, Warning, TEXT("ClientToken is empty, cannot generate signature"));
		return;
	}

	// 构建签名字符串
	// 格式: METHOD\nPATH_AND_QUERY\nHEADERS\nBODY\n

	FString MethodPart = Method.ToUpper();

	// 提取 URL 的 Path 和 Query
	FString PathAndQuery;
	int32 ProtocolEnd = URL.Find(TEXT("://"));
	if (ProtocolEnd != INDEX_NONE)
	{
		// FString::Find 签名: Find(SubStr, SearchCase, SearchDir, StartPosition)
		// 第四个参数才是起始位置
		int32 HostStart = ProtocolEnd + 3;
		int32 PathStart = URL.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, HostStart);
		if (PathStart != INDEX_NONE)
		{
			PathAndQuery = URL.Mid(PathStart);
		}
		else
		{
			PathAndQuery = TEXT("/");
		}
	}
	else
	{
		PathAndQuery = URL;
	}

	// 收集 X-Tap-* 开头的请求头，按 key 排序后再拼接 value（对齐 Unity 实现）
	TArray<TPair<FString, FString>> HeaderPairs;
	for (const auto& Header : Headers)
	{
		if (Header.Key.StartsWith(TEXT("X-Tap-"), ESearchCase::IgnoreCase))
		{
			HeaderPairs.Add(TPair<FString, FString>(Header.Key.ToLower(), Header.Value));
		}
	}
	// 按 key 排序（Unity: OrderBy(h => h.Key.ToLowerInvariant())）
	HeaderPairs.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
	{
		return A.Key < B.Key;
	});
	TArray<FString> HeaderLines;
	for (const auto& Pair : HeaderPairs)
	{
		HeaderLines.Add(FString::Printf(TEXT("%s:%s"), *Pair.Key, *Pair.Value));
	}

	FString HeadersPart = FString::Join(HeaderLines, TEXT("\n"));
	FString BodyPart = Body;

	// 构建完整的签名数据
	FString SignData = FString::Printf(
		TEXT("%s\n%s\n%s\n%s\n"),
		*MethodPart,
		*PathAndQuery,
		*HeadersPart,
		*BodyPart
	);

	TArray<FString> SafeHeaderLines;
	for (const auto& Pair : HeaderPairs)
	{
		SafeHeaderLines.Add(FString::Printf(TEXT("%s:%s"), *Pair.Key, *TapLogPrivacy::Header(Pair.Key, Pair.Value)));
	}
	UE_LOG(LogTapCommonParams, Verbose, TEXT("Sign Data:\n%s\n%s\n%s\n%s\n"),
		*MethodPart, *TapLogPrivacy::Url(PathAndQuery), *FString::Join(SafeHeaderLines, TEXT("\n")),
		*TapLogPrivacy::Body(BodyPart));

	// 生成 HMAC-SHA256 签名
	FString Signature = GenerateHMACSHA256(SignData, ClientToken);
	Headers.Add(TEXT("X-Tap-Sign"), Signature);

	UE_LOG(LogTapCommonParams, Verbose, TEXT("Generated signature: %s"), TEXT("[REDACTED]"));
}

void FTapHttpCommonParams::SetGameUserID(const FString& UserID)
{
	GameUserID = UserID;
	UE_LOG(LogTapCommonParams, Log, TEXT("Set game user ID: %s"), *UserID);
}

void FTapHttpCommonParams::ClearGameUserID()
{
	GameUserID.Empty();
	UE_LOG(LogTapCommonParams, Log, TEXT("Cleared game user ID"));
}

void FTapHttpCommonParams::SetAuthorizationToken(const FString& Token)
{
	AuthorizationToken = Token;
	UE_LOG(LogTapCommonParams, Log, TEXT("Set authorization token"));
}

void FTapHttpCommonParams::ClearAuthorizationToken()
{
	AuthorizationToken.Empty();
	UE_LOG(LogTapCommonParams, Log, TEXT("Cleared authorization token"));
}

FString FTapHttpCommonParams::GetDeviceID()
{
	return FTapDeviceId::Get();
}

FString FTapHttpCommonParams::GetSDKVersion()
{
	return TEXT("1.0.0");
}

FString FTapHttpCommonParams::GenerateNonce()
{
	const FString Chars = TEXT("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	FString Nonce;
	Nonce.Reserve(10);

	for (int32 i = 0; i < 10; ++i)
	{
		int32 Index = FMath::RandRange(0, Chars.Len() - 1);
		Nonce.AppendChar(Chars[Index]);
	}

	return Nonce;
}

FString FTapHttpCommonParams::GenerateTimestamp()
{
	// 使用校准后的时间戳（包含 FixTime 计算的 TimeOffset）
	int64 Timestamp = FTapHttpTime::GetCurrentTime();
	return FString::Printf(TEXT("%lld"), Timestamp);
}

FString FTapHttpCommonParams::GetPlatform()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	return TEXT("PC");
#elif PLATFORM_IOS
	return TEXT("iOS");
#elif PLATFORM_ANDROID
	return TEXT("Android");
#else
	return TEXT("Unknown");
#endif
}

FString FTapHttpCommonParams::GetLanguage()
{
	// 获取当前的 Culture 语言设置
	FString LanguageCode = FInternationalization::Get().GetCurrentCulture()->GetName();

	// 转换格式：en-US -> en_US
	LanguageCode = LanguageCode.Replace(TEXT("-"), TEXT("_"));

	return LanguageCode;
}

FString FTapHttpCommonParams::GenerateUserAgent()
{
	return FString::Printf(TEXT("TapSDK-UE5/%s"), *GetSDKVersion());
}

FString FTapHttpCommonParams::GenerateHMACSHA256(const FString& SignData, const FString& Key)
{
	// 将字符串转换为 UTF-8 字节数组
	FTCHARToUTF8 SignDataUTF8(*SignData);
	FTCHARToUTF8 KeyUTF8(*Key);

	// 准备 HMAC key
	TArray<uint8> KeyBytes;
	KeyBytes.Append((uint8*)KeyUTF8.Get(), KeyUTF8.Length());

	// 准备要签名的数据
	TArray<uint8> DataBytes;
	DataBytes.Append((uint8*)SignDataUTF8.Get(), SignDataUTF8.Length());

	// 计算 HMAC-SHA256 (UE5.7 doesn't provide FSHA256::HMACBuffer, implement manually)
	// HMAC(K, m) = H((K' ⊕ opad) || H((K' ⊕ ipad) || m))
	const int32 BlockSize = 64; // SHA256 block size
	const uint8 ipad = 0x36;
	const uint8 opad = 0x5c;

	// Step 1: Prepare the key (pad or hash if too long)
	TArray<uint8> KeyPadded;
	KeyPadded.SetNumZeroed(BlockSize);

	if (KeyBytes.Num() > BlockSize)
	{
		FEncryptionContextOpenSSL CryptoContext;
		TArray<uint8> HashedKey;
		CryptoContext.CalcSHA256(TArrayView<const uint8>(KeyBytes), HashedKey);
		FMemory::Memcpy(KeyPadded.GetData(), HashedKey.GetData(), HashedKey.Num());
	}
	else
	{
		FMemory::Memcpy(KeyPadded.GetData(), KeyBytes.GetData(), KeyBytes.Num());
	}

	// Step 2: Compute inner hash H((K' ⊕ ipad) || m)
	TArray<uint8> InnerData;
	InnerData.Reserve(BlockSize + DataBytes.Num());
	for (int32 i = 0; i < BlockSize; i++)
	{
		InnerData.Add(KeyPadded[i] ^ ipad);
	}
	InnerData.Append(DataBytes);

	FEncryptionContextOpenSSL CryptoContext;
	TArray<uint8> InnerHash;
	CryptoContext.CalcSHA256(TArrayView<const uint8>(InnerData), InnerHash);

	// Step 3: Compute outer hash H((K' ⊕ opad) || inner_hash)
	TArray<uint8> OuterData;
	OuterData.Reserve(BlockSize + InnerHash.Num());
	for (int32 i = 0; i < BlockSize; i++)
	{
		OuterData.Add(KeyPadded[i] ^ opad);
	}
	OuterData.Append(InnerHash);

	TArray<uint8> FinalHash;
	CryptoContext.CalcSHA256(TArrayView<const uint8>(OuterData), FinalHash);

	// Copy result to fixed buffer
	uint8 Digest[32];
	FMemory::Memcpy(Digest, FinalHash.GetData(), 32);

	// 转换为 Base64
	FString Signature = FBase64::Encode(Digest, 32);

	return Signature;
}
