// 请在项目设置的说明页面填写版权声明

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
	GlideRight = 0.0f;
	GlideForward = 0.0f;
	bIsExhausted = false;
	bIsExhaustedIdle = false;

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

	const FVector Velocity = PlayerRef->GetVelocity();
	GroundSpeed = UKismetMathLibrary::VSizeXY(Velocity);
	AirSpeed = Velocity.Z;
	bIsFalling = MoveComp->IsFalling();
	bShouldMove = !bIsFalling && GroundSpeed >5.0f && MoveComp->GetCurrentAcceleration().Size()>0;
	bIsGliding = PlayerRef->CurrentMT == EMovementTypes::MT_Gliding;
	bIsExhausted = PlayerRef->CurrentMT == EMovementTypes::MT_Exhausted;
	bIsExhaustedIdle = bIsExhausted && MoveComp->IsMovingOnGround() && GroundSpeed <= 5.0f;
	if (bIsGliding)
	{
		// 用实际水平速度持续驱动滑翔姿态，避免加速度归零时 BlendSpace 回到中心
		const FVector2D GlideInput = CalculateGlideBlendInput(
			Velocity,
			PlayerRef->GetControlRotation(),
			MoveComp->MaxFlySpeed);
		GlideRight = GlideInput.X;
		GlideForward = GlideInput.Y;
	}
	bReadyToThrow = PlayerRef->bReadyToThrow;
	// 拔刀完成后才进入装备姿势；攻击和收刀期间继续保持该姿势，直到收刀结束
	bWeaponEquipped = PlayerRef->Combat && PlayerRef->Combat->IsWeaponEquippedForAnimation();
	bGuarding = PlayerRef->Combat && PlayerRef->Combat->IsGuardPoseActive();
	LockOnDirection = bIsTargetLocked
		? CalculateLockOnDirection(
			PlayerRef->GetActorRotation(),
			FVector(Velocity.X, Velocity.Y, 0.0f),
			LockOnDirection)
		: 0.0f;
}

FVector2D UZCAnimInst::CalculateGlideBlendInput(
	const FVector& WorldVelocity,
	const FRotator& ControlRotation,
	const float MaxFlySpeed)
{
	if (WorldVelocity.ContainsNaN()
		|| !FMath::IsFinite(ControlRotation.Yaw)
		|| !FMath::IsFinite(MaxFlySpeed)
		|| MaxFlySpeed <= KINDA_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}

	const FVector HorizontalVelocity(WorldVelocity.X, WorldVelocity.Y, 0.0f);
	const FRotator ControlYaw(0.0f, ControlRotation.Yaw, 0.0f);
	const FVector LocalInput = ControlYaw.UnrotateVector(HorizontalVelocity / MaxFlySpeed);
	return FVector2D(
		FMath::Clamp(LocalInput.Y, -1.0f, 1.0f),
		FMath::Clamp(LocalInput.X, -1.0f, 1.0f));
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
