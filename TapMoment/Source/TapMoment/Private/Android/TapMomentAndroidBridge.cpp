// Copyright TapTap. All Rights Reserved.
#include "Android/TapMomentAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapMomentModule.h"
#include "Internal/Platform/TapCoreAndroid.h"
#include "Internal/TapAndroidCallbackId.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include <atomic>

namespace
{
    std::atomic<jmethodID>           GRouteMethodID{ nullptr };
    FCriticalSection                 GCallbackLock;
    int64                            GMomentCallbackId  = 0;
    jobject                          GMomentProxyGlobal = nullptr;
    TapMomentAndroidBridge::FMomentCallback GMomentCallback;
    bool                             GInitialized = false;

    // Create EngineBridgeCallbackProxy(id)
    jobject CreateProxy(JNIEnv* Env, int64 Id)
    {
        jclass ProxyClass = FAndroidApplication::FindJavaClass(
            "com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy");
        if (!ProxyClass || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapMoment, Error, TEXT("[TapMoment-Android] EngineBridgeCallbackProxy not found"));
            return nullptr;
        }
        jmethodID Ctor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
        if (!Ctor || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            Env->DeleteLocalRef(ProxyClass);
            return nullptr;
        }
        jobject Local = Env->NewObject(ProxyClass, Ctor, static_cast<jlong>(Id));
        Env->DeleteLocalRef(ProxyClass);
        if (!Local || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            return nullptr;
        }
        jobject Global = Env->NewGlobalRef(Local);
        Env->DeleteLocalRef(Local);
        return Global;
    }

    // Build {"method":"xxx","args":"yyy"} and call routeToTapMoment(json, callback)
    void InvokeRoute(JNIEnv* Env, const FString& Method, const FString& ArgsJson, jobject CallbackProxy)
    {
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapMoment, Error, TEXT("[TapMoment-Android] GameActivity null")); return; }

        jmethodID RouteMethod = GRouteMethodID.load(std::memory_order_acquire);
        if (RouteMethod == nullptr)
        {
            jclass Cls = Env->GetObjectClass(Activity);
            RouteMethod = Env->GetMethodID(
                Cls, "routeToTapMoment",
                "(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)V");
            Env->DeleteLocalRef(Cls);
            if (!RouteMethod || Env->ExceptionCheck())
            {
                if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
                UE_LOG(LogTapMoment, Error, TEXT("[TapMoment-Android] routeToTapMoment not found"));
                return;
            }
            GRouteMethodID.store(RouteMethod, std::memory_order_release);
        }

        // {"method":"xxx","args":"yyy"}
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("method"), Method);
        Root->SetStringField(TEXT("args"), ArgsJson);
        FString ReqJson;
        auto Writer = TJsonWriterFactory<>::Create(&ReqJson);
        FJsonSerializer::Serialize(Root, Writer);

        jstring JReq = Env->NewStringUTF(TCHAR_TO_UTF8(*ReqJson));
        Env->CallVoidMethod(Activity, RouteMethod, JReq, CallbackProxy);
        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        Env->DeleteLocalRef(JReq);
    }

    // Dispatch persistent moment callback — called by GlobalCallbackHandler
    // ResultJson = {"code":N,"msg":"..."} (no extra envelope)
    bool DispatchMomentCallback(int64 CallbackId, const FString& ResultJson)
    {
        TapMomentAndroidBridge::FMomentCallback Snapshot;
        {
            FScopeLock Lock(&GCallbackLock);
            if (CallbackId != GMomentCallbackId) { return false; }
            Snapshot = GMomentCallback;
        }
        if (!Snapshot) { return true; }

        int32 Code = 0;
        FString Msg;
        TSharedPtr<FJsonObject> Root;
        auto Reader = TJsonReaderFactory<>::Create(ResultJson);
        if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
        {
            double CodeNum = 0.0;
            if (Root->TryGetNumberField(TEXT("code"), CodeNum))
                Code = static_cast<int32>(CodeNum);
            Root->TryGetStringField(TEXT("msg"), Msg);
        }

        AsyncTask(ENamedThreads::GameThread, [Snapshot, Code, Msg]()
        {
            Snapshot(Code, Msg);
        });
        return true; // persistent: keep registered
    }
}

namespace TapMomentAndroidBridge
{
    void Init(FMomentCallback OnCallback)
    {
        {
            FScopeLock Lock(&GCallbackLock);
            GMomentCallback = MoveTemp(OnCallback);
            if (GInitialized) { return; }
            GInitialized = true;
        }
        FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
        {
            return DispatchMomentCallback(CallbackId, ResultJson);
        });
        UE_LOG(LogTapMoment, Log, TEXT("[TapMoment-Android] Bridge initialized"));
    }

    void Open()
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        InvokeRoute(Env, TEXT("openMoment"), TEXT(""), nullptr);
    }

    void OpenScene(const FString& SceneId)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
        Args->SetStringField(TEXT("sceneId"), SceneId);
        FString ArgsJson; auto W = TJsonWriterFactory<>::Create(&ArgsJson);
        FJsonSerializer::Serialize(Args, W);
        InvokeRoute(Env, TEXT("openScene"), ArgsJson, nullptr);
    }

    void Publish(const FString& PublishMetaDataJson)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
        Args->SetStringField(TEXT("publishMetaData"), PublishMetaDataJson);
        FString ArgsJson; auto W = TJsonWriterFactory<>::Create(&ArgsJson);
        FJsonSerializer::Serialize(Args, W);
        InvokeRoute(Env, TEXT("publish"), ArgsJson, nullptr);
    }

    void Close()
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        InvokeRoute(Env, TEXT("close"), TEXT(""), nullptr);
    }

    void CloseWithTitle(const FString& Title, const FString& Content)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
        Args->SetStringField(TEXT("title"), Title);
        Args->SetStringField(TEXT("content"), Content);
        FString ArgsJson; auto W = TJsonWriterFactory<>::Create(&ArgsJson);
        FJsonSerializer::Serialize(Args, W);
        InvokeRoute(Env, TEXT("closeWithConfirmWindow"), ArgsJson, nullptr);
    }

    void FetchNotification()
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        InvokeRoute(Env, TEXT("fetchNotification"), TEXT(""), nullptr);
    }

    void SetCallback()
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;

        int64 NewId;
        {
            FScopeLock Lock(&GCallbackLock);
            if (GMomentProxyGlobal) { Env->DeleteGlobalRef(GMomentProxyGlobal); GMomentProxyGlobal = nullptr; }
            NewId = FTapAndroidCallbackId::Allocate();
            GMomentCallbackId = NewId;
        }

        jobject Proxy = CreateProxy(Env, NewId);  // 用局部变量，无 data race
        if (!Proxy)
        {
            UE_LOG(LogTapMoment, Error, TEXT("[TapMoment-Android] SetCallback: proxy creation failed"));
            return;
        }
        { FScopeLock Lock(&GCallbackLock); GMomentProxyGlobal = Proxy; }

        InvokeRoute(Env, TEXT("setCallback"), TEXT(""), Proxy);
        UE_LOG(LogTapMoment, Log, TEXT("[TapMoment-Android] setCallback registered, id=%lld"), NewId);
    }
}

#endif // PLATFORM_ANDROID
