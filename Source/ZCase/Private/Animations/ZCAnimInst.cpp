// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/ZCAnimInst.h"
#include "Combat/ZCCombatComponent.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Combat/ZCTargetable.h"
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

	if (!IsValid(PlayerRef))
	{
		PlayerRef = Cast<AZCCharBase>(TryGetPawnOwner());
	}
	if (!IsValid(PlayerRef))
	{
		bIsTargetLocked = false;
		bGuarding = false;
		LockOnDirection = 0.0f;
		return;
	}

	AActor* CurrentTarget = PlayerRef->TargetLock ? PlayerRef->TargetLock->GetCurrentTarget() : nullptr;
	const IZCTargetable* Targetable = IsValid(CurrentTarget)
		&& CurrentTarget->GetClass()->ImplementsInterface(UZCTargetable::StaticClass())
		? Cast<IZCTargetable>(CurrentTarget)
		: nullptr;
	bIsTargetLocked = Targetable && Targetable->CanBeTargetLocked();

	if (!IsValid(MoveComp))
	{
		MoveComp = PlayerRef->GetCharacterMovement();
	}
	if (!IsValid(MoveComp))
	{
		return;
	}

	GroundSpeed = UKismetMathLibrary::VSizeXY(PlayerRef->GetVelocity());
	AirSpeed = PlayerRef->GetVelocity().Z;
	bIsFalling = MoveComp->IsFalling();
	bShouldMove = !bIsFalling && GroundSpeed >5.0f && MoveComp->GetCurrentAcceleration().Size()>0;
	bIsGliding = PlayerRef->CurrentMT == EMovementTypes::MT_Gliding;
	bReadyToThrow = PlayerRef->bReadyToThrow;
	// 拔刀完成后才进入装备姿势；攻击和收刀期间继续保持该姿势，直到收刀结束。
	bWeaponEquipped = PlayerRef->Combat && PlayerRef->Combat->IsWeaponEquippedForAnimation();
	bGuarding = PlayerRef->Combat && PlayerRef->Combat->IsGuardPoseActive();
	LockOnDirection = bIsTargetLocked
		? CalculateLockOnDirection(
			PlayerRef->GetActorRotation(),
			FVector(PlayerRef->GetVelocity().X, PlayerRef->GetVelocity().Y, 0.0f),
			LockOnDirection)
		: 0.0f;
}

float UZCAnimInst::CalculateLockOnDirection(
	const FRotator& ActorRotation,
	const FVector& HorizontalVelocity,
	const float CurrentDirection)
{
	const float StableDirection = FMath::IsFinite(CurrentDirection)
		? FMath::Clamp(FMath::UnwindDegrees(CurrentDirection), -180.0f, 180.0f)
		: 0.0f;

	if (!FMath::IsFinite(ActorRotation.Yaw)
		|| !FMath::IsFinite(HorizontalVelocity.X)
		|| !FMath::IsFinite(HorizontalVelocity.Y)
		|| HorizontalVelocity.SizeSquared2D() <= FMath::Square(KINDA_SMALL_NUMBER))
	{
		return StableDirection;
	}

	const FRotator ActorYaw(0.0f, ActorRotation.Yaw, 0.0f);
	const FVector LocalVelocity = ActorYaw.UnrotateVector(
		FVector(HorizontalVelocity.X, HorizontalVelocity.Y, 0.0f));
	const float DirectionRadians = FMath::Atan2(LocalVelocity.Y, LocalVelocity.X);
	if (!FMath::IsFinite(DirectionRadians))
	{
		return StableDirection;
	}

	const float DirectionDegrees = FMath::RadiansToDegrees(DirectionRadians);
	return FMath::Clamp(FMath::UnwindDegrees(DirectionDegrees), -180.0f, 180.0f);
}
