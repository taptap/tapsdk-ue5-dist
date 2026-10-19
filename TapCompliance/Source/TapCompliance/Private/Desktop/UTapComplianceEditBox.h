// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "UTapComplianceEditBox.generated.h"

/**
 * TapCompliance 通用输入框
 *
 * 直接继承 UEditableTextBox, 不过滤输入, 供姓名等自由文本使用.
 * 身份证过滤请用 UTapComplianceNumericEditBox (本类子类).
 *
 * 提供 ApplyForegroundColor: 原地改 UObject 持有的 WidgetStyle, 再只把颜色
 * 同步到 SEditableTextBox. 不要走 UEditableTextBox::SetWidgetStyle —
 * UE 5.7 会把临时参数地址交给 SetStyle, 下一帧 DetermineFont() 会崩;
 * 也不要在 OnTextChanged 里 SynchronizeProperties, 那会 SetText 打断 IME.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceEditBox : public UEditableTextBox
{
	GENERATED_BODY()

public:
	/** 同时更新无焦点 / 焦点 / 内层文字颜色, 并同步到已创建的 Slate 控件. */
	void ApplyForegroundColor(FLinearColor Color);
};
