// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Characters/ZCCharBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZCAnimInst.generated.h"

class AZCCharBase;
class UCharacterMovementComponent;
UCLASS()
class ZCASE_API UZCAnimInst : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** 初始化角色与移动组件引用，后续 Tick 只读取这些运行时对象 */
	virtual void NativeInitializeAnimation() override;
	/** 每帧同步移动、装备、防御和目标锁定状态给 AnimBP */
	virtual void NativeUpdateAnimation(float DeltaTime) override;

	UPROPERTY()
	/** 当前 AnimInstance 所属的玩家角色 */
	AZCCharBase* PlayerRef;

	UPROPERTY()
	/** 当前角色的移动组件，缺失时动画状态保持安全默认值 */
	UCharacterMovementComponent* MoveComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	/** 水平移动速度，单位为厘米每秒 */
	float GroundSpeed = 0.0f;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	/** 垂直移动速度，单位为厘米每秒 */
	float AirSpeed = 0.0f;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	/** 是否存在足够的移动速度供 BlendSpace 使用 */
	bool bShouldMove = false;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	/** 是否处于 Falling 移动模式 */
	bool bIsFalling = false;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	/** 是否处于滑翔状态 */
	bool bIsGliding = false;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	/** 是否持有可投掷物体 */
	bool bReadyToThrow = false;

	/** 由战斗组件的武器状态驱动；供 AnimBP 判断当前是否使用装备战斗姿态 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	bool bWeaponEquipped = false;

	/** 由 Combat 独立防御状态驱动；供 ABP_Link 接 Guard_Wait */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References|Defense")
	bool bGuarding = false;

	/** 当前是否持有有效的目标锁定目标 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References|Target Lock")
	bool bIsTargetLocked = false;

	/** 水平移动速度相对角色朝向的局部角度，范围为 -180 到 180 度 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References|Target Lock")
	float LockOnDirection = 0.0f;

	/**
	 * 将世界空间水平速度转换为相对角色 Yaw 的局部移动角度
	 * 速度为零或输入非法时返回 CurrentDirection，避免站立时方向跳变或产生 NaN
	 */
	UFUNCTION(BlueprintPure, Category = "References|Target Lock")
	static float CalculateLockOnDirection(
		const FRotator& ActorRotation,
		const FVector& HorizontalVelocity,
		float CurrentDirection = 0.0f);
};
