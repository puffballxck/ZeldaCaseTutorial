// Fill out your copyright notice in the Description page of Project Settings.

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
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaTime) override;

	UPROPERTY()
	AZCCharBase* PlayerRef;

	UPROPERTY()
	UCharacterMovementComponent* MoveComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	float GroundSpeed = 0.0f;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	float AirSpeed = 0.0f;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	bool bShouldMove = false;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	bool bIsFalling = false;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	bool bIsGliding = false;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category ="References")
	bool bReadyToThrow = false;

	/** 由战斗组件的武器状态驱动；供 AnimBP 判断当前是否使用装备战斗姿态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References")
	bool bWeaponEquipped = false;

	/** 当前是否持有有效的目标锁定目标。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References|Target Lock")
	bool bIsTargetLocked = false;

	/** 水平移动速度相对角色朝向的局部角度，范围为 -180 到 180 度。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "References|Target Lock")
	float LockOnDirection = 0.0f;

	/**
	 * 将世界空间水平速度转换为相对角色 Yaw 的局部移动角度。
	 * 速度为零或输入非法时返回 CurrentDirection，避免站立时方向跳变或产生 NaN。
	 */
	UFUNCTION(BlueprintPure, Category = "References|Target Lock")
	static float CalculateLockOnDirection(
		const FRotator& ActorRotation,
		const FVector& HorizontalVelocity,
		float CurrentDirection = 0.0f);
};
