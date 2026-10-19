// TapRelationLitePlatformIOS.mm
// Copyright TapTap. All Rights Reserved.
#include "iOS/TapRelationLitePlatformIOS.h"
#if PLATFORM_IOS
#include "iOS/TapRelationLiteIOSBridge.h"
#include "TapRelationLiteCallbackManager.h"
#include "TapRelationLiteModule.h"

void FTapRelationLitePlatformIOS::Initialize(const FTapTapSdkOptions&)
{
    TapRelationLiteIOSBridge::Init([](int32 Code) { FTapRelationLiteCallbackManager::Get().DispatchResult(Code); });
}

void FTapRelationLitePlatformIOS::InviteGame() { TapRelationLiteIOSBridge::InviteGame(); }
void FTapRelationLitePlatformIOS::InviteTeam(const FString& T) { TapRelationLiteIOSBridge::InviteTeam(T); }
void FTapRelationLitePlatformIOS::ShowTapUserProfile(const FString& O, const FString& U) { TapRelationLiteIOSBridge::ShowTapUserProfile(O, U); }
void FTapRelationLitePlatformIOS::RegisterRelationLiteCallback() { TapRelationLiteIOSBridge::RegisterRelationLiteCallback(); }
void FTapRelationLitePlatformIOS::UnregisterRelationLiteCallback() { TapRelationLiteIOSBridge::UnregisterRelationLiteCallback(); }
void FTapRelationLitePlatformIOS::RegisterRelationLiteInviteCallback(){TapRelationLiteIOSBridge::RegisterRelationLiteInviteCallback([](const FString&O,const FString&U,const FString&T){FTapRelationLiteCallbackManager::Get().DispatchInvite(O,U,T);});}
void FTapRelationLitePlatformIOS::UnregisterRelationLiteInviteCallback(){TapRelationLiteIOSBridge::UnregisterRelationLiteInviteCallback();}
void FTapRelationLitePlatformIOS::GetFriendsList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF) { TapRelationLiteIOSBridge::GetFriendsList(T, MoveTemp(OS), MoveTemp(OF)); }
void FTapRelationLitePlatformIOS::GetFollowingList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF) { TapRelationLiteIOSBridge::GetFollowingList(T, MoveTemp(OS), MoveTemp(OF)); }
void FTapRelationLitePlatformIOS::GetFansList(const FString& T, FRelationLiteListSuccess OS, FRelationLiteFailure OF) { TapRelationLiteIOSBridge::GetFansList(T, MoveTemp(OS), MoveTemp(OF)); }
void FTapRelationLitePlatformIOS::SyncRelationshipWithOpenId(int32 A, const FString& N, const FString& FN, const FString& OId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF)
{ TapRelationLiteIOSBridge::SyncRelationshipWithOpenId(A, N, FN, OId, MoveTemp(OS), MoveTemp(OF)); }
void FTapRelationLitePlatformIOS::SyncRelationshipWithUnionId(int32 A, const FString& N, const FString& FN, const FString& UId, FRelationLiteSyncSuccess OS, FRelationLiteSyncFailure OF)
{ TapRelationLiteIOSBridge::SyncRelationshipWithUnionId(A, N, FN, UId, MoveTemp(OS), MoveTemp(OF)); }
#endif
