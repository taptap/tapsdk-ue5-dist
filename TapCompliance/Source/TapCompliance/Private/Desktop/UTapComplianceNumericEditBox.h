// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UTapComplianceEditBox.h"
#include "UTapComplianceNumericEditBox.generated.h"

/**
 * TapCompliance 身份证输入框 Widget
 *
 * 继承 UTapComplianceEditBox：在 Designer 控件面板（Palette > User Created）
 * 里像普通 EditableTextBox 一样拖入即可，无需 WBP wrapper / BindWidget。
 * 基类提供安全的前景色同步, 本类只做输入过滤.
 *
 * 用法（蓝图）：
 *   - 在任意 WBP 中拖一个 TapComplianceNumericEditBox 控件
 *   - Details 面板设 MaxLength（身份证常用 18）
 *   - 监听 OnTextChanged / OnTextCommitted（继承自 UEditableTextBox）
 *
 * 用法（C++）：
 *   UPROPERTY(meta = (BindWidget))
 *   UTapComplianceNumericEditBox* IDNumberInput;
 *
 * 输入过滤规则：
 *   - 允许 0-9 和 A-Z / a-z（满足身份证末位可为 X/x）
 *   - 其他字符（空格、标点、汉字、Emoji 等）一律丢弃
 *   - MaxLength > 0 时限制总字符数
 *   - 是否大小写敏感、是否符合身份证 18 位规则等业务级校验请放到提交时做
 *
 * 平台：仅 Desktop (Win/Mac/Linux) 有过滤逻辑；移动端走空 override（含义即不过滤）。
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceNumericEditBox : public UTapComplianceEditBox
{
	GENERATED_BODY()

public:
	/** 最大字符数，0 表示不限制 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCompliance|NumericEditBox|Input", meta = (ClampMin = "0"))
	int32 MaxLength = 0;

	/** 当前文本解析为 int32；空或非法返回 0（含字母时也会按前缀解析） */
	UFUNCTION(BlueprintPure, Category = "TapCompliance|NumericEditBox")
	int32 GetValueAsInt() const;

	/** 非空即视为有效；具体身份证校验请放业务侧 */
	UFUNCTION(BlueprintPure, Category = "TapCompliance|NumericEditBox")
	bool IsValidNumber() const;

protected:
	/** 拦截输入：先按规则过滤再 broadcast 给上层监听者 */
	virtual void HandleOnTextChanged(const FText& InText) override;

private:
	/** 防止 SetText 触发的回调递归过滤 */
	bool bIsFiltering = false;

	/** 按规则过滤输入字符串：保留 [0-9A-Za-z]，按 MaxLength 截断 */
	FString FilterInput(const FString& Raw) const;
};
