// Copyright TapTap. All Rights Reserved.
#include "Android/TapUpdateAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapUpdateModule.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Async/Async.h"

namespace
{
    FCriticalSection GCallbackLock;
    TFunction<void()> GOnCancel;
    // 通过 GameActivity.routeToTapUpdate(commandJson) 发送命令
    // 与 TapAchievement.routeToTapAchievement 完全同构
    void CallRoute(const FString& Method)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { UE_LOG(LogTapUpdate, Error, TEXT("[TapUpdate] JNI Env null")); return; }

        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapUpdate, Error, TEXT("[TapUpdate] GameActivity null")); return; }

        jclass Cls = Env->GetObjectClass(Activity);
        jmethodID RouteMethod = Env->GetMethodID(Cls, "routeToTapUpdate", "(Ljava/lang/String;)V");

        if (!RouteMethod || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapUpdate, Error, TEXT("[TapUpdate] routeToTapUpdate not found on GameActivity"));
            Env->DeleteLocalRef(Cls);
            return;
        }

        const FString CmdJson = FString::Printf(
            TEXT("{\"service\":\"BridgeUpdateService\",\"method\":\"%s\",\"args\":\"\"}"), *Method);
        jstring JCmd = Env->NewStringUTF(TCHAR_TO_UTF8(*CmdJson));

        Env->CallVoidMethod(Activity, RouteMethod, JCmd);

        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        Env->DeleteLocalRef(JCmd);
        Env->DeleteLocalRef(Cls);
    }
}

// ============================================================================
// JNI nativeOnResult（路径 B 全局回调，单参，无 callbackId）
// 对齐 Unity: result.content.ToLower().Contains("cancel")
// ============================================================================

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_kit_internal_enginebridge_EngineBridge_nativeOnResult(
    JNIEnv* Env, jobject /*thiz*/, jstring JResult)
{
    if (!Env || !JResult) return;

    const char* Chars = Env->GetStringUTFChars(JResult, nullptr);
    FString Json(UTF8_TO_TCHAR(Chars));
    Env->ReleaseStringUTFChars(JResult, Chars);

    UE_LOG(LogTapUpdate, Log, TEXT("[TapUpdate] nativeOnResult: %s"), *Json);

    if (Json.ToLower().Contains(TEXT("cancel")))
    {
        TFunction<void()> Callback;{FScopeLock Lock(&GCallbackLock);Callback=MoveTemp(GOnCancel);GOnCancel=nullptr;}
        AsyncTask(ENamedThreads::GameThread, [Callback]()
        {
            if(Callback)Callback();
        });
    }
}

// ============================================================================
// Public API
// ============================================================================

namespace TapUpdateAndroidBridge
{
    void UpdateGame(TFunction<void()> OnCancel)
    {
        {FScopeLock Lock(&GCallbackLock);GOnCancel=MoveTemp(OnCancel);}
        UE_LOG(LogTapUpdate, Log, TEXT("[TapUpdate] UpdateGame called"));
        CallRoute(TEXT("updateGame"));
    }

    void CheckForceUpdate()
    {
        UE_LOG(LogTapUpdate, Log, TEXT("[TapUpdate] CheckForceUpdate called"));
        CallRoute(TEXT("checkForceUpdate"));
    }
}

#endif // PLATFORM_ANDROID
