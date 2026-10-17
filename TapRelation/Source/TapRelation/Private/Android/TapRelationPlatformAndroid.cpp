// Copyright TapTap. All Rights Reserved.
#include "Android/TapRelationPlatformAndroid.h"
#if PLATFORM_ANDROID
#include "Android/TapRelationAndroidBridge.h"
#include "TapRelationCallbackManager.h"
#include "TapRelationModule.h"

void FTapRelationPlatformAndroid::Initialize(const FTapTapSdkOptions&) { TapRelationAndroidBridge::Init(); }
void FTapRelationPlatformAndroid::Prepare()             { TapRelationAndroidBridge::Prepare(); }
void FTapRelationPlatformAndroid::StartMessenger()      { TapRelationAndroidBridge::StartMessenger(); }
void FTapRelationPlatformAndroid::InviteGame()          { TapRelationAndroidBridge::InviteGame(); }
void FTapRelationPlatformAndroid::Destroy()             { TapRelationAndroidBridge::Destroy(); }
void FTapRelationPlatformAndroid::InviteTeam(const FString& T) { TapRelationAndroidBridge::InviteTeam(T); }
void FTapRelationPlatformAndroid::ShowTapUserProfile(const FString& O, const FString& U) { TapRelationAndroidBridge::ShowTapUserProfile(O, U); }
void FTapRelationPlatformAndroid::GetNewFansCount(TFunction<void(int32)> C) { TapRelationAndroidBridge::GetNewFansCount(MoveTemp(C)); }
void FTapRelationPlatformAndroid::GetUnreadMessageCount(TFunction<void(int32)> C) { TapRelationAndroidBridge::GetUnreadMessageCount(MoveTemp(C)); }

void FTapRelationPlatformAndroid::RegisterRelationCallback()
{
    TapRelationAndroidBridge::RegisterRelationCallback([](int32 Code, int32 Fans, int32 Unread)
    {
        FTapRelationCallbackManager::Get().DispatchRelation(Code, Fans, Unread);
    });
}
void FTapRelationPlatformAndroid::UnregisterRelationCallback(){ TapRelationAndroidBridge::UnregisterRelationCallback(); }
void FTapRelationPlatformAndroid::RegisterRelationInviteCallback(){ TapRelationAndroidBridge::RegisterRelationInviteCallback([](const FString& O,const FString& U,const FString& T){ if(T.IsEmpty()) FTapRelationCallbackManager::Get().DispatchGameInvite(O,U); else FTapRelationCallbackManager::Get().DispatchTeamInvite(O,U,T); }); }
void FTapRelationPlatformAndroid::UnregisterRelationInviteCallback(){ TapRelationAndroidBridge::UnregisterRelationInviteCallback(); }
#endif
