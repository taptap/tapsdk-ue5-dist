#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapRelationLiteCallback.generated.h"
UINTERFACE(BlueprintType) class TAPRELATIONLITE_API UTapRelationLiteCallback : public UInterface { GENERATED_BODY() };
class TAPRELATIONLITE_API ITapRelationLiteCallback { GENERATED_BODY() public: UFUNCTION(BlueprintNativeEvent,Category="TapRelationLite") void OnResult(int32 Code); };
UINTERFACE(BlueprintType) class TAPRELATIONLITE_API UTapRelationLiteInviteCallback : public UInterface { GENERATED_BODY() };
class TAPRELATIONLITE_API ITapRelationLiteInviteCallback { GENERATED_BODY() public:
	UFUNCTION(BlueprintNativeEvent,Category="TapRelationLite") void OnGameInviteReceived(const FString& OpenId,const FString& UnionId);
	UFUNCTION(BlueprintNativeEvent,Category="TapRelationLite") void OnTeamInviteReceived(const FString& OpenId,const FString& UnionId,const FString& TeamId);
};
