#include "TapRelationCallbackManager.h"
FTapRelationCallbackManager& FTapRelationCallbackManager::Get(){ static FTapRelationCallbackManager I; return I; }
void FTapRelationCallbackManager::RegisterRelation(UObject* O){ Relation.Register(O, UTapRelationCallback::StaticClass()); }
void FTapRelationCallbackManager::UnregisterRelation(UObject* O){ Relation.Unregister(O); }
void FTapRelationCallbackManager::RegisterInvite(UObject* O){ Invite.Register(O, UTapRelationInviteCallback::StaticClass()); }
void FTapRelationCallbackManager::UnregisterInvite(UObject* O){ Invite.Unregister(O); }
void FTapRelationCallbackManager::Reset(){ Relation.Reset(); Invite.Reset(); }
void FTapRelationCallbackManager::DispatchRelation(int32 C,int32 F,int32 U)
{
	Relation.Dispatch([C](UObject* O){ ITapRelationCallback::Execute_OnMessengerCodeResult(O,C); });
	Relation.Dispatch([C,F](UObject* O){ ITapRelationCallback::Execute_OnNewFansCountChanged(O,C,F); });
	Relation.Dispatch([C,U](UObject* O){ ITapRelationCallback::Execute_OnUnreadMessageCountChanged(O,C,U); });
}
void FTapRelationCallbackManager::DispatchGameInvite(const FString& O,const FString& U){ Invite.Dispatch([O,U](UObject* X){ ITapRelationInviteCallback::Execute_OnGameInviteReceived(X,O,U); }); }
void FTapRelationCallbackManager::DispatchTeamInvite(const FString& O,const FString& U,const FString& T){ Invite.Dispatch([O,U,T](UObject* X){ ITapRelationInviteCallback::Execute_OnTeamInviteReceived(X,O,U,T); }); }
