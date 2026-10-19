// Copyright TapTap. All Rights Reserved.

#include "TapComplianceCookRef.h"
#include "UObject/ConstructorHelpers.h"

// 这些 Panel 在运行时由 TapCompliancePanelManager 通过字符串路径 LoadClass 动态加载,
// 不在任何已 cook 资源的引用图中。仅靠打包脚本的 -CookDir 强制烘焙在部分环境(如 CI)下不可靠,
// 一旦 -CookDir 失效, 这些资产不会进 pak, 运行时报 LoadPackage SkipPackage / Failed to load。
// 在此 CDO 构造时用 ConstructorHelpers 建立硬引用, cooker 追踪引用图即可稳定将其打入 pak。
// 图标 / 字体 / DataTable 等被 Panel 自身引用, 会随引用图自动带入, 无需在此逐一声明。
UTapComplianceCookRef::UTapComplianceCookRef()
{
	static ConstructorHelpers::FClassFinder<UObject> RealNamePanel(
		TEXT("/TapCompliance/UI/TapComplianceRealNamePanel"));
	static ConstructorHelpers::FClassFinder<UObject> FastAuthPanel(
		TEXT("/TapCompliance/UI/TapComplianceFastAuthPanel"));
	static ConstructorHelpers::FClassFinder<UObject> AuthReviewingPanel(
		TEXT("/TapCompliance/UI/TapComplianceAuthReviewingPanel"));
	static ConstructorHelpers::FClassFinder<UObject> AuthConfirmPanel(
		TEXT("/TapCompliance/UI/TapComplianceAuthConfirmPanel"));
	static ConstructorHelpers::FClassFinder<UObject> AuthFailedPanel(
		TEXT("/TapCompliance/UI/TapComplianceAuthFailedPanel"));
	static ConstructorHelpers::FClassFinder<UObject> HealthAuthRequiredPanel(
		TEXT("/TapCompliance/UI/TapComplianceHealthAuthRequiredPanel"));
	static ConstructorHelpers::FClassFinder<UObject> HealthMinorTimeLimitPanel(
		TEXT("/TapCompliance/UI/TapComplianceHealthMinorTimeLimitPanel"));
	static ConstructorHelpers::FClassFinder<UObject> HealthMinorPaymentLimitPanel(
		TEXT("/TapCompliance/UI/TapComplianceHealthMinorPaymentLimitPanel"));
}
