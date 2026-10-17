// Copyright TapTap. All Rights Reserved.
#include "Android/TapMomentPlatformAndroid.h"

#if PLATFORM_ANDROID

#include "Android/TapMomentAndroidBridge.h"
#include "TapMomentCallbackManager.h"
#include "TapMomentModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

void FTapMomentPlatformAndroid::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    TapMomentAndroidBridge::Init([](int32 Code, const FString& Msg)
    {
        FTapMomentCallbackManager::Get().Dispatch(Code, Msg);
    });
}

void FTapMomentPlatformAndroid::Open()               { TapMomentAndroidBridge::Open(); }
void FTapMomentPlatformAndroid::OpenScene(const FString& SceneId) { TapMomentAndroidBridge::OpenScene(SceneId); }
void FTapMomentPlatformAndroid::Close()              { TapMomentAndroidBridge::Close(); }
void FTapMomentPlatformAndroid::FetchNotification()  { TapMomentAndroidBridge::FetchNotification(); }
void FTapMomentPlatformAndroid::SetCallback()        { TapMomentAndroidBridge::SetCallback(); }

void FTapMomentPlatformAndroid::CloseWithTitle(const FString& Title, const FString& Content)
{
    TapMomentAndroidBridge::CloseWithTitle(Title, Content);
}

void FTapMomentPlatformAndroid::Publish(const FTapMomentPostData& PostData)
{
    TSharedRef<FJsonObject> Meta = MakeShared<FJsonObject>();
    Meta->SetStringField(TEXT("content"), PostData.Content);

    TArray<TSharedPtr<FJsonValue>> ImgArr, VidArr;
    for (const FString& P : PostData.ImagePaths) ImgArr.Add(MakeShared<FJsonValueString>(P));
    for (const FString& P : PostData.VideoPaths) VidArr.Add(MakeShared<FJsonValueString>(P));
    Meta->SetArrayField(TEXT("imagePaths"), ImgArr);
    Meta->SetArrayField(TEXT("videoPaths"), VidArr);

    FString MetaJson; auto W = TJsonWriterFactory<>::Create(&MetaJson);
    FJsonSerializer::Serialize(Meta, W);
    TapMomentAndroidBridge::Publish(MetaJson);
}

#endif // PLATFORM_ANDROID
