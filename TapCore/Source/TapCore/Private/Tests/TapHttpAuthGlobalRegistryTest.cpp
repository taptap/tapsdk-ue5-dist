// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internal/Http/TapHttp.h"
#include "Internal/Http/TapHttpAuthorizationService.h"

namespace
{
    class FMockAuthService : public ITapHttpAuthorizationService
    {
    public:
        FString Returned = TEXT("MAC mock");
        virtual FString ObtainAuthorization(const FString&, const FString&) override
        {
            return Returned;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpAuthGlobalRegistryTest,
    "TapBasic.Http.Pure.Auth.GlobalRegistry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpAuthGlobalRegistryTest::RunTest(const FString&)
{
    // 备份原服务: 该测试是 SmokeFilter, Editor 启动会自动跑.
    // 若直接 SetAuthorizationService(nullptr) 清理, 会把 TapLogin StartupModule
    // 注册的全局服务也一并清掉, 导致后续业务请求拿不到 Authorization 头 (401).
    auto Original = FTapHttp::GetAuthorizationService();

    auto Mock = MakeShared<FMockAuthService>();
    FTapHttp::SetAuthorizationService(Mock);
    auto Got = FTapHttp::GetAuthorizationService();
    TestTrue(TEXT("SetAuthorizationService 后 Get 不为空"), Got.IsValid());
    TestEqual(TEXT("Get 返回同一实例"),
        Got->ObtainAuthorization(TEXT("https://x/a"), TEXT("GET")),
        FString(TEXT("MAC mock")));

    // 验证 nullptr 语义: 临时清空再立即还原, 不留污染
    FTapHttp::SetAuthorizationService(nullptr);
    TestFalse(TEXT("Set nullptr 后 Get 为空"),
        FTapHttp::GetAuthorizationService().IsValid());

    // 还原原服务
    FTapHttp::SetAuthorizationService(Original);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpAuthGlobalRegistryReplaceTest,
    "TapBasic.Http.Pure.Auth.GlobalRegistryReplace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpAuthGlobalRegistryReplaceTest::RunTest(const FString&)
{
    // 备份原服务 (理由同上)
    auto Original = FTapHttp::GetAuthorizationService();

    auto Mock1 = MakeShared<FMockAuthService>();
    Mock1->Returned = TEXT("first");
    auto Mock2 = MakeShared<FMockAuthService>();
    Mock2->Returned = TEXT("second");

    FTapHttp::SetAuthorizationService(Mock1);
    FTapHttp::SetAuthorizationService(Mock2);

    TestEqual(TEXT("第二次 Set 覆盖第一次"),
        FTapHttp::GetAuthorizationService()->ObtainAuthorization(TEXT(""), TEXT("")),
        FString(TEXT("second")));

    // 还原原服务
    FTapHttp::SetAuthorizationService(Original);
    return true;
}

#endif
