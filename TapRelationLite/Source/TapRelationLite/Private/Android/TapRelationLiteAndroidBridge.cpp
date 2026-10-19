// Copyright TapTap. All Rights Reserved.
#include "Android/TapRelationLiteAndroidBridge.h"
#include "TapRelationLiteInviteParser.h"

#if PLATFORM_ANDROID

#include "TapRelationLiteModule.h"
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
    struct FRelationLiteCallbackEntry
    {
        TFunction<void(const FString&)> Callback;
        jobject                         ProxyGlobal = nullptr;
        bool                            bPersistent = false;
    };

    FCriticalSection                          GCallbackLock;
    TMap<int64, FRelationLiteCallbackEntry>   GCallbackRegistry;
    int64                                     GPersistentId     = 0;
    int64                                     GInvitePersistentId = 0;
    bool                                      GInitialized      = false;
    std::atomic<jmethodID>                    GRouteMethodID{ nullptr };

    // ---------- CreateProxy ----------
    jobject CreateProxy(JNIEnv* Env, int64 Id)
    {
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

    // ---------- DispatchCallback ----------
    bool DispatchCallback(int64 Id, const FString& ResultJson)
    {
        TFunction<void(const FString&)> Cb;
        jobject ProxyToRelease = nullptr;
        {
            FScopeLock Lock(&GCallbackLock);
            FRelationLiteCallbackEntry* E = GCallbackRegistry.Find(Id);
            if (!E) return false;
            Cb = E->Callback;
            if (!E->bPersistent) { ProxyToRelease = E->ProxyGlobal; GCallbackRegistry.Remove(Id); }
        }
        if (Cb) { AsyncTask(ENamedThreads::GameThread, [Cb, ResultJson]() { Cb(ResultJson); }); }
        if (ProxyToRelease) { if (JNIEnv* E = FAndroidApplication::GetJavaEnv()) E->DeleteGlobalRef(ProxyToRelease); }
        return true;
    }

    // ---------- InvokeRoute ----------
    void InvokeRoute(JNIEnv* Env, const FString& Method, const FString& ArgsJson, jobject CallbackProxy)
    {
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapRelationLite, Error, TEXT("[TapRelationLite-Android] GameActivity null")); return; }

        jmethodID RouteMethod = GRouteMethodID.load(std::memory_order_acquire);
        if (!RouteMethod)
        {
            jclass Cls = Env->GetObjectClass(Activity);
            RouteMethod = Env->GetMethodID(Cls, "routeToTapRelationLite",
                "(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)V");
            Env->DeleteLocalRef(Cls);
            if (!RouteMethod || Env->ExceptionCheck())
            { if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
              UE_LOG(LogTapRelationLite, Error, TEXT("[TapRelationLite-Android] routeToTapRelationLite not found")); return; }
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

    // ---------- RegisterOneShot ----------
    int64 RegisterOneShot(JNIEnv* Env, TFunction<void(const FString&)> Cb)
    {
        int64 Id;
        { FScopeLock Lock(&GCallbackLock); Id = FTapAndroidCallbackId::Allocate(); GCallbackRegistry.Add(Id, {MoveTemp(Cb), nullptr, false}); }
        jobject Proxy = CreateProxy(Env, Id);
        if (!Proxy) { FScopeLock Lock(&GCallbackLock); GCallbackRegistry.Remove(Id); return -1; }
        { FScopeLock Lock(&GCallbackLock); if (auto* E = GCallbackRegistry.Find(Id)) E->ProxyGlobal = Proxy; }
        return Id;
    }

    // ---------- ParseUserList ----------
    // Parses {"success":0,"next_page_token":"...","<listKey>":"[{...}]"}
    // listKey: friends_list / following_list / fans_list
    void ParseUserList(const FString& Json, const FString& ListKey,
        TapRelationLiteAndroidBridge::FListSuccess OnSuccess,
        TapRelationLiteAndroidBridge::FListFailure OnFailure)
    {
        TSharedPtr<FJsonObject> Root; auto R = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid())
        { if (OnFailure) OnFailure(-1, TEXT("Parse error")); return; }

        double SuccessNum = 0; Root->TryGetNumberField(TEXT("success"), SuccessNum);
        if (static_cast<int32>(SuccessNum) != 0)
        {
            FString ErrMsg; Root->TryGetStringField(TEXT("error_message"), ErrMsg);
            if (OnFailure) OnFailure(static_cast<int32>(SuccessNum), ErrMsg);
            return;
        }

        FString NextPageToken; Root->TryGetStringField(TEXT("next_page_token"), NextPageToken);
        FString ListJsonStr;   Root->TryGetStringField(ListKey, ListJsonStr);

        FTapRelationLiteUserResult Result;
        Result.NextPageToken = NextPageToken;

        if (!ListJsonStr.IsEmpty())
        {
            TArray<TSharedPtr<FJsonValue>> Items;
            auto AR = TJsonReaderFactory<>::Create(ListJsonStr);
            if (FJsonSerializer::Deserialize(AR, Items))
            {
                for (auto& V : Items)
                {
                    auto Obj = V->AsObject();
                    if (!Obj.IsValid()) continue;
                    FTapRelationLiteUserItem Item;
                    Item.CreatedAt = static_cast<int64>(Obj->GetNumberField(TEXT("created_at")));

                    auto UserObj = Obj->GetObjectField(TEXT("user"));
                    if (UserObj.IsValid())
                    {
                        UserObj->TryGetStringField(TEXT("alias"),   Item.User.Alias);
                        UserObj->TryGetStringField(TEXT("avatar"),  Item.User.Avatar);
                        UserObj->TryGetStringField(TEXT("name"),    Item.User.Name);
                        UserObj->TryGetStringField(TEXT("open_id"), Item.User.OpenId);
                    }

                    auto FsObj = Obj->GetObjectField(TEXT("follow_status"));
                    if (FsObj.IsValid())
                    {
                        FsObj->TryGetBoolField(TEXT("blocked_by"),  Item.FollowStatus.bBlockedBy);
                        FsObj->TryGetBoolField(TEXT("blocking"),    Item.FollowStatus.bBlocking);
                        FsObj->TryGetBoolField(TEXT("followed_by"), Item.FollowStatus.bFollowedBy);
                        FsObj->TryGetBoolField(TEXT("following"),   Item.FollowStatus.bFollowing);
                    }
                    Result.Users.Add(Item);
                }
            }
        }
        if (OnSuccess) OnSuccess(Result);
    }

    // ---------- ParseSyncResult ----------
    void ParseSyncResult(const FString& Json,
        TapRelationLiteAndroidBridge::FSyncSuccess OnSuccess,
        TapRelationLiteAndroidBridge::FSyncFailure OnFailure)
    {
        TSharedPtr<FJsonObject> Root; auto R = TJsonReaderFactory<>::Create(Json);
        double SuccessNum = -1;
        if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
            Root->TryGetNumberField(TEXT("success"), SuccessNum);
        if (static_cast<int32>(SuccessNum) == 0)
        { if (OnSuccess) OnSuccess(); }
        else
        {
            FString ErrMsg;
            if (Root.IsValid()) Root->TryGetStringField(TEXT("error_message"), ErrMsg);
            if (OnFailure) OnFailure(ErrMsg);
        }
    }
}

namespace TapRelationLiteAndroidBridge
{
    void Init()
    {
        { FScopeLock Lock(&GCallbackLock); if (GInitialized) return; GInitialized = true; }
        FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 Id, const FString& Result) -> bool
        {
            return DispatchCallback(Id, Result);
        });
        UE_LOG(LogTapRelationLite, Log, TEXT("[TapRelationLite-Android] Bridge initialized"));
    }

    void InviteGame()
    { JNIEnv* E = FAndroidApplication::GetJavaEnv(); if (E) InvokeRoute(E, TEXT("inviteGameByRelationLite"), TEXT(""), nullptr); }

    void InviteTeam(const FString& TeamId)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("relationLiteTeamId"), TeamId);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        InvokeRoute(Env, TEXT("inviteTeam"), Aj, nullptr);
    }

    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
        A->SetStringField(TEXT("relationLiteOpenId"), OpenId); A->SetStringField(TEXT("unionId"), UnionId);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        InvokeRoute(Env, TEXT("showTapUserProfile"), Aj, nullptr);
    }

    void GetFriendsList(const FString& NextPageToken, FListSuccess OnSuccess, FListFailure OnFailure)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [OnSuccess, OnFailure](const FString& Json)
        { ParseUserList(Json, TEXT("friends_list"), OnSuccess, OnFailure); };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (OnFailure) OnFailure(-1, TEXT("proxy creation failed")); return; }

        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("friendListNextPageToken"), NextPageToken);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("getFriendsList"), Aj, Proxy);
    }

    void GetFollowingList(const FString& NextPageToken, FListSuccess OnSuccess, FListFailure OnFailure)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [OnSuccess, OnFailure](const FString& Json)
        { ParseUserList(Json, TEXT("following_list"), OnSuccess, OnFailure); };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (OnFailure) OnFailure(-1, TEXT("proxy creation failed")); return; }

        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("followingListNextPageToken"), NextPageToken);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("getFollowingList"), Aj, Proxy);
    }

    void GetFansList(const FString& NextPageToken, FListSuccess OnSuccess, FListFailure OnFailure)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [OnSuccess, OnFailure](const FString& Json)
        { ParseUserList(Json, TEXT("fans_list"), OnSuccess, OnFailure); };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (OnFailure) OnFailure(-1, TEXT("proxy creation failed")); return; }

        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("fansListNextPageToken"), NextPageToken);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("getFansList"), Aj, Proxy);
    }

    void SyncRelationshipWithOpenId(int32 Action, const FString& Nickname, const FString& FriendNickname, const FString& FriendOpenId, FSyncSuccess OnSuccess, FSyncFailure OnFailure)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [OnSuccess, OnFailure](const FString& Json)
        { ParseSyncResult(Json, OnSuccess, OnFailure); };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (OnFailure) OnFailure(TEXT("proxy creation failed")); return; }

        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
        A->SetNumberField(TEXT("syncRelationAction"), Action);
        A->SetStringField(TEXT("nickname"), Nickname);
        A->SetStringField(TEXT("friendNickname"), FriendNickname);
        A->SetStringField(TEXT("friendOpenId"), FriendOpenId);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("syncRelationshipWithOpenId"), Aj, Proxy);
    }

    void SyncRelationshipWithUnionId(int32 Action, const FString& Nickname, const FString& FriendNickname, const FString& FriendUnionId, FSyncSuccess OnSuccess, FSyncFailure OnFailure)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        TFunction<void(const FString&)> Handler = [OnSuccess, OnFailure](const FString& Json)
        { ParseSyncResult(Json, OnSuccess, OnFailure); };
        int64 Id = RegisterOneShot(Env, MoveTemp(Handler));
        if (Id < 0) { if (OnFailure) OnFailure(TEXT("proxy creation failed")); return; }

        TSharedRef<FJsonObject> A = MakeShared<FJsonObject>();
        A->SetNumberField(TEXT("syncRelationAction"), Action);
        A->SetStringField(TEXT("nickname"), Nickname);
        A->SetStringField(TEXT("friendNickname"), FriendNickname);
        A->SetStringField(TEXT("friendUnionId"), FriendUnionId);
        FString Aj; auto W = TJsonWriterFactory<>::Create(&Aj); FJsonSerializer::Serialize(A, W);
        jobject Proxy; { FScopeLock Lock(&GCallbackLock); auto* E = GCallbackRegistry.Find(Id); Proxy = E ? E->ProxyGlobal : nullptr; }
        if (Proxy) InvokeRoute(Env, TEXT("syncRelationshipWithUnionId"), Aj, Proxy);
    }

    void RegisterRelationLiteCallback(TFunction<void(int32)> Callback)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        { FScopeLock Lock(&GCallbackLock);
          if (GPersistentId && GCallbackRegistry.Contains(GPersistentId))
          { auto* E = GCallbackRegistry.Find(GPersistentId);
            if (E && E->ProxyGlobal) { Env->DeleteGlobalRef(E->ProxyGlobal); }
            GCallbackRegistry.Remove(GPersistentId); }
          GPersistentId = FTapAndroidCallbackId::Allocate(); }

        TFunction<void(const FString&)> Handler = [Callback](const FString& Json)
        {
            TSharedPtr<FJsonObject> Root; auto R = TJsonReaderFactory<>::Create(Json);
            int32 Code = 0;
            if (FJsonSerializer::Deserialize(R, Root) && Root.IsValid())
            { FString S; if (Root->TryGetStringField(TEXT("relation_lite_result_code"), S)) Code = FCString::Atoi(*S); }
            if (Callback) Callback(Code);
        };

        { FScopeLock Lock(&GCallbackLock); GCallbackRegistry.Add(GPersistentId, {MoveTemp(Handler), nullptr, true}); }
        jobject Proxy = CreateProxy(Env, GPersistentId);
        if (!Proxy) { FScopeLock Lock(&GCallbackLock); GCallbackRegistry.Remove(GPersistentId); GPersistentId = 0; return; }
        { FScopeLock Lock(&GCallbackLock); if (auto* E = GCallbackRegistry.Find(GPersistentId)) E->ProxyGlobal = Proxy; }

        InvokeRoute(Env, TEXT("registerRelationLiteCallback"), TEXT(""), Proxy);
        UE_LOG(LogTapRelationLite, Log, TEXT("[TapRelationLite-Android] registerRelationLiteCallback registered"));
    }

    void UnregisterRelationLiteCallback()
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv(); if (!Env) return;
        { FScopeLock Lock(&GCallbackLock);
          if (GPersistentId && GCallbackRegistry.Contains(GPersistentId))
          { auto* E = GCallbackRegistry.Find(GPersistentId);
            if (E && E->ProxyGlobal) { Env->DeleteGlobalRef(E->ProxyGlobal); }
            GCallbackRegistry.Remove(GPersistentId); GPersistentId = 0; } }
        InvokeRoute(Env, TEXT("unregisterRelationLiteCallback"), TEXT(""), nullptr);
    }

    void RegisterRelationLiteInviteCallback(FInviteCallback Callback)
    {
        JNIEnv* Env=FAndroidApplication::GetJavaEnv(); if(!Env)return;
        UnregisterRelationLiteInviteCallback();
        int64 Id; {FScopeLock Lock(&GCallbackLock);Id=FTapAndroidCallbackId::Allocate();GInvitePersistentId=Id;
            GCallbackRegistry.Add(Id,{[Callback](const FString& Json){FString O,U,T;
                if (TapRelationLiteInviteParser::Parse(Json, O, U, T) && Callback) Callback(O,U,T);},nullptr,true});}
        jobject Proxy=CreateProxy(Env,Id);if(!Proxy)return;{FScopeLock Lock(&GCallbackLock);if(auto*E=GCallbackRegistry.Find(Id))E->ProxyGlobal=Proxy;}
        InvokeRoute(Env,TEXT("registerRelationLiteInviteCallback"),TEXT(""),Proxy);
    }
    void UnregisterRelationLiteInviteCallback()
    {
        JNIEnv* Env=FAndroidApplication::GetJavaEnv();if(!Env)return;{FScopeLock Lock(&GCallbackLock);if(GInvitePersistentId){if(auto*E=GCallbackRegistry.Find(GInvitePersistentId))if(E->ProxyGlobal)Env->DeleteGlobalRef(E->ProxyGlobal);GCallbackRegistry.Remove(GInvitePersistentId);GInvitePersistentId=0;}}
        InvokeRoute(Env,TEXT("unregisterRelationLiteInviteCallback"),TEXT(""),nullptr);
    }
}

#endif // PLATFORM_ANDROID
