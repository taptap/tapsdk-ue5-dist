// Copyright TapTap. All Rights Reserved.
#include "Android/TapReviewAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapReviewModule.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"

namespace TapReviewAndroidBridge
{
    void OpenReview()
    {
        JNIEnv* Env = FAndroidApplication::GetJavaEnv();
        if (!Env) { UE_LOG(LogTapReview, Error, TEXT("[TapReview] JNI Env null")); return; }

        jobject Activity = FAndroidApplication::GetGameActivityThis();
        if (!Activity) { UE_LOG(LogTapReview, Error, TEXT("[TapReview] GameActivity null")); return; }

        jclass Cls = Env->GetObjectClass(Activity);
        jmethodID RouteMethod = Env->GetMethodID(Cls, "routeToTapReview", "(Ljava/lang/String;)V");

        if (!RouteMethod || Env->ExceptionCheck())
        {
            if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
            UE_LOG(LogTapReview, Error, TEXT("[TapReview] routeToTapReview not found"));
            Env->DeleteLocalRef(Cls);
            return;
        }

        const FString CmdJson = TEXT("{\"service\":\"BridgeReviewService\",\"method\":\"openReview\",\"args\":\"\"}");
        jstring JCmd = Env->NewStringUTF(TCHAR_TO_UTF8(*CmdJson));

        UE_LOG(LogTapReview, Log, TEXT("[TapReview] OpenReview called"));
        Env->CallVoidMethod(Activity, RouteMethod, JCmd);

        if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
        Env->DeleteLocalRef(JCmd);
        Env->DeleteLocalRef(Cls);
    }
}

#endif // PLATFORM_ANDROID
