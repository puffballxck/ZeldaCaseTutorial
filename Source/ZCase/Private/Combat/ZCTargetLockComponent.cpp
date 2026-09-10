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

bool UZCTargetLockComponent::CycleTarget()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(Owner);
	APlayerController* PlayerController = OwnerPawn
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;
	if (!World || !IsValid(Owner) || !OwnerPawn || !OwnerPawn->IsLocallyControlled() || !PlayerController)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	if (!IsFiniteVector(ViewLocation) || !IsFiniteVector(ViewRotation.Vector()))
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	if (!IsFiniteVector(OwnerLocation))
	{
		return false;
	}

	const float SafeAcquisitionRadius = FMath::Max(AcquisitionRadius, 0.0f);
	const float RadiusSquared = FMath::Square(SafeAcquisitionRadius);
	struct FCycleCandidate
	{
		AActor* Target = nullptr;
		float DistanceSquared = 0.0f;
		uint32 UniqueID = 0;
	};

	TArray<FCycleCandidate> Candidates;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (Candidate == Owner || !IsValidTarget(Candidate))
		{
			continue;
		}

		const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
		const FVector TargetLocation = Targetable
			? Targetable->GetTargetLockLocation()
			: FVector::ZeroVector;
		if (!Targetable || !IsFiniteVector(TargetLocation))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(OwnerLocation, TargetLocation);
		if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > RadiusSquared)
		{
			continue;
		}

		// 中键循环只使用实际玩家视点的可见且处于安全区内的目标，
		// 但不再施加镜头前方角度限制，因此角色背后的目标也可参与循环。
		if (!IsTargetVisible(Candidate, ViewLocation)
			|| !IsTargetInScreenSafeArea(Candidate, PlayerController, ViewLocation, ViewRotation))
		{
			continue;
		}

		Candidates.Add({ Candidate, DistanceSquared, Candidate->GetUniqueID() });
	}

	if (Candidates.Num() == 0)
	{
		// 没有候选时保持当前状态，按键本身不产生任何锁定变化。
		return false;
	}

	Candidates.Sort([](const FCycleCandidate& Left, const FCycleCandidate& Right)
	{
		if (Left.DistanceSquared != Right.DistanceSquared)
		{
			return Left.DistanceSquared < Right.DistanceSquared;
		}

		return Left.UniqueID < Right.UniqueID;
	});

	int32 CurrentIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (Candidates[Index].Target == CurrentTarget.Get())
		{
			CurrentIndex = Index;
			break;
		}
	}

	const int32 NextIndex = CurrentIndex == INDEX_NONE
		? 0
		: (CurrentIndex + 1) % Candidates.Num();
	// SetTarget 内部直接替换目标，A->B 只会广播一次，不经过 ClearTarget。
	return SetTarget(Candidates[NextIndex].Target);
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

	const FVector OwnerLocation = Owner->GetActorLocation();
	if (!IsFiniteVector(OwnerLocation))
	{
		return false;
	}

	const APawn* OwnerPawn = Cast<APawn>(Owner);
	const APlayerController* PlayerController = OwnerPawn && OwnerPawn->IsLocallyControlled()
		? Cast<APlayerController>(OwnerPawn->GetController())
		: nullptr;

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

		const FVector OwnerToTarget = TargetLocation - OwnerLocation;
		const FVector ViewToTarget = TargetLocation - ViewLocation;
		const float DistanceSquared = OwnerToTarget.SizeSquared();
		if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > RadiusSquared || DistanceSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Distance = FMath::Sqrt(DistanceSquared);
		const float ViewDistanceSquared = ViewToTarget.SizeSquared();
		if (!FMath::IsFinite(ViewDistanceSquared) || ViewDistanceSquared <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector DirectionToTarget = ViewToTarget / FMath::Sqrt(ViewDistanceSquared);
		const float DirectionDot = FVector::DotProduct(ViewDirection, DirectionToTarget);
		if (!FMath::IsFinite(DirectionDot) || DirectionDot < CosHalfAngle)
		{
			continue;
		}

		if (!IsTargetVisible(Candidate, ViewLocation))
		{
			continue;
		}

		// 真实本地玩家沿用循环锁定的安全屏幕边界；自动化/非玩家对象
		// 没有本地视点时保留原有的参数化获取回退路径。
		if (PlayerController
			&& !IsTargetInScreenSafeArea(Candidate, PlayerController, ViewLocation, ViewForward.Rotation()))
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

	// 本地玩家目标离开屏幕安全区后先进入宽限计时；非玩家/自动化对象没有
	// 本地视点时回退到拥有者位置，避免测试对象被强行依赖屏幕投影。
	FVector ViewLocation = OwnerLocation;
	bool bTargetInScreenSafeArea = true;
	if (const APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (PawnOwner->IsLocallyControlled())
		{
			const APlayerController* PlayerController = Cast<APlayerController>(PawnOwner->GetController());
			if (PlayerController)
			{
				FRotator ViewRotation;
				PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
				bTargetInScreenSafeArea = IsTargetInScreenSafeArea(
					Target,
					PlayerController,
					ViewLocation,
					ViewRotation);
			}
		}
	}

	const float SafeDeltaTime = FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.0f) : 0.0f;
	if (!bTargetInScreenSafeArea)
	{
		OffScreenDuration = FMath::Min(OffScreenDuration + SafeDeltaTime, 1000000.0f);
		const float SafeGracePeriod = FMath::Max(OffScreenGracePeriod, 0.0f);
		if (SafeGracePeriod <= KINDA_SMALL_NUMBER || OffScreenDuration >= SafeGracePeriod)
		{
			// 锁定镜头下目标可以短暂离屏；持续离屏才结束锁定生命周期。
			ClearTarget();
			return;
		}
	}
	else
	{
		OffScreenDuration = 0.0f;
	}

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

bool UZCTargetLockComponent::IsTargetInScreenSafeArea(
	const AActor* Candidate,
	const APlayerController* PlayerController,
	const FVector& ViewLocation,
	const FRotator& ViewRotation) const
{
	if (!IsValidTarget(Candidate)
		|| !PlayerController
		|| !IsFiniteVector(ViewLocation)
		|| !IsFiniteVector(ViewRotation.Vector()))
	{
		return false;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
	const FVector TargetLocation = Targetable
		? Targetable->GetTargetLockLocation()
		: FVector::ZeroVector;
	if (!Targetable || !IsFiniteVector(TargetLocation))
	{
		return false;
	}

	const FVector ToTarget = TargetLocation - ViewLocation;
	if (!IsFiniteVector(ToTarget) || ToTarget.IsNearlyZero()
		|| FVector::DotProduct(ViewRotation.Vector(), ToTarget) <= 0.0f)
	{
		return false;
	}

	FVector2D ScreenPosition;
	if (!PlayerController->ProjectWorldLocationToScreen(TargetLocation, ScreenPosition, true))
	{
		return false;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0
		|| !FMath::IsFinite(ScreenPosition.X) || !FMath::IsFinite(ScreenPosition.Y))
	{
		return false;
	}

	const float SafeMargin = FMath::Clamp(ScreenSafeMargin, 0.0f, 0.25f);
	const float MinX = static_cast<float>(ViewportSizeX) * SafeMargin;
	const float MaxX = static_cast<float>(ViewportSizeX) * (1.0f - SafeMargin);
	const float MinY = static_cast<float>(ViewportSizeY) * SafeMargin;
	const float MaxY = static_cast<float>(ViewportSizeY) * (1.0f - SafeMargin);
	return ScreenPosition.X >= MinX && ScreenPosition.X <= MaxX
		&& ScreenPosition.Y >= MinY && ScreenPosition.Y <= MaxY;
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
	OffScreenDuration = 0.0f;
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
