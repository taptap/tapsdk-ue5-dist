// Copyright TapTap. All Rights Reserved.

#include "Internal/TapLogPrivacy.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Internationalization/Regex.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* Redacted = TEXT("[REDACTED]");

	FString Normalize(const FString& Key)
	{
		FString Result;
		for (TCHAR C : Key)
		{
			if (FChar::IsAlnum(C)) Result.AppendChar(FChar::ToLower(C));
		}
		return Result;
	}

	bool IsPrivate(const FString& Key)
	{
		static const TSet<FString> Keys = {
			TEXT("token"), TEXT("accesstoken"), TEXT("refreshtoken"), TEXT("clienttoken"),
			TEXT("idtoken"), TEXT("accesstokendetail"), TEXT("tokenkid"), TEXT("authorizationtoken"),
			TEXT("authorization"), TEXT("password"), TEXT("passwd"),
			TEXT("secret"), TEXT("clientsecret"), TEXT("apikey"), TEXT("mackey"), TEXT("kid"),
			TEXT("privatekey"), TEXT("secretkey"), TEXT("signature"), TEXT("devicecode"), TEXT("usercode"),
			TEXT("authcode"), TEXT("authorizationcode"), TEXT("codeverifier"),
			TEXT("realname"), TEXT("fullname"), TEXT("idcard"), TEXT("idcardnumber"),
			TEXT("idnumber"), TEXT("identitynumber"), TEXT("email"), TEXT("phone"),
			TEXT("phonenumber"), TEXT("mobilephone"), TEXT("mobile"), TEXT("avatar")
		};
		return Keys.Contains(Key);
	}

	bool IsTokenObject(const FString& Key)
	{
		return Key == TEXT("token") || Key == TEXT("accesstoken") || Key == TEXT("accesstokendetail");
	}

	FString SanitizeBody(const FString& Input, bool bPrivateData, int32 Depth);

	bool HasPrivateJsonField(const FString& Input)
	{
		const FRegexPattern Pattern(TEXT("\"((?:\\\\.|[^\"\\\\])*)\"\\s*:"));
		FRegexMatcher Matcher(Pattern, Input);
		while (Matcher.FindNext())
		{
			// 解码字段名中的 JSON 转义，避免 token 被写成 to\\u006ben 后漏出。
			TSharedPtr<FJsonObject> Object;
			const FString Probe = TEXT("{\"") + Matcher.GetCaptureGroup(1) + TEXT("\":0}");
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Probe), Object))
			{
				for (const auto& Pair : Object->Values)
				{
					const FString K = Normalize(Pair.Key);
					if (IsPrivate(K)) return true;
				}
			}
		}
		return false;
	}

	// 只有 OAuth 回调或 OAuth grant 表单中的 code 才确认为授权码。
	FString Parameters(const FString& Input, bool bAuthorizationCode = false, int32 Depth = 0)
	{
		TArray<FString> Pairs;
		Input.ParseIntoArray(Pairs, TEXT("&"), false);
		bool bRealName = false;
		for (const FString& Pair : Pairs)
		{
			FString Key, Value;
			if (Pair.Split(TEXT("="), &Key, &Value))
			{
				const FString K = Normalize(FGenericPlatformHttp::UrlDecode(Key));
				bRealName |= K == TEXT("idcard") || K == TEXT("idnumber") || K == TEXT("idcardnumber");
				if (K == TEXT("granttype"))
				{
					const FString Grant = FGenericPlatformHttp::UrlDecode(Value);
					bAuthorizationCode |= Grant == TEXT("authorization_code") || Grant == TEXT("device_token");
				}
			}
		}
		for (FString& Pair : Pairs)
		{
			FString Key, Value;
			if (!Pair.Split(TEXT("="), &Key, &Value)) continue;
			const FString K = Normalize(FGenericPlatformHttp::UrlDecode(Key));
			if (IsPrivate(K) || (bAuthorizationCode && K == TEXT("code")) || (bRealName && K == TEXT("name")))
			{
				Pair = Key + TEXT("=") + Redacted;
			}
			else
			{
				// 表单内嵌 JSON 同样只脱敏凭据和个人信息，保留排查标识。
				const FString Decoded = FGenericPlatformHttp::UrlDecode(Value);
				const FString Trimmed = Decoded.TrimStartAndEnd();
				if (Trimmed.StartsWith(TEXT("{")) || Trimmed.StartsWith(TEXT("[")))
				{
					Pair = Key + TEXT("=") + FGenericPlatformHttp::UrlEncode(SanitizeBody(Decoded, false, Depth + 1));
				}
			}
		}
		return FString::Join(Pairs, TEXT("&"));
	}

	void SanitizeValue(TSharedPtr<FJsonValue>& Value, const FString& Context, bool bPrivateData, int32 Depth)
	{
		if (!Value.IsValid()) return;
		if (Depth > 32)
		{
			Value = MakeShared<FJsonValueString>(TEXT("[nested payload exceeds log depth limit]"));
			return;
		}
		if (Value->Type == EJson::Object)
		{
			const auto Object = Value->AsObject();
			bool bAccount = Context == TEXT("account") || Context == TEXT("profile") || Context == TEXT("user");
			for (const auto& Pair : Object->Values)
			{
				const FString K = Normalize(Pair.Key);
				bAccount |= K == TEXT("openid") || K == TEXT("unionid") || K == TEXT("idcard")
					|| K == TEXT("idnumber") || K == TEXT("idcardnumber");
			}
			for (auto& Pair : Object->Values)
			{
				const FString K = Normalize(Pair.Key);
				// 保留 token 对象中的 scopes、算法、有效期等排查信息。
				const bool bTokenObject = IsTokenObject(K) && Pair.Value->Type == EJson::Object;
				if ((!bTokenObject && IsPrivate(K)) || (bAccount && K == TEXT("name"))
					|| (IsTokenObject(Context) && (K == TEXT("key") || K == TEXT("id")))
					|| (bPrivateData && K == TEXT("data")))
				{
					Pair.Value = MakeShared<FJsonValueString>(Redacted);
				}
				else SanitizeValue(Pair.Value, K, false, Depth + 1);
			}
		}
		else if (Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> Values = Value->AsArray();
			for (auto& Item : Values) SanitizeValue(Item, Context, false, Depth + 1);
			Value = MakeShared<FJsonValueArray>(Values);
		}
		else if (Value->Type == EJson::String)
		{
			// Router 的 params/data 可能是再次序列化的 JSON 字符串。
			Value = MakeShared<FJsonValueString>(SanitizeBody(Value->AsString(), false, Depth + 1));
		}
	}

	FString SanitizeBody(const FString& Input, bool bPrivateData, int32 Depth)
	{
		if (Depth > 32) return TEXT("[nested payload exceeds log depth limit]");
		const FString Trimmed = Input.TrimStartAndEnd();
		if (Trimmed.StartsWith(TEXT("{")) || Trimmed.StartsWith(TEXT("[")))
		{
			TSharedPtr<FJsonValue> Root;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Root))
			{
				// 只有已识别敏感字段、但结构损坏无法定位值时才省略正文。
				// 普通的 [SDK] 错误文本仍然保留。
				return bPrivateData || HasPrivateJsonField(Input)
					? FString::Printf(TEXT("[invalid JSON with private fields; %d characters]"), Input.Len()) : Input;
			}
			SanitizeValue(Root, FString(), bPrivateData, Depth);
			FString Result;
			const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
			FJsonSerializer::Serialize(Root, TEXT(""), Writer);
			return Result;
		}
		if (bPrivateData) return Redacted;
		FString FirstKey, Rest;
		if (Trimmed.Split(TEXT("="), &FirstKey, &Rest) && !FirstKey.IsEmpty())
		{
			bool bFormKey = true;
			for (TCHAR C : FirstKey)
			{
				bFormKey &= FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') || C == TEXT('%') || C == TEXT('.');
			}
			// 表单中的 redirect_uri 可以包含 ://，不能把整个表单当成 URL。
			if (bFormKey) return Parameters(Input, false, Depth);
		}
		if (Trimmed.Contains(TEXT("://"))) return TapLogPrivacy::Url(Input);
		return Input;
	}
}

FString TapLogPrivacy::Body(const FString& Value, bool bPrivateData)
{
	return SanitizeBody(Value, bPrivateData, 0);
}

FString TapLogPrivacy::Url(const FString& Value, bool bAuthorizationCode)
{
	FString Result = Value;
	const int32 Scheme = Result.Find(TEXT("://"));
	if (Scheme != INDEX_NONE)
	{
		const int32 Start = Scheme + 3;
		int32 End = Start;
		while (End < Result.Len() && Result[End] != TEXT('/') && Result[End] != TEXT('?') && Result[End] != TEXT('#')) ++End;
		const int32 At = Result.Find(TEXT("@"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (At != INDEX_NONE && At < End) Result = Result.Left(Start) + Redacted + Result.Mid(At);
	}
	// 支持 OAuth query/fragment，同时保留 HTTP 请求行末尾的协议版本。
	int32 Start = 0;
	for (int32 I = 0; I < Result.Len(); ++I)
	{
		if (Result[I] == TEXT('?') || Result[I] == TEXT('#')) { Start = I + 1; break; }
	}
	if (Start == 0) return Result;
	FString Output = Result.Left(Start);
	for (int32 I = Start; I <= Result.Len(); ++I)
	{
		if (I == Result.Len() || Result[I] == TEXT('#') || FChar::IsWhitespace(Result[I]))
		{
			Output += Parameters(Result.Mid(Start, I - Start), bAuthorizationCode);
			if (I == Result.Len()) break;
			if (FChar::IsWhitespace(Result[I])) return Output + Result.Mid(I);
			Output.AppendChar(Result[I]);
			Start = I + 1;
		}
	}
	return Output;
}

FString TapLogPrivacy::Header(const FString& Key, const FString& Value)
{
	FString K = Normalize(Key);
	if (K.StartsWith(TEXT("xtap"))) K.RightChopInline(4);
	if (IsPrivate(K) || K == TEXT("proxyauthorization") || K == TEXT("cookie")
		|| K == TEXT("setcookie") || K == TEXT("xapikey") || K == TEXT("sign")) return Redacted;
	return Value;
}
