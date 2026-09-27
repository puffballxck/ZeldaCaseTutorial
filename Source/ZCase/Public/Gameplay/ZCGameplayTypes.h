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
	MT_Falling UMETA(DisplayName = "Falling"),     // 下落
	MT_Climbing UMETA(DisplayName = "Climbing"),       // 墙面自由移动的玩法和动画状态
	MT_Mantling UMETA(DisplayName = "Mantling"),       // 从墙面翻上平台
	MT_ClimbDownLedge UMETA(DisplayName = "ClimbDownLedge") // 从平台边缘下降到墙面
};

/** CMC 实际物理模式，CurrentMT 只同步给现有玩法和动画使用，零保留给未指定状态 */
UENUM(BlueprintType)
enum class EZCCustomMovementMode : uint8
{
	None, Climbing, Mantling, ClimbDownLedge
};

/** 退出原因决定是否需要松键、离墙冷却，以及死亡时是否禁用移动 */
UENUM(BlueprintType)
enum class EZCClimbExitReason : uint8
{
	Released, Exhausted, HitReaction, LostSurface, Interrupted, Death
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
