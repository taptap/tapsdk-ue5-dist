// Copyright TapTap. All Rights Reserved.

#include "TapQRCodeGenerator.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapLoginModule.h"

THIRD_PARTY_INCLUDES_START
#include "qrcodegen.hpp"
THIRD_PARTY_INCLUDES_END

#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "TextureResource.h"

TSharedPtr<FSlateDynamicImageBrush> FTapQRCodeGenerator::GenerateQRBrush(
	const FString& Content, int32 Size)
{
	if (Content.IsEmpty() || Size <= 0)
	{
		return nullptr;
	}

	// 生成 QR 码 (Error Correction Medium, 匹配 Unity)
	FTCHARToUTF8 Utf8Content(*Content);
	qrcodegen::QrCode Qr = qrcodegen::QrCode::encodeText(
		Utf8Content.Get(), qrcodegen::QrCode::Ecc::MEDIUM);

	int32 QrSize = Qr.getSize();
	if (QrSize <= 0)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[QRCode] QR code generation failed"));
		return nullptr;
	}

	// 缩放模块到 Size x Size 像素 (最近邻, 无白边, 匹配 Unity DeleteWhite)
	TArray<uint8> PixelData;
	PixelData.SetNum(Size * Size * 4); // BGRA8

	for (int32 Y = 0; Y < Size; Y++)
	{
		for (int32 X = 0; X < Size; X++)
		{
			// 最近邻采样: 映射像素坐标到 QR 模块坐标
			int32 ModuleX = X * QrSize / Size;
			int32 ModuleY = Y * QrSize / Size;
			bool bBlack = Qr.getModule(ModuleX, ModuleY);

			int32 Idx = (Y * Size + X) * 4;
			uint8 Color = bBlack ? 0 : 255;
			PixelData[Idx + 0] = Color; // B
			PixelData[Idx + 1] = Color; // G
			PixelData[Idx + 2] = Color; // R
			PixelData[Idx + 3] = 255;   // A
		}
	}

	// 创建 UTexture2D
	UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[QRCode] Failed to create transient texture"));
		return nullptr;
	}

	// 设置 Point 过滤 (匹配 Unity FilterMode.Point)
	Texture->Filter = TF_Nearest;
	Texture->SRGB = true;

	// 写入像素数据
	void* TextureData = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, PixelData.GetData(), PixelData.Num());
	Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
	Texture->UpdateResource();

	// 创建 FSlateDynamicImageBrush
	FName BrushName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("TapQRCode"));
	TSharedPtr<FSlateDynamicImageBrush> Brush = MakeShareable(new FSlateDynamicImageBrush(
		Texture,
		FVector2D(Size, Size),
		BrushName
	));

	UE_LOG(LogTapLogin, Log, TEXT("[QRCode] Generated QR code brush (%dx%d, modules: %d)"), Size, Size, QrSize);
	return Brush;
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
