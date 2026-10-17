// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpSign.h"
#include "Internal/Http/TapHttpCommonParams.h"

namespace
{
    class FTapHttpDefaultSign : public ITapHttpSign
    {
    public:
        virtual TMap<FString, FString> GetFixedHeaders(
            const FString& /*Url*/, const FString& Method,
            const FString& ModuleName, const FString& ModuleVersion,
            bool bEnableAuthorization) override
        {
            // 对齐 Unity TapHttpSign.cs:55 Default.GetFixedHeaders:
            // 只在 GET/POST 返回; 其它方法返回空 map (对齐 Unity null).
            if (!Method.Equals(TEXT("GET")) && !Method.Equals(TEXT("POST")))
            {
                return TMap<FString, FString>();
            }
            return FTapHttpCommonParams::GetCommonHeaders(
                ModuleName, ModuleVersion, bEnableAuthorization);
        }

        virtual TMap<FString, FString> GetFixedQueryParams() override
        {
            TMap<FString, FString> Q;
            const FString& Cid = FTapHttpCommonParams::GetClientID();
            if (!Cid.IsEmpty())
            {
                Q.Add(TEXT("client_id"), Cid);
            }
            return Q;
        }

        virtual void Sign(FTapHttpSignData& Data) override
        {
            FTapHttpCommonParams::AddSignature(
                Data.Method, Data.Url, Data.Headers, Data.Body);
        }
    };

    class FTapHttpNoneSign : public ITapHttpSign
    {
    public:
        virtual TMap<FString, FString> GetFixedHeaders(
            const FString&, const FString&, const FString&, const FString&, bool) override
        { return TMap<FString, FString>(); }

        virtual TMap<FString, FString> GetFixedQueryParams() override
        { return TMap<FString, FString>(); }

        virtual void Sign(FTapHttpSignData&) override {}
    };
}

TSharedRef<ITapHttpSign> FTapHttpSign::CreateDefaultSign()
{
    return MakeShared<FTapHttpDefaultSign>();
}

TSharedRef<ITapHttpSign> FTapHttpSign::CreateNoneSign()
{
    return MakeShared<FTapHttpNoneSign>();
}
