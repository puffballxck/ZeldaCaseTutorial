// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/ZCAnimInst.h"
#include "Combat/ZCCombatComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UZCAnimInst::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	PlayerRef = Cast<AZCCharBase>(TryGetPawnOwner());
	if (PlayerRef == nullptr) return;
	MoveComp = PlayerRef->GetCharacterMovement(); 
}

void UZCAnimInst::NativeUpdateAnimation(float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	if (!PlayerRef || !MoveComp) return;

	GroundSpeed = UKismetMathLibrary::VSizeXY(PlayerRef->GetVelocity());
	AirSpeed = PlayerRef->GetVelocity().Z;
	bIsFalling = MoveComp->IsFalling();
	bShouldMove = !bIsFalling && GroundSpeed >5.0f && MoveComp->GetCurrentAcceleration().Size()>0;
	bIsGliding = PlayerRef->CurrentMT == EMovementTypes::MT_Gliding;
	bReadyToThrow = PlayerRef->bReadyToThrow;
	// 拔刀完成后才进入装备姿势；攻击和收刀期间继续保持该姿势，直到收刀结束。
	bWeaponEquipped = PlayerRef->Combat && PlayerRef->Combat->IsWeaponEquippedForAnimation();
}
