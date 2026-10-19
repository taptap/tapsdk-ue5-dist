// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include <atomic>

// Invalidates callbacks both before native execution and after queuing on GameThread.
class FTapRelationLiteCallbackGeneration
{
public:
    void Invalidate() { ++Generation; }
    TFunction<bool()> Capture() const
    {
        const uint64 Value = Generation.load();
        return [this, Value] { return Generation.load() == Value; };
    }
private:
    std::atomic<uint64> Generation{0};
};

namespace TapRelationLiteCallbackUtils
{
    void Dispatch(TFunction<void()> Callback, TFunction<bool()> IsCurrent = {});
    void CompleteSync(const FString& Json, TFunction<void()> OnSuccess,
        TFunction<void(const FString&)> OnFailure, TFunction<bool()> IsCurrent = {});
    bool ParseResultCode(const FString& Json, int32& Code);
}
