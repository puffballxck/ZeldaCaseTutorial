// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ZCBokoblinEnemy.h"

#include "AI/ZCBokoblinAIController.h"
#include "AIController.h"
#include "Characters/ZCCharBase.h"
#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

AZCBokoblinEnemy::AZCBokoblinEnemy()
{
	AttackRange = 130.0f;
	PatrolSpeed = 180.0f;
	ChaseSpeed = 400.0f;
	AttackDamage = 15.0f;
	AttackTraceRadius = 20.0f;
	AttackTraceBasePoint = TEXT("Wrist_R");
	AttackTraceTipPoint = TEXT("Finger_B_2_R");
	AIControllerClass = AZCBokoblinAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		// Bone driven traces must continue updating when the enemy is off-screen.
		EnemyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// A Bokoblin's unarmed trace may only damage the player. The base Combat
	// default remains false so existing player attacks retain their behavior.
	if (Combat)
	{
		Combat->SetPlayerOnlyDamage(true);
	}
}

void AZCBokoblinEnemy::ConfigureCombat()
{
	Super::ConfigureCombat();

	if (!Combat)
	{
		return;
	}
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		EnemyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// The Bokoblin has no weapon mesh. Reuse the character skeleton as the
	// source and let Combat resolve these names as socket or bone endpoints.
	Combat->InitializeAttackSource(
		GetMesh(),
		GetMesh(),
		AttackTraceBasePoint,
		AttackTraceTipPoint);
	Combat->SetTraceDamage(AttackDamage);
	Combat->SetTraceRadius(AttackTraceRadius);
	Combat->SetPlayerOnlyDamage(true);
}

bool AZCBokoblinEnemy::TryAttack(AActor* Target)
{
	if (const AZCBokoblinAIController* AI = Cast<AZCBokoblinAIController>(GetController());
		AI && AI->IsReturningHome())
	{
		return false;
	}

	APawn* TargetPawn = Cast<APawn>(Target);
	if (!TargetPawn || !IsValid(TargetPawn) || !TargetPawn->IsPlayerControlled())
	{
		return false;
	}

	if (!CanBeTargetLocked() || !Combat || !Combat->CanAcceptCombatInput()
		|| Combat->IsAttackActive() || Combat->GetWeaponState() == EZCWeaponState::Attacking)
	{
		return false;
	}

	if (const UZCAttributeComponent* TargetAttributes = TargetPawn->FindComponentByClass<UZCAttributeComponent>())
	{
		if (TargetAttributes->IsDead())
		{
			return false;
		}
	}
	if (const AZCCharBase* Player = Cast<AZCCharBase>(TargetPawn))
	{
		if (Player->IsDeathStarted())
		{
			return false;
		}
	}

	const float SafeAttackRange = FMath::IsFinite(AttackRange) ? FMath::Max(AttackRange, 0.0f) : 0.0f;
	constexpr float DistanceTolerance = 2.0f;
	constexpr float MaxVerticalAttackDelta = 120.0f;
	const FVector ToTarget = TargetPawn->GetActorLocation() - GetActorLocation();
	if (FMath::Abs(ToTarget.Z) > MaxVerticalAttackDelta
		|| ToTarget.SizeSquared2D() > FMath::Square(SafeAttackRange + DistanceTolerance))
	{
		return false;
	}

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		AIController->SetFocus(TargetPawn, EAIFocusPriority::Gameplay);
	}
	else if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	TArray<UAnimMontage*> ValidMontages;
	ValidMontages.Reserve(AttackMontages.Num());
	for (UAnimMontage* Montage : AttackMontages)
	{
		if (IsValid(Montage))
		{
			ValidMontages.Add(Montage);
		}
	}
	if (ValidMontages.IsEmpty())
	{
		return false;
	}

	const int32 MontageIndex = FMath::RandRange(0, ValidMontages.Num() - 1);
	Combat->SetAttackMontage(ValidMontages[MontageIndex]);
	return Combat->TryAttack();
}
