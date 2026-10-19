// Copyright TapTap. All Rights Reserved.

#include "TapCloudSave.h"
#include "TapCloudSaveModule.h"
#include "Platform/ITapCloudSavePlatform.h"
#include "Internal/TapSdkChecker.h"
#include "Misc/Paths.h"
#include "TapCloudSaveCallbackManager.h"

namespace
{
	bool FireErrorIfFail(bool bCondition, int32 Code, const FString& Msg, const FTapOnArchiveError& OnError)
	{
		if (bCondition) { return true; }
		UE_LOG(LogTapCloudSave, Warning, TEXT("[TapCloudSave] Input check failed: %s"), *Msg);
		OnError.ExecuteIfBound(Code, Msg);
		return false;
	}

	bool FireBinaryErrorIfFail(bool bCondition, int32 Code, const FString& Msg, const FTapOnArchiveError& OnError)
	{
		// Same shape, kept separate for readability with binary entrypoints.
		return FireErrorIfFail(bCondition, Code, Msg, OnError);
	}

	TSharedPtr<ITapCloudSavePlatform> Platform()
	{
		auto P = FTapCloudSavePlatformFactory::CreatePlatform();
		if (!P.IsValid())
		{
			UE_LOG(LogTapCloudSave, Error, TEXT("[TapCloudSave] platform not available"));
		}
		return P;
	}

}

FString UTapCloudSave::GetVersion() { return TapCloudSave_VERSION; }

void UTapCloudSave::RegisterCloudSaveCallback(TScriptInterface<ITapCloudSaveCallback> Callback)
{
	if(!FTapSdkChecker::CheckInitialize())return;auto&M=FTapCloudSaveCallbackManager::Get();const bool First=M.Num()==0;M.Register(Callback.GetObject());if(First)if(auto P=Platform())P->RegisterStateCallback(M.NativeDelegate());
}

void UTapCloudSave::UnregisterCloudSaveCallback(TScriptInterface<ITapCloudSaveCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) return;
	auto&M=FTapCloudSaveCallbackManager::Get();M.Unregister(Callback.GetObject());if(M.Num()==0)if(auto P=Platform())P->UnregisterStateCallback(M.NativeDelegate());
}

void UTapCloudSave::CreateArchive(const FTapArchiveMetadata& Metadata, const FString& ArchiveFilePath, const FString& CoverFilePath, const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError)
{
	if (!FireErrorIfFail(FTapSdkChecker::CheckInitialize(), TapCloudSaveResultCode::INIT_FAIL, TEXT("TapSDK not initialized"), OnError)) return;
	UE_LOG(LogTapCloudSave, Log,
		TEXT("[CloudSave] CreateArchive input: archive=%s exists=%d, cover=%s coverExists=%d"),
		*ArchiveFilePath,
		FPaths::FileExists(ArchiveFilePath) ? 1 : 0,
		*CoverFilePath,
		CoverFilePath.IsEmpty() ? 1 : (FPaths::FileExists(CoverFilePath) ? 1 : 0));

	if (!FireErrorIfFail(!Metadata.Name.IsEmpty(), TapCloudSaveResultCode::INVALID_PARAM, TEXT("Archive name required"), OnError)) return;
	if (!FireErrorIfFail(FPaths::FileExists(ArchiveFilePath), TapCloudSaveResultCode::INVALID_PARAM, FString::Printf(TEXT("Archive file not found: %s"), *ArchiveFilePath), OnError)) return;
	if (!CoverFilePath.IsEmpty() && !FireErrorIfFail(FPaths::FileExists(CoverFilePath), TapCloudSaveResultCode::INVALID_PARAM, FString::Printf(TEXT("Cover file not found: %s"), *CoverFilePath), OnError)) return;

	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapCloudSaveResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	// 传原始 UE 虚拟路径（../../../<Project>/Saved/...）。不要在这里做
	// FPaths::ConvertRelativePathToFull：它只拼 FPlatformProcess::BaseDir() 再折叠 ".."，
	// 而 iOS/Android 的 BaseDir() 返回空串，转换是 no-op；更糟的是一旦它真产出了
	// 绝对路径，就不再以 "../" 开头，平台层的沙盒映射会直接原样返回而失效。
	// 映射到原生可用绝对路径由各平台层负责（见 Android/iOS 平台实现）。
	P->CreateArchive(Metadata, ArchiveFilePath, CoverFilePath, OnSuccess, OnError);
}

void UTapCloudSave::UpdateArchive(const FString& Uuid, const FTapArchiveMetadata& Metadata, const FString& ArchiveFilePath, const FString& CoverFilePath, const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError)
{
	if (!FireErrorIfFail(FTapSdkChecker::CheckInitialize(), TapCloudSaveResultCode::INIT_FAIL, TEXT("TapSDK not initialized"), OnError)) return;
	UE_LOG(LogTapCloudSave, Log,
		TEXT("[CloudSave] UpdateArchive input: uuid=%s archive=%s exists=%d, cover=%s coverExists=%d"),
		*Uuid,
		*ArchiveFilePath,
		FPaths::FileExists(ArchiveFilePath) ? 1 : 0,
		*CoverFilePath,
		CoverFilePath.IsEmpty() ? 1 : (FPaths::FileExists(CoverFilePath) ? 1 : 0));

	if (!FireErrorIfFail(!Metadata.Name.IsEmpty(), TapCloudSaveResultCode::INVALID_PARAM, TEXT("Archive name required"), OnError)) return;
	if (!FireErrorIfFail(FPaths::FileExists(ArchiveFilePath), TapCloudSaveResultCode::INVALID_PARAM, FString::Printf(TEXT("Archive file not found: %s"), *ArchiveFilePath), OnError)) return;
	if (!CoverFilePath.IsEmpty() && !FireErrorIfFail(FPaths::FileExists(CoverFilePath), TapCloudSaveResultCode::INVALID_PARAM, FString::Printf(TEXT("Cover file not found: %s"), *CoverFilePath), OnError)) return;

	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapCloudSaveResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	// 同 CreateArchive：传原始 UE 虚拟路径，沙盒映射交给平台层。
	P->UpdateArchive(Uuid, Metadata, ArchiveFilePath, CoverFilePath, OnSuccess, OnError);
}

void UTapCloudSave::DeleteArchive(const FString& Uuid, const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError)
{
	if (!FireErrorIfFail(FTapSdkChecker::CheckInitialize(), TapCloudSaveResultCode::INIT_FAIL, TEXT("TapSDK not initialized"), OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapCloudSaveResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->DeleteArchive(Uuid, OnSuccess, OnError);
}

void UTapCloudSave::GetArchiveList(const FTapOnArchiveListResult& OnSuccess, const FTapOnArchiveError& OnError)
{
	if (!FireErrorIfFail(FTapSdkChecker::CheckInitialize(), TapCloudSaveResultCode::INIT_FAIL, TEXT("TapSDK not initialized"), OnError)) return;
	if (!FireErrorIfFail(Platform().IsValid(), TapCloudSaveResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	Platform()->GetArchiveList(OnSuccess, OnError);
}

void UTapCloudSave::GetArchiveData(const FString& Uuid, const FString& FileId, const FTapOnArchiveBinaryResult& OnSuccess, const FTapOnArchiveError& OnError)
{
	if (!FireErrorIfFail(FTapSdkChecker::CheckInitialize(), TapCloudSaveResultCode::INIT_FAIL, TEXT("TapSDK not initialized"), OnError)) return;
	if (!FireErrorIfFail(!FileId.IsEmpty(), -1, TEXT("Archive file_id required"), OnError)) return;
	if (!FireErrorIfFail(Platform().IsValid(), TapCloudSaveResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	Platform()->GetArchiveData(Uuid, FileId, OnSuccess, OnError);
}

void UTapCloudSave::GetArchiveCover(const FString& Uuid, const FString& FileId, const FTapOnArchiveBinaryResult& OnSuccess, const FTapOnArchiveError& OnError)
{
	if (!FireErrorIfFail(FTapSdkChecker::CheckInitialize(), TapCloudSaveResultCode::INIT_FAIL, TEXT("TapSDK not initialized"), OnError)) return;
	if (!FireErrorIfFail(!Uuid.IsEmpty(), TapCloudSaveResultCode::INVALID_PARAM, TEXT("Archive uuid required"), OnError)) return;
	if (!FireErrorIfFail(!FileId.IsEmpty(), TapCloudSaveResultCode::INVALID_PARAM, TEXT("Archive file_id required"), OnError)) return;
	if (!FireErrorIfFail(Platform().IsValid(), TapCloudSaveResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	Platform()->GetArchiveCover(Uuid, FileId, OnSuccess, OnError);
}
