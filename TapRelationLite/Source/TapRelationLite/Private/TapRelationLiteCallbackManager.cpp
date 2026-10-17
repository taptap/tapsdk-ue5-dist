#include "TapRelationLiteCallbackManager.h"
FTapRelationLiteCallbackManager& FTapRelationLiteCallbackManager::Get(){static FTapRelationLiteCallbackManager I;return I;}
void FTapRelationLiteCallbackManager::DispatchResult(int32 C){Relation.Dispatch([C](UObject*O){ITapRelationLiteCallback::Execute_OnResult(O,C);});}
void FTapRelationLiteCallbackManager::DispatchInvite(const FString&O,const FString&U,const FString&T){if(T.IsEmpty())Invite.Dispatch([O,U](UObject*X){ITapRelationLiteInviteCallback::Execute_OnGameInviteReceived(X,O,U);});else Invite.Dispatch([O,U,T](UObject*X){ITapRelationLiteInviteCallback::Execute_OnTeamInviteReceived(X,O,U,T);});}
