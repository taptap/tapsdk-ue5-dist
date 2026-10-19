// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Styling/SlateBrush.h"
#include "Brushes/SlateDynamicImageBrush.h"

/**
 * QR 码纹理生成工具
 * 将字符串编码为 QR 码并生成 Slate 可用的动态 Brush
 * 参照 Unity QRCodeUtils.EncodeQrImage (ZXing → nayuki qrcodegen)
 */
class FTapQRCodeGenerator
{
public:
	/**
	 * 将字符串编码为 QR 码 Slate Brush
	 * @param Content 要编码的内容 (通常是 URL)
	 * @param Size 输出图片尺寸 (像素, 默认 256x256)
	 * @return Slate Brush (调用方持有生命周期), nullptr 表示失败
	 */
	static TSharedPtr<FSlateDynamicImageBrush> GenerateQRBrush(
		const FString& Content, int32 Size = 256);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
