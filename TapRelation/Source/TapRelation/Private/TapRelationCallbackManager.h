#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapRelationCallback.h"
class FTapRelationCallbackManager
{
public:
	static FTapRelationCallbackManager& Get();
	void RegisterRelation(UObject* O); void UnregisterRelation(UObject* O);
	void RegisterInvite(UObject* O); void UnregisterInvite(UObject* O);
	void Reset();
	void DispatchRelation(int32 Code, int32 Fans, int32 Unread);
	void DispatchGameInvite(const FString& OpenId, const FString& UnionId);
	void DispatchTeamInvite(const FString& OpenId, const FString& UnionId, const FString& TeamId);
	int32 RelationCount() { return Relation.Num(); } int32 InviteCount() { return Invite.Num(); }
private:
	FTapWeakCallbackRegistry Relation, Invite;
};
