// Copyright TapTap. All Rights Reserved.
#include "Mobile/Android/TapLeaderboardAndroid.h"

#if PLATFORM_ANDROID

#include "TapLeaderboardCallbackManager.h"
#include "TapLeaderboardModule.h"
#include "TapLeaderboardResultParser.h"
#include "Async/Async.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// =============================================================================
// JNI 持久回调入口
// 类名对应 com.taptap.sdk.leaderboard.internal.ue.TapLeaderboardUERouter
// =============================================================================

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_leaderboard_internal_ue_TapLeaderboardUERouter_nativeOnLeaderboardResult(
    JNIEnv* Env, jclass, jstring JResult)
{
    if (!Env || !JResult) return;
    const char* Chars = Env->GetStringUTFChars(JResult, nullptr);
    FString Json(UTF8_TO_TCHAR(Chars));
    Env->ReleaseStringUTFChars(JResult, Chars);

    AsyncTask(ENamedThreads::GameThread, [Json]()
    {
        FTapLeaderboardAndroid::DispatchResult(Json);
    });
}

// =============================================================================
// Constructor
// =============================================================================

FTapLeaderboardAndroid::FTapLeaderboardAndroid()
{
    UE_LOG(LogTapLeaderboard, Log, TEXT("[Android] TapLeaderboard bridge initialized"));
}

// =============================================================================
// ITapLeaderboardPlatform Interface
// =============================================================================

void FTapLeaderboardAndroid::OpenLeaderboard(const FString& LeaderboardId, const FString& Collection)
{
    TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("leaderboardId"),         LeaderboardId);
    P->SetStringField(TEXT("leaderboardCollection"), Collection);
    FString Params; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params); FJsonSerializer::Serialize(P, W);
    CallRoute(TEXT("openLeaderboard"), Params);
}

void FTapLeaderboardAndroid::OpenUserProfile(const FString& OpenId, const FString& UnionId)
{
    TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("openId"),   OpenId);
    P->SetStringField(TEXT("unionId"),  UnionId);
    FString Params; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params); FJsonSerializer::Serialize(P, W);
    CallRoute(TEXT("openUserProfile"), Params);
}

void FTapLeaderboardAndroid::SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores)
{
    // 手动构建 JSON 数组，避免 int64 → double 精度丢失（double 仅 53 位尾数）
    FString ScoresJson = TEXT("[");
    for (int32 i = 0; i < Scores.Num(); ++i)
    {
        if (i > 0) ScoresJson += TEXT(",");
        FString EscapedId = Scores[i].LeaderboardId;
        EscapedId.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        EscapedId.ReplaceInline(TEXT("\""), TEXT("\\\""));
        ScoresJson += FString::Printf(TEXT("{\"leaderboard_id\":\"%s\",\"score\":%lld}"),
            *EscapedId, Scores[i].Score);
    }
    ScoresJson += TEXT("]");

    TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("scores"), ScoresJson);
    FString Params; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params); FJsonSerializer::Serialize(P, W);
    CallRoute(TEXT("submitScores"), Params);
}

void FTapLeaderboardAndroid::LoadLeaderboardScores(
    const FString& LeaderboardId, const FString& Collection,
    const FString& NextPage, const FString& PeriodToken)
{
    TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("leaderboardId"),         LeaderboardId);
    P->SetStringField(TEXT("leaderboardCollection"), Collection);
    if (!NextPage.IsEmpty())    P->SetStringField(TEXT("nextPage"),    NextPage);
    if (!PeriodToken.IsEmpty()) P->SetStringField(TEXT("periodToken"), PeriodToken);
    FString Params; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params); FJsonSerializer::Serialize(P, W);
    CallRoute(TEXT("loadLeaderboardScores"), Params);
}

void FTapLeaderboardAndroid::LoadCurrentPlayerLeaderboardScore(
    const FString& LeaderboardId, const FString& Collection, const FString& PeriodToken)
{
    TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("leaderboardId"),         LeaderboardId);
    P->SetStringField(TEXT("leaderboardCollection"), Collection);
    if (!PeriodToken.IsEmpty()) P->SetStringField(TEXT("periodToken"), PeriodToken);
    FString Params; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params); FJsonSerializer::Serialize(P, W);
    CallRoute(TEXT("loadCurrentPlayerLeaderboardScore"), Params);
}

void FTapLeaderboardAndroid::LoadPlayerCenteredScores(
    const FString& LeaderboardId, const FString& Collection,
    const FString& PeriodToken, int32 MaxCount)
{
    TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
    P->SetStringField(TEXT("leaderboardId"),         LeaderboardId);
    P->SetStringField(TEXT("leaderboardCollection"), Collection);
    if (!PeriodToken.IsEmpty()) P->SetStringField(TEXT("periodToken"), PeriodToken);
    if (MaxCount > 0)           P->SetNumberField(TEXT("maxCount"),    MaxCount);
    FString Params; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params); FJsonSerializer::Serialize(P, W);
    CallRoute(TEXT("loadPlayerCenteredScores"), Params);
}

void FTapLeaderboardAndroid::RegisterLeaderboardCallback()
{
    CallRoute(TEXT("registerLeaderboardCallback"), FString());
}

void FTapLeaderboardAndroid::UnregisterLeaderboardCallback()
{
    CallRoute(TEXT("unregisterLeaderboardCallback"), FString());
}

void FTapLeaderboardAndroid::SetShareCallback()
{
    CallRoute(TEXT("setShareCallback"), FString());
}

// =============================================================================
// Dispatch（GameThread 调用）
// =============================================================================

void FTapLeaderboardAndroid::DispatchResult(const FString& ResultJson)
{
    const TapLeaderboardResultParser::FParsed P = TapLeaderboardResultParser::Parse(ResultJson);
    if (!P.bParsed) { UE_LOG(LogTapLeaderboard, Warning, TEXT("[Android] unparsed result: %s"), *ResultJson); return; }

    if (P.Op == TEXT("loadScores") || P.Op == TEXT("loadPlayerCenteredScores"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().Scores(P.Op,TapLeaderboardResultParser::ParseScoresResult(P.Data));
        else
            FTapLeaderboardCallbackManager::Get().Error(P.Op, P.ErrCode, P.ErrMsg);
    }
    else if (P.Op == TEXT("loadCurrentPlayerScore"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().Current(TapLeaderboardResultParser::ParseUserScoreResult(P.Data));
        else
            FTapLeaderboardCallbackManager::Get().Error(P.Op, P.ErrCode, P.ErrMsg);
    }
    else if (P.Op == TEXT("submitScores"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().Submitted(TapLeaderboardResultParser::ParseSubmitResult(P.Data));
        else
            FTapLeaderboardCallbackManager::Get().Error(P.Op, P.ErrCode, P.ErrMsg);
    }
    else if (P.Op == TEXT("leaderboardCallback"))
    {
        FTapLeaderboardCallbackManager::Get().Event(P.EventCode, P.EventMessage);
    }
    else if (P.Op == TEXT("shareCallback"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().ShareResult(true, P.Data);
        else
            FTapLeaderboardCallbackManager::Get().ShareResult(false, P.ErrMsg);
    }
    else
    {
        UE_LOG(LogTapLeaderboard, Verbose, TEXT("[Android] ignored op: %s"), *P.Op);
    }
}

// =============================================================================
// Router Helper
// =============================================================================

FString FTapLeaderboardAndroid::BuildRequest(const FString& Method, const FString& ParamsJson)
{
    TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
    Obj->SetStringField(TEXT("method"), Method);
    if (!ParamsJson.IsEmpty()) Obj->SetStringField(TEXT("params"), ParamsJson);
    FString Out;
    TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
    return Out;
}

FString FTapLeaderboardAndroid::CallRoute(const FString& Method, const FString& ParamsJson)
{
    JNIEnv* Env = FAndroidApplication::GetJavaEnv();
    if (!Env) { UE_LOG(LogTapLeaderboard, Error, TEXT("[Android] JNI null")); return FString(); }

    jobject Activity = FAndroidApplication::GetGameActivityThis();
    if (!Activity) { UE_LOG(LogTapLeaderboard, Error, TEXT("[Android] GameActivity null")); return FString(); }

    jclass    Cls       = Env->GetObjectClass(Activity);
    jmethodID RouteMethod = Env->GetMethodID(
        Cls, "routeToTapLeaderboard",
        "(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
    );

    if (!RouteMethod || Env->ExceptionCheck())
    {
        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        UE_LOG(LogTapLeaderboard, Error, TEXT("[Android] routeToTapLeaderboard not found"));
        Env->DeleteLocalRef(Cls);
        return FString();
    }

    const FString Req  = BuildRequest(Method, ParamsJson);
    jstring       jReq = Env->NewStringUTF(TCHAR_TO_UTF8(*Req));
    jstring       jRes = (jstring)Env->CallObjectMethod(Activity, RouteMethod, jReq, nullptr);

    if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }

    FString Result;
    if (jRes)
    {
        const char* C = Env->GetStringUTFChars(jRes, nullptr);
        Result = FString(UTF8_TO_TCHAR(C));
        Env->ReleaseStringUTFChars(jRes, C);
        Env->DeleteLocalRef(jRes);
    }
    Env->DeleteLocalRef(jReq);
    Env->DeleteLocalRef(Cls);
    return Result;
}

#endif // PLATFORM_ANDROID
