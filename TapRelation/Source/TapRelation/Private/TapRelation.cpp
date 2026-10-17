// Copyright TapTap. All Rights Reserved.
#include "TapRelation.h"
#include "TapRelationModule.h"
#include "Platform/ITapRelationPlatform.h"
#include "Internal/TapSdkChecker.h"
#include "TapRelationCallbackManager.h"

void UTapRelation::Prepare()
{ if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->Prepare(); }

void UTapRelation::StartMessenger()
{ if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->StartMessenger(); }

void UTapRelation::InviteGame()
{ if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->InviteGame(); }

void UTapRelation::InviteTeam(const FString& TeamId)
{ if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->InviteTeam(TeamId); }

void UTapRelation::ShowTapUserProfile(const FString& OpenId, const FString& UnionId)
{ if (!FTapSdkChecker::CheckInitialize()) return; if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->ShowTapUserProfile(OpenId, UnionId); }

void UTapRelation::GetNewFansCount(TFunction<void(int32)> Callback)
{ if (!FTapSdkChecker::CheckInitialize()) { if (Callback) Callback(-1); return; } if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->GetNewFansCount(MoveTemp(Callback)); else if (Callback) Callback(-1); }

void UTapRelation::GetUnreadMessageCount(TFunction<void(int32)> Callback)
{ if (!FTapSdkChecker::CheckInitialize()) { if (Callback) Callback(-1); return; } if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->GetUnreadMessageCount(MoveTemp(Callback)); else if (Callback) Callback(-1); }

void UTapRelation::RegisterRelationCallback(TScriptInterface<ITapRelationCallback> C)
{ if (!FTapSdkChecker::CheckInitialize()) return; auto& M=FTapRelationCallbackManager::Get(); const bool bFirst=M.RelationCount()==0; M.RegisterRelation(C.GetObject()); if (bFirst && M.RelationCount()>0) if (auto P=FTapRelationPlatformFactory::CreatePlatform()) P->RegisterRelationCallback(); }
void UTapRelation::UnregisterRelationCallback(TScriptInterface<ITapRelationCallback> C)
{ if (!FTapSdkChecker::CheckInitialize()) return; auto& M=FTapRelationCallbackManager::Get(); M.UnregisterRelation(C.GetObject()); if (M.RelationCount()==0) if(auto P=FTapRelationPlatformFactory::CreatePlatform()) P->UnregisterRelationCallback(); }
void UTapRelation::RegisterRelationInviteCallback(TScriptInterface<ITapRelationInviteCallback> C)
{ if (!FTapSdkChecker::CheckInitialize()) return; auto& M=FTapRelationCallbackManager::Get(); const bool bFirst=M.InviteCount()==0; M.RegisterInvite(C.GetObject()); if(bFirst && M.InviteCount()>0) if(auto P=FTapRelationPlatformFactory::CreatePlatform()) P->RegisterRelationInviteCallback(); }
void UTapRelation::UnregisterRelationInviteCallback(TScriptInterface<ITapRelationInviteCallback> C)
{ if (!FTapSdkChecker::CheckInitialize()) return; auto& M=FTapRelationCallbackManager::Get(); M.UnregisterInvite(C.GetObject()); if(M.InviteCount()==0) if(auto P=FTapRelationPlatformFactory::CreatePlatform()) P->UnregisterRelationInviteCallback(); }

void UTapRelation::Destroy()
{ if (!FTapSdkChecker::CheckInitialize()) return; FTapRelationCallbackManager::Get().Reset(); if (auto P = FTapRelationPlatformFactory::CreatePlatform()) P->Destroy(); }
