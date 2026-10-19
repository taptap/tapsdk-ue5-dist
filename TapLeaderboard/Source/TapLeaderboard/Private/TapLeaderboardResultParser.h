// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapLeaderboardTypes.h"

/**
 * 解析移动端持久回调 JSON（Android / iOS 共用协议）。
 *
 * 统一格式（op 字段区分操作）：
 *   成功: {"op":"loadScores","state":"success","data":"<嵌套JSON字符串>"}
 *   失败: {"op":"loadScores","state":"failure","errCode":<int>,"errMessage":"..."}
 *   事件: {"op":"leaderboardCallback","state":"event","code":<int>,"message":"..."}
 *
 * op 取值：submitScores / loadScores / loadCurrentPlayerScore /
 *          loadPlayerCenteredScores / leaderboardCallback / shareCallback
 */
namespace TapLeaderboardResultParser
{
    struct FParsed
    {
        bool    bParsed    = false;
        FString Op;
        bool    bIsSuccess = false;
        bool    bIsEvent   = false;
        FString Data;          // success 时的嵌套 JSON 字符串
        int32   ErrCode    = 0;
        FString ErrMsg;
        // leaderboardCallback / shareCallback 附加字段
        int32   EventCode  = 0;
        FString EventMessage;
    };

    FParsed Parse(const FString& Json);

    /** 解析 loadScores / loadPlayerCenteredScores 的 data 字段 */
    FTapLeaderboardScoresResult ParseScoresResult(const FString& DataJson);

    /** 解析 loadCurrentPlayerScore 的 data 字段 */
    FTapUserScoreResult ParseUserScoreResult(const FString& DataJson);

    /** 解析 submitScores 的 data 字段 */
    FTapSubmitScoresResult ParseSubmitResult(const FString& DataJson);

}
