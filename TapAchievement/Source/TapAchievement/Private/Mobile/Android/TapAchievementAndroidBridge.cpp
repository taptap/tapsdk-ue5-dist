// Copyright TapTap. All Rights Reserved.
#include "Mobile/Android/TapAchievementAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapAchievementModule.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/CriticalSection.h"

namespace
{
    TapAchievementAndroidBridge::FResultCallback GResultCallback;
    FCriticalSection                             GCallbackLock;

    // ── helpers ─────────────────────────────────────────────────────────────

    FString MakeRequest(const FString& Method, const FString& Params)
    {
        TSharedPtr<FJsonObject> O = MakeShareable(new FJsonObject);
        O->SetStringField(TEXT("method"), Method);
        if (!Params.IsEmpty()) O->SetStringField(TEXT("params"), Params);
        FString Out;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(O.ToSharedRef(), W);
        return Out;
    }

    // 同步调 routeToTapAchievement（fire-and-forget，结果走 nativeOnAchievementEvent）
    void CallRoute(const FString& Method, const FString& Params)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { UE_LOG(LogTapAchievement, Error, TEXT("[Android] JNI null")); return; }

        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapAchievement, Error, TEXT("[Android] GameActivity null")); return; }

        jclass    Cls    = Env->GetObjectClass(Activity);
        jmethodID Method_ = Env->GetMethodID(
            Cls, "routeToTapAchievement",
            "(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
        );

        if (!Method_ || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapAchievement, Error, TEXT("[Android] routeToTapAchievement not found"));
            Env->DeleteLocalRef(Cls);
            return;
        }

        const FString Req = MakeRequest(Method, Params);
        jstring jReq = Env->NewStringUTF(TCHAR_TO_UTF8(*Req));
        jstring jRes = (jstring)Env->CallObjectMethod(Activity, Method_, jReq, nullptr);

        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        if (jRes) Env->DeleteLocalRef(jRes);
        Env->DeleteLocalRef(jReq);
        Env->DeleteLocalRef(Cls);
    }

    FString ArgsAchievementId(const FString& Id)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("achievementId"), Id);
        FString S; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&S); FJsonSerializer::Serialize(O, W); return S;
    }
    FString ArgsIncrement(const FString& Id, int32 Steps)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("achievementId"), Id);
        O->SetNumberField(TEXT("steps"), Steps);
        FString S; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&S); FJsonSerializer::Serialize(O, W); return S;
    }
    FString ArgsToast(bool bEnable)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetBoolField(TEXT("enableToast"), bEnable);
        FString S; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&S); FJsonSerializer::Serialize(O, W); return S;
    }
}

// =============================================================================
// JNI Persistent Callback Entry Point
// 类名对应 com.taptap.sdk.achievement.internal.ue.TapAchievementUERouter
// =============================================================================

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_achievement_internal_ue_TapAchievementUERouter_nativeOnAchievementEvent(
    JNIEnv* Env, jclass, jstring JResult)
{
    if (!Env || !JResult) return;
    const char* Chars = Env->GetStringUTFChars(JResult, nullptr);
    FString Json(UTF8_TO_TCHAR(Chars));
    Env->ReleaseStringUTFChars(JResult, Chars);

    TapAchievementAndroidBridge::FResultCallback Cb;
    {
        FScopeLock Lock(&GCallbackLock);
        Cb = GResultCallback;
    }
    if (Cb)
    {
        AsyncTask(ENamedThreads::GameThread, [Cb, Json]() { Cb(Json); });
    }
}

// =============================================================================
// Public API
// =============================================================================

namespace TapAchievementAndroidBridge
{
    void Init(FResultCallback OnPersistentResult)
    {
        FScopeLock Lock(&GCallbackLock);
        GResultCallback = MoveTemp(OnPersistentResult);
    }

    void RegisterCallback() { CallRoute(TEXT("registerCallback"), FString()); }
    void Unlock(const FString& Id)              { CallRoute(TEXT("unlock"),          ArgsAchievementId(Id)); }
    void Increment(const FString& Id, int32 N)  { CallRoute(TEXT("increment"),       ArgsIncrement(Id, N)); }
    void ShowAchievements()                     { CallRoute(TEXT("showAchievements"), FString()); }
    void SetToastEnable(bool bEnable)           { CallRoute(TEXT("setToastEnable"),   ArgsToast(bEnable)); }
}

#endif // PLATFORM_ANDROID
