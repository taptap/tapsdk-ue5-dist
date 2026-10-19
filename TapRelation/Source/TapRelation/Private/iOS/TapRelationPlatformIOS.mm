// Copyright TapTap. All Rights Reserved.
#include "iOS/TapRelationPlatformIOS.h"
#if PLATFORM_IOS
#include "iOS/TapRelationIOSBridge.h"
#include "TapRelationCallbackManager.h"
#include "TapRelationModule.h"

void FTapRelationPlatformIOS::Initialize(const FTapTapSdkOptions&)
{
    TapRelationIOSBridge::Init([](int32 Code, int32 Fans, int32 Unread)
    {
        FTapRelationCallbackManager::Get().DispatchRelation(Code, Fans, Unread);
    });
}

void FTapRelationPlatformIOS::Prepare()             { TapRelationIOSBridge::Prepare(); }
void FTapRelationPlatformIOS::StartMessenger()      { TapRelationIOSBridge::StartMessenger(); }
void FTapRelationPlatformIOS::InviteGame()          { TapRelationIOSBridge::InviteGame(); }
void FTapRelationPlatformIOS::Destroy()             { TapRelationIOSBridge::Destroy(); }
void FTapRelationPlatformIOS::InviteTeam(const FString& T) { TapRelationIOSBridge::InviteTeam(T); }
void FTapRelationPlatformIOS::ShowTapUserProfile(const FString& O, const FString& U) { TapRelationIOSBridge::ShowTapUserProfile(O, U); }
void FTapRelationPlatformIOS::GetNewFansCount(TFunction<void(int32)> C) { TapRelationIOSBridge::GetNewFansCount(MoveTemp(C)); }
void FTapRelationPlatformIOS::GetUnreadMessageCount(TFunction<void(int32)> C) { TapRelationIOSBridge::GetUnreadMessageCount(MoveTemp(C)); }
void FTapRelationPlatformIOS::RegisterRelationCallback() { TapRelationIOSBridge::RegisterRelationCallback(); }
void FTapRelationPlatformIOS::UnregisterRelationCallback() { TapRelationIOSBridge::UnregisterRelationCallback(); }
void FTapRelationPlatformIOS::RegisterRelationInviteCallback() { TapRelationIOSBridge::RegisterRelationInviteCallback([](const FString& O,const FString& U,const FString& T){ if(T.IsEmpty()) FTapRelationCallbackManager::Get().DispatchGameInvite(O,U); else FTapRelationCallbackManager::Get().DispatchTeamInvite(O,U,T); }); }
void FTapRelationPlatformIOS::UnregisterRelationInviteCallback() { TapRelationIOSBridge::UnregisterRelationInviteCallback(); }
#endif
