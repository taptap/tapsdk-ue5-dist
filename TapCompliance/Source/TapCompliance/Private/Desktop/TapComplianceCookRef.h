// Copyright TapTap. All Rights Reserved.
// Helper: ConstructorHelpers 在 CDO 中建立 Cook 依赖,
// 确保插件 Content 中的 Blueprint 资产被打进 pak 文件

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TapComplianceCookRef.generated.h"

UCLASS()
class UTapComplianceCookRef : public UObject
{
	GENERATED_BODY()
public:
	UTapComplianceCookRef();
};
