// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/ZCEnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/ZCAttributeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogZCEnemy, Log, All);

AZCEnemyBase::AZCEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;
	Attributes = CreateDefaultSubobject<UZCAttributeComponent>(TEXT("Attributes"));
}

float AZCEnemyBase::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	const float EngineDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!Attributes || EngineDamage <= 0.0f)
	{
		return 0.0f;
	}

	const FZCDamageResult Result = Attributes->ApplyDamage(EngineDamage);
	if (Result.AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	// 致命伤害只进入死亡分支，避免先播放 Hit React 再立即被 Death 打断。
	if (Result.bBecameDead)
	{
		HandleDeath();
	}
	else
	{
		PlayHitReact();
	}

	return Result.AppliedDamage;
}

bool AZCEnemyBase::CanBeTargetLocked() const
{
	return Attributes && !Attributes->IsDead() && !bDeathStarted;
}

FVector AZCEnemyBase::GetTargetLockLocation() const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HeightOffset = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.5f : 0.0f;
	return GetActorLocation() + FVector::UpVector * HeightOffset;
}

void AZCEnemyBase::PlayHitReact()
{
	USkeletalMeshComponent* EnemyMesh = GetMesh();
	UAnimInstance* AnimInstance = EnemyMesh ? EnemyMesh->GetAnimInstance() : nullptr;
	if (!HitReactMontage || !AnimInstance)
	{
		if (!bHitReactDiagnosticIssued)
		{
			bHitReactDiagnosticIssued = true;
			UE_LOG(
				LogZCEnemy,
				Warning,
				TEXT("%s received non-lethal damage but cannot play Hit React: Montage or AnimInstance is missing."),
				*GetNameSafe(this));
		}
		return;
	}

	AnimInstance->Montage_Play(HitReactMontage);
}

void AZCEnemyBase::HandleDeath()
{
	if (bDeathStarted)
	{
		return;
	}

	bDeathStarted = true;
	SetCanBeDamaged(false);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 第一版只忽略 Pawn，保留对世界的碰撞，避免倒地后穿地或位置失稳。
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	USkeletalMeshComponent* EnemyMesh = GetMesh();
	UAnimInstance* AnimInstance = EnemyMesh ? EnemyMesh->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.05f);
	}

	if (!DeathMontage || !AnimInstance)
	{
		if (!bDeathDiagnosticIssued)
		{
			bDeathDiagnosticIssued = true;
			UE_LOG(
				LogZCEnemy,
				Warning,
				TEXT("%s died but cannot play Death Montage: Montage or AnimInstance is missing."),
				*GetNameSafe(this));
		}
		return;
	}

	AnimInstance->Montage_Play(DeathMontage);
}
