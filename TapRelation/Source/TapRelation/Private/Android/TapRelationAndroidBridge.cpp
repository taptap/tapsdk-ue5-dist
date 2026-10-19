// Copyright TapTap. All Rights Reserved.
#include "Android/TapRelationAndroidBridge.h"
#include "TapRelationInviteParser.h"

#if PLATFORM_ANDROID

#include "TapRelationModule.h"
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
    struct FRelationCallbackEntry
    {
        TFunction<void(const FString&)> Callback;
        jobject                         ProxyGlobal = nullptr;
        bool                            bPersistent = false;
    };

    FCriticalSection                    GCallbackLock;
    TMap<int64, FRelationCallbackEntry> GCallbackRegistry;
    int64                               GPersistentId      = 0;
    int64                               GInvitePersistentId = 0;
    bool                                GInitialized       = false;
    std::atomic<jmethodID>              GRouteMethodID{ nullptr };

    jobject CreateProxy(JNIEnv* Env, int64 Id)
    {
        // UE native threads must use the app ClassLoader to resolve SDK classes.
        jclass Cls = FAndroidApplication::FindJavaClass(
            "com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy");
        if (!Cls || Env->ExceptionCheck()) { if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); } return nullptr; }
        jmethodID Ctor = Env->GetMethodID(Cls, "<init>", "(J)V");
        if (!Ctor || Env->ExceptionCheck()) { if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); } Env->DeleteLocalRef(Cls); return nullptr; }
        jobject Local = Env->NewObject(Cls, Ctor, static_cast<jlong>(Id));
        Env->DeleteLocalRef(Cls);
        if (!Local || Env->ExceptionCheck()) { if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); } return nullptr; }
        jobject Global = Env->NewGlobalRef(Local);
        Env->DeleteLocalRef(Local);
        return Global;
    }

    bool DispatchCallback(int64 Id, const FString& ResultJson)
    {
        TFunction<void(const FString&)> Cb;
        jobject ProxyToRelease = nullptr;
        {
            FScopeLock Lock(&GCallbackLock);
            FRelationCallbackEntry* E = GCallbackRegistry.Find(Id);
            if (!E) { return false; }
            Cb = E->Callback;
            if (!E->bPersistent) { ProxyToRelease = E->ProxyGlobal; GCallbackRegistry.Remove(Id); }
        }
        if (Cb) { AsyncTask(ENamedThreads::GameThread, [Cb, ResultJson]() { Cb(ResultJson); }); }
        if (ProxyToRelease) { if (JNIEnv* E = FAndroidApplication::GetJavaEnv()) E->DeleteGlobalRef(ProxyToRelease); }
        return true;
    }

    void InvokeRoute(JNIEnv* Env, const FString& Method, const FString& ArgsJson, jobject CallbackProxy)
    {
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapRelation, Error, TEXT("[TapRelation-Android] GameActivity null")); return; }

        jmethodID RouteMethod = GRouteMethodID.load(std::memory_order_acquire);
        if (!RouteMethod)
        {
            jclass Cls = Env->GetObjectClass(Activity);
            RouteMethod = Env->GetMethodID(Cls, "routeToTapRelation",
                "(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)V");
            Env->DeleteLocalRef(Cls);
            if (!RouteMethod || Env->ExceptionCheck())
            { if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
              UE_LOG(LogTapRelation, Error, TEXT("[TapRelation-Android] routeToTapRelation not found")); return; }
            GRouteMethodID.store(RouteMethod, std::memory_order_release);
        }

        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("method"), Method);
        Root->SetStringField(TEXT("args"), ArgsJson);
        FString ReqJson; auto W = TJsonWriterFactory<>::Create(&ReqJson);
        FJsonSerializer::Serialize(Root, W);

        jstring JReq = Env->NewStringUTF(TCHAR_TO_UTF8(*ReqJson));
        Env->CallVoidMethod(Activity, RouteMethod, JReq, CallbackProxy);
        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        Env->DeleteLocalRef(JReq);
    }

    int64 RegisterOneShot(JNIEnv* Env, TFunction<void(const FString&)> Cb)
    {
        int64 Id;
        { FScopeLock Lock(&GCallbackLock); Id = FTapAndroidCallbackId::Allocate(); GCallbackRegistry.Add(Id, {MoveTemp(Cb), nullptr, false}); }
        jobject Proxy = CreateProxy(Env, Id);
        if (!Proxy) { FScopeLock Lock(&GCallbackLock); GCallbackRegistry.Remove(Id); return -1; }
        { FScopeLock Lock(&GCallbackLock); if (auto* E = GCallbackRegistry.Find(Id)) E->ProxyGlobal = Proxy; }
        return Id;
    }
}

namespace TapRelationAndroidBridge
{
    void Init()
    {
        { FScopeLock Lock(&GCallbackLock); if (GInitialized) return; GInitialized = true; }
        FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 Id, const FString& Result) -> bool
        {
            return DispatchCallback(Id, Result);
        });
        UE_LOG(LogTapRelation, Log, TEXT("[TapRelation-Android] Bridge initialized"));
    }

    void Prepare()
    { JNIEnv* E = FAndroidApplication::GetJavaEnv(); if (E) InvokeRoute(E, TEXT("prepare"), TEXT(""), nullptr); }

    void StartMessenger()
    { JNIEnv* E = FAndroidApplication::GetJavaEnv(); if (E) InvokeRoute(E, TEXT("startMessenger"), TEXT(""), nullptr); }

    void InviteGame()
    { JNIEnv* E = FAndroidApplication::GetJavaEnv(); if (E) InvokeRoute(E, TEXT("inviteGame"), TEXT(""), nullptr); }

    void InviteTeam(const FString& TeamId)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("teamId"), TeamId);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        InvokeRoute(Env, TEXT("inviteTeam"), Aj, nullptr);
    }

    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
        A->SetStringField(TEXT("openId"), OpenId); A->SetStringField(TEXT("unionId"), UnionId);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        InvokeRoute(Env, TEXT("showTapUserProfile"), Aj, nullptr);
    }

    void GetNewFansCount(TFunction<void(int32)> Callback)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [Callback](const FString& Json)
        {
            TSharedPtr<FJsonObject> Root; auto R = TJsonReaderFactory<>::Create(Json);
            int32 Count = 0;
            if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
            { double V = 0; if (Root->TryGetNumberField(TEXT("new_fans_count"), V)) Count = static_cast<int32>(V); }
            if (Callback) Callback(Count);
        };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (Callback) Callback(0); return; }
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("getNewFansCount"), TEXT(""), Proxy);
    }

    void GetUnreadMessageCount(TFunction<void(int32)> Callback)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [Callback](const FString& Json)
        {
            TSharedPtr<FJsonObject> Root; auto R = TJsonReaderFactory<>::Create(Json);
            int32 Count = 0;
            if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
            { double V = 0; if (Root->TryGetNumberField(TEXT("unread_message_count"), V)) Count = static_cast<int32>(V); }
            if (Callback) Callback(Count);
        };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (Callback) Callback(0); return; }
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("getUnreadMessageCount"), TEXT(""), Proxy);
    }

    void RegisterRelationCallback(TFunction<void(int32, int32, int32)> Callback)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;

        int64 NewId;
        {
            FScopeLock Lock(&GCallbackLock);
            if (GPersistentId && GCallbackRegistry.Contains(GPersistentId))
            { auto* E = GCallbackRegistry.Find(GPersistentId);
              if (E && E->ProxyGlobal) { Env->DeleteGlobalRef(E->ProxyGlobal); }
              GCallbackRegistry.Remove(GPersistentId); }
            NewId = FTapAndroidCallbackId::Allocate();
            GPersistentId = NewId;
        }

        TFunction<void(const FString&)> Handler = [Callback](const FString& Json)
        {
            TSharedPtr<FJsonObject> Root; auto R = TJsonReaderFactory<>::Create(Json);
            int32 Code = 0, Fans = 0, Unread = 0;
            if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
            {
                FString CodeStr; double V = 0;
                if (Root->TryGetStringField(TEXT("relation_code"), CodeStr)) Code = FCString::Atoi(*CodeStr);
                if (Root->TryGetNumberField(TEXT("new_fans_count"), V))       Fans   = static_cast<int32>(V);
                if (Root->TryGetNumberField(TEXT("unread_message_count"), V)) Unread = static_cast<int32>(V);
            }
            if (Callback) Callback(Code, Fans, Unread);
        };

        { FScopeLock Lock(&GCallbackLock); GCallbackRegistry.Add(NewId, {MoveTemp(Handler), nullptr, true}); }
        jobject Proxy = CreateProxy(Env, NewId);
        if (!Proxy) { FScopeLock Lock(&GCallbackLock); GCallbackRegistry.Remove(NewId); GPersistentId = 0; return; }
        { FScopeLock Lock(&GCallbackLock); if (auto* E = GCallbackRegistry.Find(NewId)) E->ProxyGlobal = Proxy; }

        InvokeRoute(Env, TEXT("registerRelationCallback"), TEXT(""), Proxy);
        UE_LOG(LogTapRelation, Log, TEXT("[TapRelation-Android] registerRelationCallback registered, id=%lld"), NewId);
    }

    void UnregisterRelationRelationCallbackImpl(int64& Id, const TCHAR* Method)
    {
        JNIEnv* Env=FAndroidApplication::GetJavaEnv(); if(!Env) return;
        { FScopeLock Lock(&GCallbackLock); if(Id){ if(auto* E=GCallbackRegistry.Find(Id)) if(E->ProxyGlobal) Env->DeleteGlobalRef(E->ProxyGlobal); GCallbackRegistry.Remove(Id); Id=0; } }
        InvokeRoute(Env, Method, TEXT(""), nullptr);
    }
    void UnregisterRelationCallback(){ UnregisterRelationRelationCallbackImpl(GPersistentId, TEXT("unregisterRelationCallback")); }

    void RegisterRelationInviteCallback(TFunction<void(const FString&, const FString&, const FString&)> Callback)
    {
        JNIEnv* Env=FAndroidApplication::GetJavaEnv(); if(!Env) return;
        UnregisterRelationRelationCallbackImpl(GInvitePersistentId, TEXT("unregisterRelationInviteCallback"));
        int64 Id; { FScopeLock Lock(&GCallbackLock); Id=FTapAndroidCallbackId::Allocate(); GInvitePersistentId=Id;
            GCallbackRegistry.Add(Id,{[Callback](const FString& Json){ FString O,U,T;
                if (!TapRelationInviteParser::Parse(Json, O, U, T)) return;
                if(Callback) Callback(O,U,T); },nullptr,true}); }
        jobject Proxy=CreateProxy(Env,Id); if(!Proxy) return;
        { FScopeLock Lock(&GCallbackLock); if(auto* E=GCallbackRegistry.Find(Id)) E->ProxyGlobal=Proxy; }
        InvokeRoute(Env,TEXT("registerRelationInviteCallback"),TEXT(""),Proxy);
    }
    void UnregisterRelationInviteCallback(){ UnregisterRelationRelationCallbackImpl(GInvitePersistentId, TEXT("unregisterRelationInviteCallback")); }

    void Destroy()
    { UnregisterRelationCallback(); UnregisterRelationInviteCallback(); JNIEnv* E = FAndroidApplication::GetJavaEnv(); if (E) InvokeRoute(E, TEXT("destroy"), TEXT(""), nullptr); }
}

#endif // PLATFORM_ANDROID
