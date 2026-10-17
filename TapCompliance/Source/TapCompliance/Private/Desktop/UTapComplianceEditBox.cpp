// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceEditBox.h"

#include "Widgets/Input/SEditableTextBox.h"

void UTapComplianceEditBox::ApplyForegroundColor(FLinearColor Color)
{
	const FSlateColor SlateColor(Color);

	WidgetStyle.ForegroundColor           = SlateColor;
	WidgetStyle.FocusedForegroundColor    = SlateColor;
	WidgetStyle.TextStyle.ColorAndOpacity = SlateColor;

	if (!MyEditableTextBlock.IsValid())
	{
		return;
	}

	// 只刷颜色, 不 SetStyle / 不 SetText, 避免 5.7 悬空 Style 和 IME/光标被重置.
	MyEditableTextBlock->SetForegroundColor(Color);
	MyEditableTextBlock->SetFocusedForegroundColor(SlateColor);
	MyEditableTextBlock->SetTextBlockStyle(&WidgetStyle.TextStyle);
}
