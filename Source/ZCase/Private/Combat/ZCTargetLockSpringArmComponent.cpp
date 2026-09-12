// 版权所有 Epic Games, Inc，保留所有权利

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
			const float Speed = FMath::IsFinite(HeightInterpSpeed) ? FMath::Max(HeightInterpSpeed, 0.1f) : 4.0f;
			TargetOffset = FMath::VInterpTo(TargetOffset, UnlockedTargetOffset,
				FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.0f) : 0.0f, Speed);
			if (TargetOffset.Equals(UnlockedTargetOffset, 0.1f))
			{
				TargetOffset = UnlockedTargetOffset;
				bLockFramingActive = false;
			}
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
	// 不仅在首次抬高时检测角色走到新天花板下方后，也必须重新约束臂原点
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

	// 锁定构图在本帧输入和移动之后执行约束时不叠加相机滞后，避免
	// 已经修正的朝向再次被 lag 拉出边界；解锁后沿用原有 lag 配置
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

	// 只关注战斗锚点，允许敌人的头、肢体和箭头在近身时出框
	// 加入玩家躯干位置，避免为了看敌人而完全忽略玩家所在方向
	TArray<FVector, TInlineAllocator<2>> Points;
	Points.Add(Targetable->GetTargetLockCameraLocation());
	const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
	Points.Add(Player->GetActorLocation() + FVector::UpVector
		* (PlayerCapsule ? PlayerCapsule->GetScaledCapsuleHalfHeight() * 0.25f : 0.0f));
	for (const FVector& Point : Points)
	{
		if (Point.ContainsNaN())
		{
			return;
		}
	}

	// 使用更新过的真实相机位置（包括碰撞缩臂），按最坏的边界误差修正
	auto Measure = [&](FVector& WorstDirection)
	{
		const FTransform CameraTransform = Player->FollowCamera->GetComponentTransform();
		double WorstError = 0.0;
		for (const FVector& Point : Points)
		{
			FVector Direction = CameraTransform.InverseTransformVectorNoScale(Point - CameraTransform.GetLocation()).GetSafeNormal();
			if (Direction.IsNearlyZero())
			{
				// 相机恰好贴到锚点时视线无定义，不能把零向量送入四元数修正
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
	const FRotator InputRotation = PC->GetControlRotation();
	const FVector CurrentOffset = TargetOffset;
	FRotator BestRotation = InputRotation;
	FVector BestOffset = CurrentOffset;
	FVector WorstDirection;
	double BestError = TNumericLimits<double>::Max();

	// 每帧从原始高度评估；低位能容纳两个锚点时优先低位，不保留旧的抬高档位
	// 这里只求期望姿态最后统一插值，求解过程不能提前返回并暴露瞬时姿态
	for (int32 HeightStep = 0; HeightStep < 3; ++HeightStep)
	{
		TargetOffset = UnlockedTargetOffset;
		TargetOffset.Z += MaxHeight * HeightStep / 2.0f;
		TargetOffset = ConstrainHeight(TargetOffset);
		PC->SetControlRotation(InputRotation);
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
			if (Error <= 0.0001 || Iteration == 8)
			{
				break;
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
		if (BestError <= 0.0001)
		{
			break;
		}
	}

	const float SafeDeltaTime = FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.0f) : 0.0f;
	const float RotationSpeed = FMath::IsFinite(FramingInterpSpeed) ? FMath::Max(FramingInterpSpeed, 0.1f) : 6.0f;
	const float TurnRate = FMath::IsFinite(MaxFramingTurnRate) ? FMath::Max(MaxFramingTurnRate, 1.0f) : 90.0f;
	const float HeightSpeed = FMath::IsFinite(HeightInterpSpeed) ? FMath::Max(HeightInterpSpeed, 0.1f) : 4.0f;
	const FRotator SmoothRotation = FMath::RInterpTo(InputRotation, BestRotation, SafeDeltaTime, RotationSpeed);
	// 自动纠正限速，尤其在目标从头顶/身后掠过时避免镜头瞬间翻转
	PC->SetControlRotation(FMath::RInterpConstantTo(InputRotation, SmoothRotation, SafeDeltaTime, TurnRate));
	TargetOffset = ConstrainHeight(FMath::VInterpTo(CurrentOffset, BestOffset, SafeDeltaTime, HeightSpeed));
	Super::UpdateDesiredArmLocation(bDoTrace, false, false, DeltaTime);
}
