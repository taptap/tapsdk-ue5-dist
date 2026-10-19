// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapAchievementTypes.h"

class FJsonObject;

/**
 * 解析移动端 registerCallback 持久回调的 JSON 回包(Android / iOS 同构)。
 *
 * 成功: {"state":"success","code":<int>,"result":"<内嵌JSON字符串>"}
 *        内嵌 result: {achievementId, achievementName, achievementType:"NORMAL"/"PLATINUM", currentSteps}
 * 失败: {"state":"failure","achievementId":..,"errorCode":<int>,"errorMsg":".."}
 */
namespace TapAchievementResultParser
{
    struct FParsed
    {
        bool                  bParsed    = false;  // JSON 是否成功解析
        bool                  bIsSuccess = false;  // state == "success"
        int32                 Code       = 0;      // 成功码(70001/70002)
        FTapAchievementResult Result;              // 成功时填充
        FString               AchievementId;       // 失败时填充
        int32                 ErrorCode  = 0;      // 失败码
        FString               ErrorMsg;            // 失败信息
    };

    /** 解析回包。无法解析时返回 bParsed=false。 */
    FParsed Parse(const FString& Json);

    /** 桌面 HTTP data 中的 achievement / platinum_achievement。数字需保留原文。 */
    bool ParseAchievementBean(const TSharedPtr<FJsonObject>& Bean,
                              ETapAchievementType Type, FTapAchievementResult& OutResult);
}
