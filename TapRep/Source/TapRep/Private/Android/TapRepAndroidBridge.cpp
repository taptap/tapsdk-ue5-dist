// Copyright TapTap. All Rights Reserved.
#include "Android/TapRepAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapRepModule.h"
#include "TapRepTypes.h"
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
    std::atomic<jmethodID>      GRouteMethodID{ nullptr };
    FCriticalSection            GCallbackLock;
    TMap<int64,TapRepAndroidBridge::FRepCallback> GPendingCallbacks;
    bool                        GInitialized = false;

    // 创建 EngineBridgeCallbackProxy(id)
    jobject CreateProxy(JNIEnv* Env, int64 Id)
    {
        jclass ProxyClass = FAndroidApplication::FindJavaClass(
            "com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy");
        if (!ProxyClass || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapRep, Error, TEXT("[TapRep-Android] EngineBridgeCallbackProxy not found"));
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

    // 构造 {"method":"xxx","args":"<json string>"} 并调用 routeToTapRep(json, callback)
    bool InvokeRoute(JNIEnv* Env, const FString& Method, const FString& ArgsJson, jobject CallbackProxy)
    {
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity)
        {
            UE_LOG(LogTapRep, Error, TEXT("[TapRep-Android] GameActivity null"));
            return false;
        }

        jmethodID RouteMethod = GRouteMethodID.load(std::memory_order_acquire);
        if (RouteMethod == nullptr)
        {
            jclass Cls = Env->GetObjectClass(Activity);
            RouteMethod = Env->GetMethodID(
                Cls, "routeToTapRep",
                "(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)V");
            Env->DeleteLocalRef(Cls);
            if (!RouteMethod || Env->ExceptionCheck())
            {
                if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
                UE_LOG(LogTapRep, Error, TEXT("[TapRep-Android] routeToTapRep not found"));
                return false;
            }
            GRouteMethodID.store(RouteMethod, std::memory_order_release);
        }

        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("method"), Method);
        Root->SetStringField(TEXT("args"), ArgsJson);
        FString ReqJson;
        auto Writer = TJsonWriterFactory<>::Create(&ReqJson);
        FJsonSerializer::Serialize(Root, Writer);

        jstring JReq = Env->NewStringUTF(TCHAR_TO_UTF8(*ReqJson));
        Env->CallVoidMethod(Activity, RouteMethod, JReq, CallbackProxy);
        const bool bSucceeded = !Env->ExceptionCheck();
        if (!bSucceeded)
        {
            Env->ExceptionDescribe();
            Env->ExceptionClear();
        }
        Env->DeleteLocalRef(JReq);
        return bSucceeded;
    }

    // GlobalCallbackHandler 回调入口；ResultJson = {"code":N,"message":..,"content":null}
    bool DispatchRepCallback(int64 CallbackId, const FString& ResultJson)
    {
        TapRepAndroidBridge::FRepCallback Snapshot;
        {
            FScopeLock Lock(&GCallbackLock);
            if (CallbackId == 0) { return false; }
            TapRepAndroidBridge::FRepCallback* Found=GPendingCallbacks.Find(CallbackId);if(!Found)return false;Snapshot=MoveTemp(*Found);GPendingCallbacks.Remove(CallbackId);
        }
        if (!Snapshot) { return true; }

        int32 Code = ETapRepError::UNKNOWN_ERROR;
        FString Msg;
        TSharedPtr<FJsonObject> Root;
        auto Reader = TJsonReaderFactory<>::Create(ResultJson);
        if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
        {
            double CodeNum = 0.0;
            if (Root->TryGetNumberField(TEXT("code"), CodeNum))
                Code = static_cast<int32>(CodeNum);
            Root->TryGetStringField(TEXT("message"), Msg);
        }

        AsyncTask(ENamedThreads::GameThread, [Snapshot, Code, Msg]()
        {
            Snapshot(Code, Msg);
        });
        return true;
    }
}

namespace TapRepAndroidBridge
{
    void Init()
    {
        {
            FScopeLock Lock(&GCallbackLock);
            if (GInitialized) { return; }
            GInitialized = true;
        }
        FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
        {
            return DispatchRepCallback(CallbackId, ResultJson);
        });
        UE_LOG(LogTapRep, Log, TEXT("[TapRep-Android] Bridge initialized"));
    }

    void Open(const FString& OpenUrl, FRepCallback Callback)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { UE_LOG(LogTapRep, Error, TEXT("[TapRep-Android] JNI Env null")); return; }

        int64 NewId;
        {
            FScopeLock Lock(&GCallbackLock);
            NewId = FTapAndroidCallbackId::Allocate();
            GPendingCallbacks.Add(NewId,MoveTemp(Callback));
        }

        jobject Proxy = CreateProxy(Env, NewId);
        if (!Proxy)
        {
            UE_LOG(LogTapRep, Error, TEXT("[TapRep-Android] Open: proxy creation failed"));
            DispatchRepCallback(
                NewId,
                TEXT("{\"code\":-1,\"message\":\"EngineBridgeCallbackProxy creation failed\"}"));
            return;
        }

        TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
        Args->SetStringField(TEXT("openUrl"), OpenUrl);
        FString ArgsJson;
        auto W = TJsonWriterFactory<>::Create(&ArgsJson);
        FJsonSerializer::Serialize(Args, W);

        const bool bRouted =
            InvokeRoute(Env, TEXT("open"), ArgsJson, Proxy);

        // routeToTapRep 内部已把 proxy 透传给原生 service；此处用完即释放 global ref
        Env->DeleteGlobalRef(Proxy);

        if (!bRouted)
        {
            DispatchRepCallback(
                NewId,
                TEXT("{\"code\":-1,\"message\":\"routeToTapRep invocation failed\"}"));
            return;
        }
        UE_LOG(
            LogTapRep,
            Log,
            TEXT("[TapRep-Android] Open routed, id=%lld"),
            NewId);
    }
}

#endif // PLATFORM_ANDROID
