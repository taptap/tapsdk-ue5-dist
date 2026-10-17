// Copyright TapTap. All Rights Reserved.
#include "Mobile/iOS/TapLeaderboardIOSBridge.h"

#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapLeaderboardModule.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"

// BridgeLeaderboardService 来自 TapTapLeaderboardSDK。
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
#import "BridgeLeaderboardService.h"
#endif

namespace
{
    FCriticalSection                        GLock;
    TapLeaderboardIOSBridge::FResultCallback GCallback;
    bool                                    GInitialized = false;

    NSString* ToNS(const FString& In)
    {
        return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*In)];
    }
    FString FromNS(NSString* In)
    {
        return In ? FString(UTF8_TO_TCHAR([In UTF8String])) : FString();
    }

    /**
     * 将 iOS async 结果 {"code":N,"content":"...","message":"..."} 规范化为 UE 协议格式。
     * TODO: content 字段内嵌套的 JSON schema 须与 Android TapJson.encodeToString 输出一致
     * （相同 key 命名：next_page / scores / score_result / current_user_score 等），
     * 否则 TapLeaderboardResultParser 会静默解析出空结果。需 iOS 团队确认后更新。
     */
    FString NormalizeAsync(const FString& Op, NSString* IOSResult)
    {
        FString Raw = FromNS(IOSResult);
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Raw);
        if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid())
        {
            TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
            E->SetStringField(TEXT("op"),         Op);
            E->SetStringField(TEXT("state"),      TEXT("failure"));
            E->SetNumberField(TEXT("errCode"),    -1);
            E->SetStringField(TEXT("errMessage"), TEXT("iOS parse failed"));
            FString Out; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out); FJsonSerializer::Serialize(E, W); return Out;
        }

        int32   Code = 0; Root->TryGetNumberField(TEXT("code"), Code);
        FString Msg  = TEXT("Success");  Root->TryGetStringField(TEXT("message"), Msg);
        FString Data = TEXT("");         Root->TryGetStringField(TEXT("content"), Data);

        TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("op"), Op);
        if (Code == 0)
        {
            Out->SetStringField(TEXT("state"), TEXT("success"));
            Out->SetStringField(TEXT("data"),  Data);
        }
        else
        {
            Out->SetStringField(TEXT("state"),      TEXT("failure"));
            Out->SetNumberField(TEXT("errCode"),    Code);
            Out->SetStringField(TEXT("errMessage"), Msg);
        }
        FString OutStr; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&OutStr); FJsonSerializer::Serialize(Out, W);
        return OutStr;
    }

    /** 规范化 leaderboard/share 持久回调 {"status":"success","data":{...}} */
    FString NormalizePersistent(const FString& Op, NSString* IOSResult)
    {
        FString Raw = FromNS(IOSResult);
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Raw);
        if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid())
            return FString::Printf(TEXT("{\"op\":\"%s\",\"state\":\"failure\",\"errCode\":-1,\"errMessage\":\"parse failed\"}"), *Op);

        FString Status; Root->TryGetStringField(TEXT("status"), Status);

        TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
        Out->SetStringField(TEXT("op"), Op);

        if (Status == TEXT("success"))
        {
            if (Op == TEXT("leaderboardCallback"))
            {
                // data is {"code":N,"message":"..."}
                const TSharedPtr<FJsonObject>* DataObj = nullptr;
                if (Root->TryGetObjectField(TEXT("data"), DataObj) && DataObj)
                {
                    int32   Code = 0; (*DataObj)->TryGetNumberField(TEXT("code"), Code);
                    FString Msg;      (*DataObj)->TryGetStringField(TEXT("message"), Msg);
                    Out->SetStringField(TEXT("state"),   TEXT("event"));
                    Out->SetNumberField(TEXT("code"),    Code);
                    Out->SetStringField(TEXT("message"), Msg);
                }
                else { Out->SetStringField(TEXT("state"), TEXT("event")); Out->SetNumberField(TEXT("code"), 0); Out->SetStringField(TEXT("message"), TEXT("")); }
            }
            else // shareCallback success: data is localPath string
            {
                FString Data; Root->TryGetStringField(TEXT("data"), Data);
                Out->SetStringField(TEXT("state"), TEXT("success"));
                Out->SetStringField(TEXT("data"),  Data);
            }
        }
        else // failure
        {
            int32   ErrCode = 0;      Root->TryGetNumberField(TEXT("errCode"), ErrCode);
            FString ErrMsg;           Root->TryGetStringField(TEXT("errMessage"), ErrMsg);
            Out->SetStringField(TEXT("state"),      TEXT("failure"));
            Out->SetNumberField(TEXT("errCode"),    ErrCode);
            Out->SetStringField(TEXT("errMessage"), ErrMsg);
        }
        FString OutStr; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&OutStr); FJsonSerializer::Serialize(Out, W);
        return OutStr;
    }

    void FireCallback(const FString& Json)
    {
        TapLeaderboardIOSBridge::FResultCallback Snapshot;
        { FScopeLock Lock(&GLock); Snapshot = GCallback; }
        if (Snapshot)
            AsyncTask(ENamedThreads::GameThread, [Snapshot, Json]() { Snapshot(Json); });
    }
}

namespace TapLeaderboardIOSBridge
{
    void Init(FResultCallback OnResult)
    {
        FScopeLock Lock(&GLock);
        if (GInitialized) { GCallback = MoveTemp(OnResult); return; }
        GInitialized = true;
        GCallback = MoveTemp(OnResult);
    }

    void OpenLeaderboard(const FString& LeaderboardId, const FString& Collection)
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        NSString* LeaderboardIdNS = ToNS(LeaderboardId);
        NSString* CollectionNS = ToNS(Collection);
        dispatch_async(dispatch_get_main_queue(), ^{
            [BridgeLeaderboardService leaderboardId:LeaderboardIdNS
                                leaderboardCollection:CollectionNS];
        });
#endif
    }

    void OpenUserProfile(const FString& OpenId, const FString& UnionId)
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        NSString* OpenIdNS = ToNS(OpenId);
        NSString* UnionIdNS = ToNS(UnionId);
        dispatch_async(dispatch_get_main_queue(), ^{
            [BridgeLeaderboardService openId:OpenIdNS unionId:UnionIdNS];
        });
#endif
    }

    void SubmitScores(const FString& ScoresJson)
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        NSString* NSScores = ToNS(ScoresJson);
        [BridgeLeaderboardService scores:NSScores callback:^(NSString* Result)
        {
            FireCallback(NormalizeAsync(TEXT("submitScores"), Result));
        }];
#endif
    }

    void LoadLeaderboardScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& NextPage, const FString& PeriodToken)
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        [BridgeLeaderboardService
            leaderboardId:ToNS(LeaderboardId)
            leaderboardCollection:ToNS(Collection)
            nextPage:(NextPage.IsEmpty() ? nil : ToNS(NextPage))
            periodToken:(PeriodToken.IsEmpty() ? nil : ToNS(PeriodToken))
            callback:^(NSString* Result)
        {
            FireCallback(NormalizeAsync(TEXT("loadScores"), Result));
        }];
#endif
    }

    void LoadCurrentPlayerLeaderboardScore(
        const FString& LeaderboardId, const FString& Collection, const FString& PeriodToken)
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        [BridgeLeaderboardService
            leaderboardId:ToNS(LeaderboardId)
            leaderboardCollection:ToNS(Collection)
            periodToken:(PeriodToken.IsEmpty() ? nil : ToNS(PeriodToken))
            callback:^(NSString* Result)
        {
            FireCallback(NormalizeAsync(TEXT("loadCurrentPlayerScore"), Result));
        }];
#endif
    }

    void LoadPlayerCenteredScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken, int32 MaxCount)
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        NSNumber* MaxCountNum = (MaxCount > 0) ? @(MaxCount) : nil;
        [BridgeLeaderboardService
            leaderboardId:ToNS(LeaderboardId)
            leaderboardCollection:ToNS(Collection)
            periodToken:(PeriodToken.IsEmpty() ? nil : ToNS(PeriodToken))
            maxCount:MaxCountNum
            callback:^(NSString* Result)
        {
            FireCallback(NormalizeAsync(TEXT("loadPlayerCenteredScores"), Result));
        }];
#endif
    }

    void RegisterLeaderboardCallback()
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        [BridgeLeaderboardService registerLeaderboardCallback:^(NSString* Result)
        {
            FireCallback(NormalizePersistent(TEXT("leaderboardCallback"), Result));
        }];
#endif
    }

    void UnregisterLeaderboardCallback()
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        [BridgeLeaderboardService unregisterLeaderboardCallback];
#endif
    }

    void SetShareCallback()
    {
#if TAP_LEADERBOARD_IOS_SDK_AVAILABLE
        [BridgeLeaderboardService setShareCallback:^(NSString* Result)
        {
            FireCallback(NormalizePersistent(TEXT("shareCallback"), Result));
        }];
#endif
    }
}

#endif // PLATFORM_IOS
