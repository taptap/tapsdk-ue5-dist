// Copyright TapTap. All Rights Reserved.
#include "Android/TapShareAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapShareModule.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"

namespace
{
    // 创建 java String[]（每项需在调用后 DeleteLocalRef）
    jobjectArray MakeStringArray(JNIEnv* Env, const TArray<FString>& Items)
    {
        jclass StringClass = Env->FindClass("java/lang/String");
        jobjectArray Arr = Env->NewObjectArray(Items.Num(), StringClass, nullptr);
        for (int32 i = 0; i < Items.Num(); ++i)
        {
            jstring S = Env->NewStringUTF(TCHAR_TO_UTF8(*Items[i]));
            Env->SetObjectArrayElement(Arr, i, S);
            Env->DeleteLocalRef(S);
        }
        Env->DeleteLocalRef(StringClass);
        return Arr;
    }

    // 调用 GameActivity 上的无参 void 方法
    void CallVoidNoArg(const char* MethodName)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { return; }
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { return; }

        jclass Cls = Env->GetObjectClass(Activity);
        jmethodID Method = Env->GetMethodID(Cls, MethodName, "()V");
        if (Method && !Env->ExceptionCheck())
        {
            Env->CallVoidMethod(Activity, Method);
        }
        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        Env->DeleteLocalRef(Cls);
    }
}

namespace TapShareAndroidBridge
{
    int32 Share(const FTapShareContent& Content)
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { UE_LOG(LogTapShare, Error, TEXT("[TapShare-Android] JNI Env null")); return ETapShareResult::ERROR_NOT_SUPPORTED; }

        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapShare, Error, TEXT("[TapShare-Android] GameActivity null")); return ETapShareResult::ERROR_NOT_SUPPORTED; }

        jclass Cls = Env->GetObjectClass(Activity);
        jmethodID Method = Env->GetMethodID(
            Cls, "shareToTapTap",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)I");
        if (!Method || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapShare, Error, TEXT("[TapShare-Android] shareToTapTap not found"));
            Env->DeleteLocalRef(Cls);
            return ETapShareResult::ERROR_NOT_SUPPORTED;
        }

        jstring JTitle   = Env->NewStringUTF(TCHAR_TO_UTF8(*Content.Title));
        jstring JContents= Env->NewStringUTF(TCHAR_TO_UTF8(*Content.Contents));
        jstring JHashtag = Env->NewStringUTF(TCHAR_TO_UTF8(*Content.HashtagIds));
        jstring JGroup   = Env->NewStringUTF(TCHAR_TO_UTF8(*Content.GroupLabelId));
        jstring JFailUrl = Env->NewStringUTF(TCHAR_TO_UTF8(*Content.FailUrl));
        jobjectArray JImages = MakeStringArray(Env, Content.ImagePaths);

        jint Result = Env->CallIntMethod(Activity, Method,
            JTitle, JContents, JHashtag, JGroup, JFailUrl, JImages);

        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }

        Env->DeleteLocalRef(JTitle);
        Env->DeleteLocalRef(JContents);
        Env->DeleteLocalRef(JHashtag);
        Env->DeleteLocalRef(JGroup);
        Env->DeleteLocalRef(JFailUrl);
        Env->DeleteLocalRef(JImages);
        Env->DeleteLocalRef(Cls);

        UE_LOG(LogTapShare, Log, TEXT("[TapShare-Android] Share result=%d"), static_cast<int32>(Result));
        return static_cast<int32>(Result);
    }

    void PickImage()
    {
        CallVoidNoArg("pickShareImage");
        UE_LOG(LogTapShare, Log, TEXT("[TapShare-Android] PickImage launched"));
    }

    void ClearPickedImages()
    {
        CallVoidNoArg("clearPickedShareImages");
    }

    TArray<FString> GetPickedImagePaths()
    {
        TArray<FString> Result;

        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { return Result; }
        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { return Result; }

        jclass Cls = Env->GetObjectClass(Activity);
        jmethodID Method = Env->GetMethodID(Cls, "getPickedShareImagePaths", "()[Ljava/lang/String;");
        if (!Method || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapShare, Error, TEXT("[TapShare-Android] getPickedShareImagePaths not found"));
            Env->DeleteLocalRef(Cls);
            return Result;
        }

        jobjectArray Arr = static_cast<jobjectArray>(Env->CallObjectMethod(Activity, Method));
        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }

        if (Arr)
        {
            const jsize Count = Env->GetArrayLength(Arr);
            for (jsize i = 0; i < Count; ++i)
            {
                jstring S = static_cast<jstring>(Env->GetObjectArrayElement(Arr, i));
                if (S)
                {
                    const char* Chars = Env->GetStringUTFChars(S, nullptr);
                    if (Chars)
                    {
                        Result.Add(FString(UTF8_TO_TCHAR(Chars)));
                        Env->ReleaseStringUTFChars(S, Chars);
                    }
                    Env->DeleteLocalRef(S);
                }
            }
            Env->DeleteLocalRef(Arr);
        }

        Env->DeleteLocalRef(Cls);
        UE_LOG(LogTapShare, Log, TEXT("[TapShare-Android] GetPickedImagePaths count=%d"), Result.Num());
        return Result;
    }
}

#endif // PLATFORM_ANDROID
