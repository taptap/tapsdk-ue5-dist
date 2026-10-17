// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

// 持久回调委托（registerRelationCallback 注册后触发）
// relation_code 在 JSON 中是字符串 "N"，已在桥接层转换为 int32
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnRelationCallback,
    int32 /*Code*/, int32 /*NewFansCount*/, int32 /*UnreadMsgCount*/);
