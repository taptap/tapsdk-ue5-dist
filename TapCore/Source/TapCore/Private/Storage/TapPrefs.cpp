// Copyright TapTap. All Rights Reserved.

#include "Internal/Storage/TapPrefs.h"
#include "TapCoreModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FTapPrefs::FTapPrefs()
{
	Data = MakeShared<FJsonObject>();
}

FTapPrefs::~FTapPrefs()
{
}

void FTapPrefs::Init(const FString& RelativePath)
{
	FScopeLock ScopeLock(&Lock);

	FilePath = FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath);

	// Ensure parent directory exists
	FString Dir = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}

	Load();
}

FString FTapPrefs::GetString(const FString& Key, const FString& Default) const
{
	FScopeLock ScopeLock(&Lock);
	FString Value;
	if (Data.IsValid() && Data->TryGetStringField(Key, Value))
	{
		return Value;
	}
	return Default;
}

int64 FTapPrefs::GetInt64(const FString& Key, int64 Default) const
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid() && Data->HasField(Key))
	{
		return static_cast<int64>(Data->GetNumberField(Key));
	}
	return Default;
}

void FTapPrefs::Set(const FString& Key, const FString& Value)
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid())
	{
		Data->SetStringField(Key, Value);
	}
}

void FTapPrefs::SetInt64(const FString& Key, int64 Value)
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid())
	{
		Data->SetNumberField(Key, static_cast<double>(Value));
	}
}

bool FTapPrefs::Remove(const FString& Key)
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid() && Data->HasField(Key))
	{
		Data->RemoveField(Key);
		return true;
	}
	return false;
}

void FTapPrefs::Clear()
{
	FScopeLock ScopeLock(&Lock);
	Data = MakeShared<FJsonObject>();
}

void FTapPrefs::Save()
{
	FScopeLock ScopeLock(&Lock);
	if (FilePath.IsEmpty() || !Data.IsValid())
	{
		return;
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);

	if (FFileHelper::SaveStringToFile(OutputString, *FilePath))
	{
		UE_LOG(LogTapBasic, Verbose, TEXT("[TapPrefs] Saved to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogTapBasic, Warning, TEXT("[TapPrefs] Failed to save to %s"), *FilePath);
	}
}

void FTapPrefs::Load()
{
	if (FilePath.IsEmpty())
	{
		return;
	}

	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		TSharedPtr<FJsonObject> ParsedData;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (FJsonSerializer::Deserialize(Reader, ParsedData) && ParsedData.IsValid())
		{
			Data = ParsedData;
			UE_LOG(LogTapBasic, Verbose, TEXT("[TapPrefs] Loaded from %s"), *FilePath);
		}
		else
		{
			UE_LOG(LogTapBasic, Warning, TEXT("[TapPrefs] Failed to parse %s"), *FilePath);
			Data = MakeShared<FJsonObject>();
		}
	}
	else
	{
		UE_LOG(LogTapBasic, Verbose, TEXT("[TapPrefs] No existing file at %s, starting fresh"), *FilePath);
		Data = MakeShared<FJsonObject>();
	}
}
