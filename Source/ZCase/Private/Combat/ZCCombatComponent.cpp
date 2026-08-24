// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UZCCombatComponent::UZCCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DrawMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_DrawSword.AM_DrawSword"));
	DrawSwordMontage = DrawMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> SheathMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_SheathSword.AM_SheathSword"));
	SheathSwordMontage = SheathMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Attack_01.AM_Attack_01"));
	AttackMontage = AttackMontageFinder.Object;
}

void UZCCombatComponent::InitializeEquipment(
	USkeletalMeshComponent* InCharacterMesh,
	UStaticMeshComponent* InSwordMesh,
	UStaticMeshComponent* InSheathMesh,
	UStaticMeshComponent* InShieldMesh)
{
	CharacterMesh = InCharacterMesh;
	SwordMesh = InSwordMesh;
	SheathMesh = InSheathMesh;
	ShieldMesh = InShieldMesh;
	WeaponState = EZCWeaponState::Sheathed;
	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
}

EZCWeaponCommand UZCCombatComponent::ResolveAttackCommand(const EZCWeaponState State)
{
	switch (State)
	{
	case EZCWeaponState::Sheathed:
		return EZCWeaponCommand::Draw;
	case EZCWeaponState::Equipped:
		return EZCWeaponCommand::Attack;
	default:
		return EZCWeaponCommand::None;
	}
}

bool UZCCombatComponent::HandleAttackInput()
{
	switch (ResolveAttackCommand(WeaponState))
	{
	case EZCWeaponCommand::Draw:
		return StartDraw();
	case EZCWeaponCommand::Attack:
		return StartWeaponAttack();
	default:
		return false;
	}
}

bool UZCCombatComponent::StartDraw()
{
	ClearAutoSheathTimer();
	WeaponState = EZCWeaponState::Drawing;
	if (PlayMontage(DrawSwordMontage, &UZCCombatComponent::HandleDrawMontageEnded))
	{
		return true;
	}

	WeaponState = EZCWeaponState::Sheathed;
	return false;
}

bool UZCCombatComponent::StartWeaponAttack()
{
	ClearAutoSheathTimer();
	WeaponState = EZCWeaponState::Attacking;
	StartAttack();
	if (PlayMontage(AttackMontage, &UZCCombatComponent::HandleAttackMontageEnded))
	{
		return true;
	}

	FinishAttack();
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
	return false;
}

bool UZCCombatComponent::RequestSheath()
{
	if (WeaponState != EZCWeaponState::Equipped)
	{
		return false;
	}

	ClearAutoSheathTimer();
	WeaponState = EZCWeaponState::Sheathing;
	if (PlayMontage(SheathSwordMontage, &UZCCombatComponent::HandleSheathMontageEnded))
	{
		return true;
	}

	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
	return false;
}

bool UZCCombatComponent::PlayMontage(
	UAnimMontage* Montage,
	void (UZCCombatComponent::*EndCallback)(UAnimMontage*, bool))
{
	if (!CharacterMesh || !Montage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (!AnimInstance || AnimInstance->Montage_Play(Montage) <= 0.0f)
	{
		return false;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, EndCallback);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void UZCCombatComponent::HandleDrawMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (WeaponState != EZCWeaponState::Drawing)
	{
		return;
	}

	if (bInterrupted)
	{
		SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
		WeaponState = EZCWeaponState::Sheathed;
		return;
	}

	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
}

void UZCCombatComponent::HandleAttackMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (WeaponState != EZCWeaponState::Attacking)
	{
		return;
	}

	FinishAttack();
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
}

void UZCCombatComponent::HandleSheathMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (WeaponState != EZCWeaponState::Sheathing)
	{
		return;
	}

	if (bInterrupted)
	{
		SetEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
		WeaponState = EZCWeaponState::Equipped;
		ScheduleAutoSheath();
		return;
	}

	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
	WeaponState = EZCWeaponState::Sheathed;
}

void UZCCombatComponent::SetEquipmentAttachmentState(const EZCWeaponAttachmentState AttachmentState)
{
	if (!CharacterMesh)
	{
		return;
	}

	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	if (SheathMesh)
	{
		SheathMesh->AttachToComponent(CharacterMesh, AttachmentRules, WeaponSheathSocket);
	}

	if (AttachmentState == EZCWeaponAttachmentState::Equipped)
	{
		if (SwordMesh)
		{
			SwordMesh->AttachToComponent(CharacterMesh, AttachmentRules, WeaponHandSocket);
		}
		if (ShieldMesh)
		{
			ShieldMesh->AttachToComponent(CharacterMesh, AttachmentRules, ShieldHandSocket);
		}
		return;
	}

	if (SwordMesh)
	{
		SwordMesh->AttachToComponent(CharacterMesh, AttachmentRules, WeaponSheathSocket);
	}
	if (ShieldMesh)
	{
		ShieldMesh->AttachToComponent(CharacterMesh, AttachmentRules, ShieldBackSocket);
	}
}

bool UZCCombatComponent::IsWeaponEquippedForAnimation() const
{
	return WeaponState == EZCWeaponState::Equipped
		|| WeaponState == EZCWeaponState::Attacking
		|| WeaponState == EZCWeaponState::Sheathing;
}

void UZCCombatComponent::ScheduleAutoSheath()
{
	if (WeaponState != EZCWeaponState::Equipped || AutoSheathDelay <= 0.0f || !GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AutoSheathTimerHandle,
		this,
		&UZCCombatComponent::HandleAutoSheathElapsed,
		AutoSheathDelay,
		false);
}

void UZCCombatComponent::ClearAutoSheathTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoSheathTimerHandle);
	}
}

void UZCCombatComponent::HandleAutoSheathElapsed()
{
	if (WeaponState == EZCWeaponState::Equipped)
	{
		RequestSheath();
	}
}

void UZCCombatComponent::FinishAttack()
{
	bAttackActive = false;
	bTraceActive = false;
}

void UZCCombatComponent::StartAttack()
{
	bAttackActive = true;
	bTraceActive = false;
	HitActors.Reset();
}

bool UZCCombatComponent::BeginTrace()
{
	if (!bAttackActive)
	{
		return false;
	}

	bTraceActive = true;
	return true;
}

void UZCCombatComponent::EndTrace()
{
	bTraceActive = false;
}

bool UZCCombatComponent::TryApplyHit(AActor* Target, const float DamageAmount)
{
	if (!bAttackActive || !bTraceActive || !IsValid(Target) || DamageAmount <= 0.0f)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (Owner && Target == Owner)
	{
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (HitActors.Contains(TargetKey))
	{
		return false;
	}

	HitActors.Add(TargetKey);
	AController* InstigatorController = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	UGameplayStatics::ApplyDamage(Target, DamageAmount, InstigatorController, Owner, UDamageType::StaticClass());
	return true;
}
