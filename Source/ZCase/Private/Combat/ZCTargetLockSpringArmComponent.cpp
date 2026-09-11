// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCTargetLockSpringArmComponent.h"

#include "Camera/CameraComponent.h"
#include "Characters/ZCCharBase.h"
#include "Combat/ZCTargetable.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"

void UZCTargetLockSpringArmComponent::UpdateDesiredArmLocation(
	bool bDoTrace, bool bDoLocationLag, bool bDoRotationLag, float DeltaTime)
{
	AZCCharBase* Player = Cast<AZCCharBase>(GetOwner());
	APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	AActor* Target = Player && Player->TargetLock ? Player->TargetLock->GetCurrentTarget() : nullptr;
	const IZCTargetable* Targetable = IsValid(Target) ? Cast<IZCTargetable>(Target) : nullptr;
	const bool bCanFrame = Player && PC && PC->IsLocalController() && Player->FollowCamera
		&& !PC->GetControlRotation().ContainsNaN() && !TargetOffset.ContainsNaN()
		&& Player->CanUseTargetLock() && Targetable && Targetable->CanBeTargetLocked();
	if (!bCanFrame)
	{
		if (bLockFramingActive)
		{
			TargetOffset = UnlockedTargetOffset;
			bLockFramingActive = false;
		}
		Super::UpdateDesiredArmLocation(bDoTrace, bDoLocationLag, bDoRotationLag, DeltaTime);
		return;
	}
	if (!bLockFramingActive)
	{
		UnlockedTargetOffset = TargetOffset;
		bLockFramingActive = true;
	}
	const float MaxHeight = FMath::IsFinite(MaxFramingHeight) ? FMath::Max(MaxFramingHeight, 0.0f) : 300.0f;
	// 不仅在首次抬高时检测。角色走到新天花板下方后，也必须重新约束臂原点。
	auto ConstrainHeight = [&](FVector CandidateOffset)
	{
		CandidateOffset.Z = FMath::Clamp(CandidateOffset.Z,
			UnlockedTargetOffset.Z, UnlockedTargetOffset.Z + MaxHeight);
		if (bDoTrace && GetWorld() && !CandidateOffset.Equals(UnlockedTargetOffset))
		{
			FHitResult HeightHit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetLockCameraHeight), false, Player);
			const FVector Origin = GetComponentLocation();
			if (GetWorld()->SweepSingleByChannel(HeightHit, Origin + UnlockedTargetOffset, Origin + CandidateOffset,
				FQuat::Identity, ProbeChannel, FCollisionShape::MakeSphere(FMath::Max(ProbeSize, 0.0f)), Params))
			{
				return FMath::Lerp(UnlockedTargetOffset, CandidateOffset,
					HeightHit.bStartPenetrating ? 0.0f : HeightHit.Time);
			}
		}
		return CandidateOffset;
	};
	TargetOffset = ConstrainHeight(TargetOffset);

	// 锁定构图在本帧输入和移动之后执行。约束时不叠加相机滞后，避免
	// 已经修正的朝向再次被 lag 拉出边界；解锁后沿用原有 lag 配置。
	Super::UpdateDesiredArmLocation(bDoTrace, false, false, DeltaTime);
	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport
		|| !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		return;
	}
	const double ProjectionX = ProjectionData.ProjectionMatrix.M[0][0];
	const double ProjectionY = ProjectionData.ProjectionMatrix.M[1][1];
	if (!FMath::IsFinite(ProjectionX) || !FMath::IsFinite(ProjectionY)
		|| ProjectionX <= UE_SMALL_NUMBER || ProjectionY <= UE_SMALL_NUMBER)
	{
		return;
	}
	const float Margin = FMath::IsFinite(ScreenMargin) ? FMath::Clamp(ScreenMargin, 0.0f, 0.3f) : 0.12f;
	const double MaxSlopeX = (1.0 - 2.0 * Margin) / ProjectionX;
	const double MaxSlopeY = (1.0 - 2.0 * Margin) / ProjectionY;

	// 同时容纳身体、锁定点和箭头，不能只让胸口一个点留在画面里。
	TArray<FVector, TInlineAllocator<10>> Points;
	Points.Add(Targetable->GetTargetLockCameraLocation());
	const float ArrowHeight = FMath::IsFinite(IndicatorHeight) ? FMath::Max(IndicatorHeight, 0.0f) : 80.0f;
	Points.Add(Targetable->GetTargetLockLocation() + FVector::UpVector * ArrowHeight);
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			const FVector Center = Capsule->GetComponentLocation();
			const FVector Extent(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				Points.Add(Center + Extent * FVector(Corner & 1 ? 1 : -1, Corner & 2 ? 1 : -1, Corner & 4 ? 1 : -1));
			}
		}
	}
	for (const FVector& Point : Points)
	{
		if (Point.ContainsNaN())
		{
			return;
		}
	}

	// 使用更新过的真实相机位置（包括碰撞缩臂），按最坏的边界误差修正。
	auto Measure = [&](FVector& WorstDirection)
	{
		const FTransform CameraTransform = Player->FollowCamera->GetComponentTransform();
		double WorstError = 0.0;
		for (const FVector& Point : Points)
		{
			FVector Direction = CameraTransform.InverseTransformVectorNoScale(Point - CameraTransform.GetLocation()).GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				// 相机恰好贴到锚点时视线无定义，不能把零向量送入四元数修正。
				Direction = -FVector::ForwardVector;
			}
			const double Error = Direction.X <= UE_SMALL_NUMBER ? 2.0 - Direction.X
				: FMath::Max(FMath::Abs(Direction.Y) - Direction.X * MaxSlopeX,
					FMath::Abs(Direction.Z) - Direction.X * MaxSlopeY);
			if (Error > WorstError)
			{
				WorstError = Error;
				WorstDirection = Direction;
			}
		}
		return WorstError;
	};
	FVector WorstDirection;
	double BestError = Measure(WorstDirection);
	if (BestError <= 0.0001)
	{
		return; // 不回中、不恢复旧视角，也不主动降低已经采用的构图高度。
	}
	FRotator BestRotation = PC->GetControlRotation();
	FVector BestOffset = TargetOffset;
	const FVector StartingOffset = TargetOffset;
	FVector LastTriedOffset = StartingOffset;
	// 常规边界只修正角度；无法容纳整个目标时再分两档抬高相机。
	for (int32 HeightStep = 0; HeightStep < 3; ++HeightStep)
	{
		TargetOffset = StartingOffset;
		TargetOffset.Z = FMath::Max(StartingOffset.Z, UnlockedTargetOffset.Z + MaxHeight * HeightStep / 2.0f);
		TargetOffset = ConstrainHeight(TargetOffset);
		if (HeightStep > 0 && TargetOffset.Equals(LastTriedOffset, 0.1f))
		{
			continue; // 已达到最高档，或不同档位被同一天花板限制在相同高度。
		}
		LastTriedOffset = TargetOffset;
		PC->SetControlRotation(BestRotation);
		for (int32 Iteration = 0; Iteration <= 8; ++Iteration)
		{
			Super::UpdateDesiredArmLocation(bDoTrace, false, false, DeltaTime);
			const double Error = Measure(WorstDirection);
			if (Error < BestError)
			{
				BestError = Error;
				BestRotation = PC->GetControlRotation();
				BestOffset = TargetOffset;
			}
			if (Error <= 0.0001)
			{
				return;
			}
			if (Iteration == 8)
			{
				break; // 第八次修正也先评估并记录，再考虑换高度。
			}
			const FQuat CameraRotation = Player->FollowCamera->GetComponentQuat();
			const double Forward = FMath::Max(WorstDirection.X, 0.001);
			const FVector BoundaryDirection = FVector(Forward,
				FMath::Clamp(WorstDirection.Y, -Forward * MaxSlopeX, Forward * MaxSlopeX),
				FMath::Clamp(WorstDirection.Z, -Forward * MaxSlopeY, Forward * MaxSlopeY)).GetSafeNormal();
			const FQuat Correction = FQuat::FindBetweenNormals(
				CameraRotation.RotateVector(BoundaryDirection), CameraRotation.RotateVector(WorstDirection));
			FRotator Corrected = (Correction * PC->GetControlRotation().Quaternion()).Rotator();
			Corrected.Roll = 0.0f;
			Corrected.Pitch = FMath::Clamp(FMath::UnwindDegrees(Corrected.Pitch), -85.0, 85.0);
			if (Corrected.ContainsNaN())
			{
				break;
			}
			PC->SetControlRotation(Corrected);
		}
	}
	// 极端遮挡/贴脸可能没有可行构图，保留误差最小的方案；不绕过碰撞。
	PC->SetControlRotation(BestRotation);
	TargetOffset = BestOffset;
	Super::UpdateDesiredArmLocation(bDoTrace, false, false, DeltaTime);
}
