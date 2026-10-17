// Copyright TapTap. All Rights Reserved.

#include "Internal/TapLogPrivacy.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLogPrivacyBodyTest, "TapBasic.Http.Pure.LogPrivacy.Body",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLogPrivacyBodyTest::RunTest(const FString&)
{
	const FString Input = TEXT(R"({"code":400,"message":"身份证格式错误，请重新输入","name":"level_complete","count":3,"data":[{"name":"private-player-name","idcard":"private-idcard","user_identifier":"diagnostic-user"}],"accessTokenDetail":{"kid":"private-kid","macKey":"private-key","tokenType":"mac","scopes":"public_profile","expires_in":3600},"client_token":"private-client-token","password":"private-password"})");
	const FString Output = TapLogPrivacy::Body(Input);
	TestFalse(TEXT("All private fixture values removed"), Output.Contains(TEXT("private-")));
	TestTrue(TEXT("Business event name preserved"), Output.Contains(TEXT("level_complete")));
	TestTrue(TEXT("Error message preserved"), Output.Contains(TEXT("身份证格式错误，请重新输入")));
	TestTrue(TEXT("Token metadata preserved"), Output.Contains(TEXT("public_profile")));
	TestTrue(TEXT("Expiry preserved"), Output.Contains(TEXT("3600")));
	TestTrue(TEXT("Input not modified"), Input.Contains(TEXT("private-password")));
	TSharedPtr<FJsonObject> Object;
	TestTrue(TEXT("Redacted body remains valid JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Output), Object));
	if (Object.IsValid()) TestEqual(TEXT("Business code preserved"), Object->GetIntegerField(TEXT("code")), 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLogPrivacyNestedTest, "TapBasic.Http.Pure.LogPrivacy.Nested",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLogPrivacyNestedTest::RunTest(const FString&)
{
	const FString Input = TEXT(R"({"success":true,"data":"{\"openid\":\"diagnostic-openid\",\"name\":\"private-name\",\"accessTokenDetail\":{\"macKey\":\"private-mac\",\"scopes\":\"basic\"}}","params":"{\"ClientToken\":\"private-client\",\"region\":\"CN\"}"})");
	const FString Output = TapLogPrivacy::Body(Input);
	TestFalse(TEXT("JSON-encoded router values redacted"), Output.Contains(TEXT("private-")));
	TestTrue(TEXT("Router status preserved"), Output.Contains(TEXT("true")));
	TestTrue(TEXT("Nested region preserved"), Output.Contains(TEXT("CN")));
	TestTrue(TEXT("Nested scope preserved"), Output.Contains(TEXT("basic")));
	const FString Token = TapLogPrivacy::Body(TEXT(R"({"success":true,"data":"private-token","message":"ok"})"), true);
	TestFalse(TEXT("Known opaque data redacted"), Token.Contains(TEXT("private-token")));
	TestTrue(TEXT("Other router fields preserved"), Token.Contains(TEXT("ok")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLogPrivacyUrlTest, "TapBasic.Http.Pure.LogPrivacy.UrlAndHeaders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLogPrivacyUrlTest::RunTest(const FString&)
{
	const FString Input = TEXT("GET /callback?code=private-code&client_id=app&access%5Ftoken=private-token&code=private-duplicate&error=access_denied HTTP/1.1");
	const FString Output = TapLogPrivacy::Url(Input, true);
	TestFalse(TEXT("Repeated/encoded credentials removed"), Output.Contains(TEXT("private-")));
	TestTrue(TEXT("Request line retained"), Output.StartsWith(TEXT("GET /callback?")) && Output.EndsWith(TEXT("HTTP/1.1")));
	TestTrue(TEXT("Client ID retained"), Output.Contains(TEXT("client_id=app")));
	TestTrue(TEXT("OAuth failure retained"), Output.Contains(TEXT("error=access_denied")));
	TestEqual(TEXT("Ordinary URL business code preserved"), TapLogPrivacy::Url(TEXT("https://example.com/status?code=503")), FString(TEXT("https://example.com/status?code=503")));
	TestTrue(TEXT("Device grant code redacted"), TapLogPrivacy::Body(TEXT("grant_type=device_token&code=private-device-code")).Contains(TEXT("code=[REDACTED]")));
	const FString EncodedForm = TapLogPrivacy::Body(TEXT("client_id=app&info=%7B%22device_id%22%3A%22private-device%22%2C%22platform%22%3A%22ios%22%7D"));
	TestTrue(TEXT("Encoded JSON device ID preserved"), EncodedForm.Contains(TEXT("private-device")));
	TestTrue(TEXT("Encoded JSON nonprivate field retained"), EncodedForm.Contains(TEXT("ios")));
	TestFalse(TEXT("URL userinfo/fragment credentials removed"), TapLogPrivacy::Url(TEXT("https://private-user:private-pass@example.com/a#access_token=private-fragment&scope=basic")).Contains(TEXT("private-")));
	TestFalse(TEXT("Form credentials removed"), TapLogPrivacy::Body(TEXT("grant_type=authorization_code&code=private-code&client_secret=private-secret")).Contains(TEXT("private-")));
	TestFalse(TEXT("Redirect URL inside form does not bypass code redaction"), TapLogPrivacy::Body(TEXT("client_id=app&grant_type=authorization_code&code=private-code&redirect_uri=http://localhost/authorize&code_verifier=private-verifier")).Contains(TEXT("private-")));
	TestEqual(TEXT("Safe header preserved"), TapLogPrivacy::Header(TEXT("Content-Type"), TEXT("application/json")), FString(TEXT("application/json")));
	for (const FString Key : {TEXT("Authorization"), TEXT("aUtHoRiZaTiOn"), TEXT("X-Tap-Sign"), TEXT("Cookie")})
	{
		TestEqual(*Key, TapLogPrivacy::Header(Key, TEXT("private-value")), FString(TEXT("[REDACTED]")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLogPrivacyMessageTest, "TapBasic.Http.Pure.LogPrivacy.Messages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLogPrivacyMessageTest::RunTest(const FString&)
{
	for (const FString Message : {TEXT("网络异常，请稍后重试"), TEXT("[RealName] validation failed"), TEXT("Message"), TEXT("token expired"), TEXT("{invalid response")})
	{
		TestEqual(TEXT("Ordinary diagnostic text unchanged"), TapLogPrivacy::Body(Message), Message);
	}
	TestTrue(TEXT("Bracket-prefixed JSON message preserved"), TapLogPrivacy::Body(TEXT(R"({"message":"[SDK] retry later","code":503})")).Contains(TEXT("[SDK] retry later")));
	TestFalse(TEXT("Truncated private JSON cannot leak"), TapLogPrivacy::Body(TEXT("{\"token\":\"private-truncated")).Contains(TEXT("private-truncated")));
	TestFalse(TEXT("Escaped key in truncated JSON cannot leak"), TapLogPrivacy::Body(TEXT("{\"to\\u006ben\":\"private-truncated")).Contains(TEXT("private-truncated")));
	TestTrue(TEXT("Unknown business fields preserved"), TapLogPrivacy::Body(TEXT(R"({"key":"cache","name":"event","code":0,"tokenType":"mac"})")).Contains(TEXT("cache")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLogPrivacyIdentifierTest, "TapBasic.Http.Pure.LogPrivacy.DiagnosticIdentifiers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLogPrivacyIdentifierTest::RunTest(const FString&)
{
	for (const FString Key : {TEXT("UserId"), TEXT("user_id"), TEXT("user_identifier"), TEXT("gameUserId"),
		TEXT("OpenID"), TEXT("union_id"), TEXT("device_id"), TEXT("deviceId5"), TEXT("OAID"),
		TEXT("IDFA"), TEXT("android_id"), TEXT("persistent_id"), TEXT("installation_id"), TEXT("install_uuid")})
	{
		const FString Input = FString::Printf(TEXT("{\"%s\":\"diagnostic-id\",\"token\":\"private-token\"}"), *Key);
		const FString Output = TapLogPrivacy::Body(Input);
		TestTrue(*FString::Printf(TEXT("JSON %s retained"), *Key), Output.Contains(TEXT("diagnostic-id")));
		TestFalse(TEXT("Adjacent token still redacted"), Output.Contains(TEXT("private-token")));
		const FString Params = Key + TEXT("=diagnostic-id&password=private-password");
		TestEqual(*FString::Printf(TEXT("Form %s retained"), *Key),
			TapLogPrivacy::Body(Params), Key + TEXT("=diagnostic-id&password=[REDACTED]"));
		TestEqual(*FString::Printf(TEXT("URL %s retained"), *Key),
			TapLogPrivacy::Url(TEXT("https://example.com/?") + Params),
			TEXT("https://example.com/?") + Key + TEXT("=diagnostic-id&password=[REDACTED]"));
	}
	for (const FString Key : {TEXT("X-Tap-Device-Id"), TEXT("X-Tap-SDK-Game-User-Id")})
	{
		TestEqual(*Key, TapLogPrivacy::Header(Key, TEXT("diagnostic-id")), FString(TEXT("diagnostic-id")));
	}
	TestTrue(TEXT("Nested router account ID retained"), TapLogPrivacy::Body(
		TEXT(R"({"data":"{\"openid\":\"diagnostic-openid\"}"})")).Contains(TEXT("diagnostic-openid")));
	TestTrue(TEXT("Opaque device ID result retained"), TapLogPrivacy::Body(
		TEXT(R"({"success":true,"data":"diagnostic-device"})")).Contains(TEXT("diagnostic-device")));
	return true;
}

#endif
