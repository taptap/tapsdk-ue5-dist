// Copyright TapTap. All Rights Reserved.

#include "TapCoreCookRef.h"
#include "UObject/ConstructorHelpers.h"

// 启动错误弹窗在运行时由 TapLauncherErrorDialogWidget 通过字符串路径 LoadClass 动态加载,
// 不在任何已 cook 资源的引用图中。仅靠打包脚本的 -CookDir 强制烘焙在部分环境(如 CI)下不可靠。
// 在此 CDO 构造时用 ConstructorHelpers 建立硬引用, cooker 追踪引用图即可稳定将其打入 pak。
// WBP_TapButton / T_IconWarning_3x 被弹窗自身引用, 会随引用图自动带入, 无需在此逐一声明。
UTapCoreCookRef::UTapCoreCookRef()
{
	static ConstructorHelpers::FClassFinder<UObject> ErrorDialog(
		TEXT("/TapCore/WBP_TapCoreLanuchErrorDialog"));
}
