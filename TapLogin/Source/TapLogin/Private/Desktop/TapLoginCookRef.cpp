// Copyright TapTap. All Rights Reserved.

#include "TapLoginCookRef.h"
#include "UObject/ConstructorHelpers.h"

UTapLoginCookRef::UTapLoginCookRef()
{
	// ConstructorHelpers 在 CDO 构造时创建硬引用
	// Cooker 追踪此引用并将 Blueprint 加入 pak 打包清单
	static ConstructorHelpers::FClassFinder<UObject> PanelBP(
		TEXT("/TapLogin/UI/WBP_TapLoginPanel"));
}
