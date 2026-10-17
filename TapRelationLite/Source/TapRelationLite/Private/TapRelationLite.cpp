// Copyright TapTap. All Rights Reserved.
#include "TapRelationLite.h"
#include "TapRelationLiteModule.h"
#include "Platform/ITapRelationLitePlatform.h"
#include "Internal/TapSdkChecker.h"
#include "TapRelationLiteCallbackManager.h"

void UTapRelationLite::InviteGame() { if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->InviteGame(); }
void UTapRelationLite::InviteTeam(const FString& T) { if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->InviteTeam(T); }
void UTapRelationLite::ShowTapUserProfile(const FString& O, const FString& U) { if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->ShowTapUserProfile(O, U); }
void UTapRelationLite::RegisterRelationLiteCallback(TScriptInterface<ITapRelationLiteCallback>C){if(!FTapSdkChecker::CheckInitialize())return;auto&M=FTapRelationLiteCallbackManager::Get();const bool bFirst=M.Relation.Num()==0;M.Relation.Register(C.GetObject(),UTapRelationLiteCallback::StaticClass());if(bFirst&&M.Relation.Num()>0)if(auto P=FTapRelationLitePlatformFactory::CreatePlatform())P->RegisterRelationLiteCallback();}
void UTapRelationLite::UnregisterRelationLiteCallback(TScriptInterface<ITapRelationLiteCallback>C){if(!FTapSdkChecker::CheckInitialize())return;auto&M=FTapRelationLiteCallbackManager::Get();M.Relation.Unregister(C.GetObject());if(M.Relation.Num()==0)if(auto P=FTapRelationLitePlatformFactory::CreatePlatform())P->UnregisterRelationLiteCallback();}
void UTapRelationLite::RegisterRelationLiteInviteCallback(TScriptInterface<ITapRelationLiteInviteCallback>C){if(!FTapSdkChecker::CheckInitialize())return;auto&M=FTapRelationLiteCallbackManager::Get();const bool bFirst=M.Invite.Num()==0;M.Invite.Register(C.GetObject(),UTapRelationLiteInviteCallback::StaticClass());if(bFirst&&M.Invite.Num()>0)if(auto P=FTapRelationLitePlatformFactory::CreatePlatform())P->RegisterRelationLiteInviteCallback();}
void UTapRelationLite::UnregisterRelationLiteInviteCallback(TScriptInterface<ITapRelationLiteInviteCallback>C){if(!FTapSdkChecker::CheckInitialize())return;auto&M=FTapRelationLiteCallbackManager::Get();M.Invite.Unregister(C.GetObject());if(M.Invite.Num()==0)if(auto P=FTapRelationLitePlatformFactory::CreatePlatform())P->UnregisterRelationLiteInviteCallback();}

void UTapRelationLite::GetFriendsList(const FString& T, TFunction<void(const FTapRelationLiteUserResult&)> OS, TFunction<void(int32, const FString&)> OF)
{ if (!FTapSdkChecker::CheckInitialize()) { if(OF)OF(-1,TEXT("TapSDK not initialized"));return;} if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->GetFriendsList(T, MoveTemp(OS), MoveTemp(OF)); else if(OF)OF(-1,TEXT("platform not available")); }

void UTapRelationLite::GetFollowingList(const FString& T, TFunction<void(const FTapRelationLiteUserResult&)> OS, TFunction<void(int32, const FString&)> OF)
{ if (!FTapSdkChecker::CheckInitialize()) { if(OF)OF(-1,TEXT("TapSDK not initialized"));return;} if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->GetFollowingList(T, MoveTemp(OS), MoveTemp(OF)); else if(OF)OF(-1,TEXT("platform not available")); }

void UTapRelationLite::GetFansList(const FString& T, TFunction<void(const FTapRelationLiteUserResult&)> OS, TFunction<void(int32, const FString&)> OF)
{ if (!FTapSdkChecker::CheckInitialize()) { if(OF)OF(-1,TEXT("TapSDK not initialized"));return;} if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->GetFansList(T, MoveTemp(OS), MoveTemp(OF)); else if(OF)OF(-1,TEXT("platform not available")); }

void UTapRelationLite::SyncRelationshipWithOpenId(int32 A, const FString& N, const FString& FN, const FString& OId, TFunction<void()> OS, TFunction<void(const FString&)> OF)
{ if (!FTapSdkChecker::CheckInitialize()) {if(OF)OF(TEXT("TapSDK not initialized"));return;} if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->SyncRelationshipWithOpenId(A, N, FN, OId, MoveTemp(OS), MoveTemp(OF)); else if(OF)OF(TEXT("platform not available")); }

void UTapRelationLite::SyncRelationshipWithUnionId(int32 A, const FString& N, const FString& FN, const FString& UId, TFunction<void()> OS, TFunction<void(const FString&)> OF)
{ if (!FTapSdkChecker::CheckInitialize()) {if(OF)OF(TEXT("TapSDK not initialized"));return;} if (auto P = FTapRelationLitePlatformFactory::CreatePlatform()) P->SyncRelationshipWithUnionId(A, N, FN, UId, MoveTemp(OS), MoveTemp(OF)); else if(OF)OF(TEXT("platform not available")); }
