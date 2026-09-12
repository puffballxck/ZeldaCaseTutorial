// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "ZCGameplayTypes.generated.h"

UENUM(BlueprintType)
/** 角色移动状态，驱动速度、体力和 AnimBP 分支 */
enum class EMovementTypes : uint8
{
	MT_EMAX UMETA(DisplayName = "EMAX"),           // 默认或未设置
	MT_Walking UMETA(DisplayName = "Walking"),     // 地面移动
	MT_Exhausted UMETA(DisplayName = "Exhausted"), // 体力耗尽
	MT_Sprinting UMETA(DisplayName = "Sprinting"), // 冲刺
	MT_Gliding UMETA(DisplayName = "Gliding"),     // 滑翔
	MT_Falling UMETA(DisplayName = "Falling")      // 下落
};

UENUM(BlueprintType)
/** 当前选中的玩法符文 */
enum ERunes : uint8
{
	R_EMAX UMETA(DisplayName = "EMAX"),       // 默认或没有符文
	R_RBS UMETA(DisplayName = "RBS"),         // 遥控炸弹球
	R_RBB UMETA(DisplayName = "RBB"),         // 遥控炸弹方块
	R_Mag UMETA(DisplayName = "Magnesis"),    // 磁铁吸附
	R_Stasis UMETA(DisplayName = "Stasis"),   // 静止
	R_Ice UMETA(DisplayName = "Ice"),         // 制冰
};
