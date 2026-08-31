// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCTargetLockComponent.h"

#include "Combat/ZCTargetable.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	float NormalizeTargetLockWeight(const float Weight)
	{
		return FMath::Max(Weight, 0.0f);
	}
}

UZCTargetLockComponent::UZCTargetLockComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UZCTargetLockComponent::SetTarget(AActor* Candidate)
{
	// 只有实现目标接口且明确允许锁定的对象才能成为目标。
	if (!IsValidTarget(Candidate))
	{
		return false;
	}

	if (CurrentTarget.Get() == Candidate)
	{
		// 重复锁定同一对象保持幂等，不重复触发目标变更事件。
		return true;
	}

	ReplaceTarget(Candidate);
	return true;
}

bool UZCTargetLockComponent::AcquireBestTarget(
	const FVector& ViewLocation,
	const FVector& ViewForward)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !IsValid(Owner) || !IsFiniteVector(ViewLocation) || !IsFiniteVector(ViewForward))
	{
		return false;
	}

	const FVector ViewDirection = ViewForward.GetSafeNormal();
	if (ViewDirection.IsNearlyZero())
	{
		return false;
	}

	const float SafeAcquisitionRadius = FMath::Max(AcquisitionRadius, 0.0f);
	const float SafeHalfAngle = FMath::Clamp(AcquisitionHalfAngle, 0.0f, 180.0f);
	const float RadiusSquared = FMath::Square(SafeAcquisitionRadius);
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SafeHalfAngle));
	const float SafeAngleWeight = NormalizeTargetLockWeight(AngleWeight);
	const float SafeDistanceWeight = NormalizeTargetLockWeight(DistanceWeight);
	const float WeightTotal = SafeAngleWeight + SafeDistanceWeight;

	AActor* BestTarget = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	float BestAngleScore = TNumericLimits<float>::Max();
	float BestDistanceScore = TNumericLimits<float>::Max();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValidTarget(Candidate) || Candidate == Owner)
		{
			continue;
		}

		const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
		if (!Targetable)
		{
			continue;
		}

		const FVector TargetLocation = Targetable->GetTargetLockLocation();
		if (!IsFiniteVector(TargetLocation))
		{
			continue;
		}

		const FVector ToTarget = TargetLocation - ViewLocation;
		const float DistanceSquared = ToTarget.SizeSquared();
		if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > RadiusSquared || DistanceSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Distance = FMath::Sqrt(DistanceSquared);
		const FVector DirectionToTarget = ToTarget / Distance;
		const float DirectionDot = FVector::DotProduct(ViewDirection, DirectionToTarget);
		if (!FMath::IsFinite(DirectionDot) || DirectionDot < CosHalfAngle)
		{
			continue;
		}

		if (!IsTargetVisible(Candidate, ViewLocation))
		{
			continue;
		}

		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DirectionDot, -1.0f, 1.0f)));
		const float AngleScore = SafeHalfAngle > KINDA_SMALL_NUMBER
			? FMath::Clamp(AngleDegrees / SafeHalfAngle, 0.0f, 1.0f)
			: 0.0f;
		const float DistanceScore = SafeAcquisitionRadius > KINDA_SMALL_NUMBER
			? FMath::Clamp(Distance / SafeAcquisitionRadius, 0.0f, 1.0f)
			: 0.0f;
		const float Score = WeightTotal > KINDA_SMALL_NUMBER
			? (SafeAngleWeight * AngleScore + SafeDistanceWeight * DistanceScore) / WeightTotal
			: AngleScore;

		// Score 以屏幕中心角度为主；用距离和唯一 ID 作为稳定的平局裁决，
		// 避免 TActorIterator 的内部顺序改变时锁定目标发生无意义跳变。
		const bool bBetterScore = Score < BestScore - KINDA_SMALL_NUMBER;
		const bool bEqualScore = FMath::IsNearlyEqual(Score, BestScore, KINDA_SMALL_NUMBER);
		const bool bBetterTieBreak = bEqualScore
			&& (AngleScore < BestAngleScore - KINDA_SMALL_NUMBER
				|| (FMath::IsNearlyEqual(AngleScore, BestAngleScore, KINDA_SMALL_NUMBER)
					&& (DistanceScore < BestDistanceScore - KINDA_SMALL_NUMBER
						|| (FMath::IsNearlyEqual(DistanceScore, BestDistanceScore, KINDA_SMALL_NUMBER)
							&& (!BestTarget || Candidate->GetUniqueID() < BestTarget->GetUniqueID())))));

		if (bBetterScore || bBetterTieBreak)
		{
			BestTarget = Candidate;
			BestScore = Score;
			BestAngleScore = AngleScore;
			BestDistanceScore = DistanceScore;
		}
	}

	if (!BestTarget)
	{
		return false;
	}

	return SetTarget(BestTarget);
}

void UZCTargetLockComponent::ClearTarget()
{
	ReplaceTarget(nullptr);
}

void UZCTargetLockComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Target = CurrentTarget.Get();
	AActor* Owner = GetOwner();
	if (!IsValidTarget(Target) || !IsValid(Owner))
	{
		// Tick 负责兜底清理被销毁或不再可锁定的目标。
		ClearTarget();
		return;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(Target);
	const FVector TargetLocation = Targetable ? Targetable->GetTargetLockLocation() : FVector::ZeroVector;
	const FVector OwnerLocation = Owner->GetActorLocation();
	if (!Targetable || !IsFiniteVector(TargetLocation) || !IsFiniteVector(OwnerLocation))
	{
		ClearTarget();
		return;
	}

	const float SafeLostDistance = FMath::Max(LockLostDistance, 0.0f);
	const float DistanceSquared = FVector::DistSquared(OwnerLocation, TargetLocation);
	if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > FMath::Square(SafeLostDistance))
	{
		// 目标离开锁定距离后立即解除，避免角色继续朝向远处目标。
		ClearTarget();
		return;
	}

	// 目标持续可见性从玩家的实际视点开始检查；没有 PlayerController 的
	// 非玩家/自动化对象才回退到拥有者位置，避免锁定被相机臂遮挡判断误导。
	FVector ViewLocation = OwnerLocation;
	if (const APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(PawnOwner->GetController()))
		{
			FRotator ViewRotation;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
			if (!IsFiniteVector(ViewLocation))
			{
				ViewLocation = OwnerLocation;
			}
		}
	}

	const float SafeDeltaTime = FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.0f) : 0.0f;
	if (!IsTargetVisible(Target, ViewLocation))
	{
		OccludedDuration = FMath::Min(OccludedDuration + SafeDeltaTime, 1000000.0f);
		const float SafeGracePeriod = FMath::Max(OcclusionGracePeriod, 0.0f);
		if (SafeGracePeriod <= KINDA_SMALL_NUMBER || OccludedDuration >= SafeGracePeriod)
		{
			// 短时遮挡允许镜头保持锁定；持续遮挡则结束锁定生命周期。
			ClearTarget();
		}
	}
	else
	{
		OccludedDuration = 0.0f;
	}
}

void UZCTargetLockComponent::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (CurrentTarget.Get() == DestroyedActor)
	{
		// OnDestroyed 回调比下一帧 Tick 更早清除已销毁目标。
		ClearTarget();
	}
}

bool UZCTargetLockComponent::IsValidTarget(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(UZCTargetable::StaticClass()))
	{
		return false;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
	// 接口实现仍需通过自身的可锁定策略，例如死亡状态检查。
	return Targetable && Targetable->CanBeTargetLocked();
}

bool UZCTargetLockComponent::IsTargetVisible(const AActor* Candidate, const FVector& ViewLocation) const
{
	if (!IsValid(Candidate) || !IsFiniteVector(ViewLocation) || !GetWorld())
	{
		return false;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
	if (!Targetable)
	{
		return false;
	}

	const FVector TargetLocation = Targetable->GetTargetLockLocation();
	if (!IsFiniteVector(TargetLocation))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZCTargetLockVisibility), true);
	if (AActor* Owner = GetOwner())
	{
		QueryParams.AddIgnoredActor(Owner);
	}

	FHitResult HitResult;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TargetLocation,
		ECC_Visibility,
		QueryParams);

	// 没有阻挡体，或第一阻挡体就是候选目标本身，都视为可见。
	return !bHit || HitResult.GetActor() == Candidate;
}

void UZCTargetLockComponent::ReplaceTarget(AActor* NewTarget)
{
	AActor* PreviousTarget = CurrentTarget.Get();
	if (PreviousTarget == NewTarget && !(NewTarget == nullptr && CurrentTarget.IsStale()))
	{
		return;
	}

	if (PreviousTarget)
	{
		// 替换前解绑旧目标，避免旧对象销毁时回调到当前锁定组件。
		PreviousTarget->OnDestroyed.RemoveDynamic(this, &UZCTargetLockComponent::HandleTargetDestroyed);
	}

	CurrentTarget = NewTarget;
	OccludedDuration = 0.0f;
	if (NewTarget)
	{
		// 有目标时监听销毁事件并开启 Tick，持续验证目标可用性。
		NewTarget->OnDestroyed.AddUniqueDynamic(this, &UZCTargetLockComponent::HandleTargetDestroyed);
		SetComponentTickEnabled(true);
	}
	else
	{
		// 没有目标时无需每帧验证，关闭 Tick 以结束锁定生命周期。
		SetComponentTickEnabled(false);
	}

	OnTargetChanged.Broadcast(PreviousTarget, NewTarget);
}
