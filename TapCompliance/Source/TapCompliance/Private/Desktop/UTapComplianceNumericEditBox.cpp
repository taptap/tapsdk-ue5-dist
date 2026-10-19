// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceNumericEditBox.h"

#include "TapComplianceModule.h"           // LogTapCompliance
#include "Widgets/Input/SEditableTextBox.h" // MyEditableTextBlock->SetText()

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

int32 UTapComplianceNumericEditBox::GetValueAsInt() const
{
	const FString S = GetText().ToString();
	return S.IsEmpty() ? 0 : FCString::Atoi(*S);
}

bool UTapComplianceNumericEditBox::IsValidNumber() const
{
	return !GetText().IsEmpty();
}

FString UTapComplianceNumericEditBox::FilterInput(const FString& Raw) const
{
	FString Filtered;
	Filtered.Reserve(Raw.Len());

	for (int32 i = 0; i < Raw.Len(); ++i)
	{
		const TCHAR C = Raw[i];

		const bool bIsDigit = (C >= TEXT('0') && C <= TEXT('9'));
		const bool bIsAlpha = (C >= TEXT('A') && C <= TEXT('Z')) || (C >= TEXT('a') && C <= TEXT('z'));
		if (!bIsDigit && !bIsAlpha)
		{
			continue;
		}

		if (MaxLength > 0 && Filtered.Len() >= MaxLength)
		{
			break;
		}
		Filtered.AppendChar(C);
	}

	return Filtered;
}

void UTapComplianceNumericEditBox::HandleOnTextChanged(const FText& InText)
{
	if (bIsFiltering)
	{
		UE_LOG(LogTapCompliance, Verbose,
			TEXT("[IDInput] OnTextChanged(reentry) text='%s'"),
			TEXT("[REDACTED]"));
		// 由 SetText 触发的回调，已是过滤后文本，直接 broadcast
		Super::HandleOnTextChanged(InText);
		return;
	}

	const FString Raw = InText.ToString();
	const FString Filtered = FilterInput(Raw);

	if (Filtered != Raw)
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[IDInput] FILTERED raw='%s' (%d chars) -> filtered='%s' (%d chars) [MaxLength=%d]"),
			TEXT("[REDACTED]"), Raw.Len(), TEXT("[REDACTED]"), Filtered.Len(), MaxLength);

		// UEditableTextBox::SetText 内部用 SetTextInternal 与 Text UPROPERTY 比较:
		// 若 Filtered == 上一次合法值(Text), 判断"无变化"而跳过 Slate 推送, 视觉不更新.
		// 直接调 MyEditableTextBlock->SetText 绕过此判断, 强制 Slate 回写.
		bIsFiltering = true;
		if (MyEditableTextBlock.IsValid())
		{
			MyEditableTextBlock->SetText(FText::FromString(Filtered));
		}
		bIsFiltering = false;

		Super::HandleOnTextChanged(FText::FromString(Filtered));
		return;
	}

	UE_LOG(LogTapCompliance, Verbose,
		TEXT("[IDInput] PASS raw='%s' (%d chars) [MaxLength=%d]"),
		TEXT("[REDACTED]"), Raw.Len(), MaxLength);

	Super::HandleOnTextChanged(InText);
}

#else // Non-desktop platforms: empty stubs

int32 UTapComplianceNumericEditBox::GetValueAsInt() const { return 0; }
bool UTapComplianceNumericEditBox::IsValidNumber() const { return false; }
FString UTapComplianceNumericEditBox::FilterInput(const FString&) const { return FString(); }
void UTapComplianceNumericEditBox::HandleOnTextChanged(const FText& InText)
{
	Super::HandleOnTextChanged(InText);
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
