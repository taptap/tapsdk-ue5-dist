// TapRelationLitePlatformAndroid.cpp
// Copyright TapTap. All Rights Reserved.
#include "Android/TapRelationLitePlatformAndroid.h"
#if PLATFORM_ANDROID
#include "Android/TapRelationLiteAndroidBridge.h"
#include "TapRelationLiteCallbackManager.h"
#include "TapRelationLiteModule.h"

void FTapRelationLitePlatformAndroid::Initialize(const FTapTapSdkOptions&) { TapRelationLiteAndroidBridge::Init(); }
void FTapRelationLitePlatformAndroid::InviteGame() { TapRelationLiteAndroidBridge::InviteGame(); }
void FTapRelationLitePlatformAndroid::InviteTeam(const FString& T) { TapRelationLiteAndroidBridge::InviteTeam(T); }
void FTapRelationLitePlatformAndroid::ShowTapUserProfile(const FString& O, const FString& U) { TapRelationLiteAndroidBridge::ShowTapUserProfile(O, U); }
void FTapRelationLitePlatformAndroid::UnregisterRelationLiteCallback() { TapRelationLiteAndroidBridge::UnregisterRelationLiteCallback(); }

void FTapRelationLitePlatformAndroid::GetFriendsList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF)
{ TapRelationLiteAndroidBridge::GetFriendsList(T, MoveTemp(OS), MoveTemp(OF)); }

void FTapRelationLitePlatformAndroid::GetFollowingList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF)
{ TapRelationLiteAndroidBridge::GetFollowingList(T, MoveTemp(OS), MoveTemp(OF)); }

void FTapRelationLitePlatformAndroid::GetFansList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF)
{ TapRelationLiteAndroidBridge::GetFansList(T, MoveTemp(OS), MoveTemp(OF)); }

void FTapRelationLitePlatformAndroid::SyncRelationshipWithOpenId(int32 A, const FString& N, const FString& FN, const FString& OId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF)
{ TapRelationLiteAndroidBridge::SyncRelationshipWithOpenId(A, N, FN, OId, MoveTemp(OS), MoveTemp(OF)); }

void FTapRelationLitePlatformAndroid::SyncRelationshipWithUnionId(int32 A, const FString& N, const FString& FN, const FString& UId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF)
{ TapRelationLiteAndroidBridge::SyncRelationshipWithUnionId(A, N, FN, UId, MoveTemp(OS), MoveTemp(OF)); }

void FTapRelationLitePlatformAndroid::RegisterRelationLiteCallback()
{
    TapRelationLiteAndroidBridge::RegisterRelationLiteCallback([](int32 Code)
    {
        FTapRelationLiteCallbackManager::Get().DispatchResult(Code);
    });
}
void FTapRelationLitePlatformAndroid::RegisterRelationLiteInviteCallback(){TapRelationLiteAndroidBridge::RegisterRelationLiteInviteCallback([](const FString&O,const FString&U,const FString&T){FTapRelationLiteCallbackManager::Get().DispatchInvite(O,U,T);});}
void FTapRelationLitePlatformAndroid::UnregisterRelationLiteInviteCallback(){TapRelationLiteAndroidBridge::UnregisterRelationLiteInviteCallback();}
#endif
