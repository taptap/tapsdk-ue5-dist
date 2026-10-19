#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapRelationLiteCallback.h"
class FTapRelationLiteCallbackManager { public: static FTapRelationLiteCallbackManager& Get(); FTapWeakCallbackRegistry Relation,Invite; void Reset(){Relation.Reset();Invite.Reset();}
	void DispatchResult(int32 C); void DispatchInvite(const FString&O,const FString&U,const FString&T); };
