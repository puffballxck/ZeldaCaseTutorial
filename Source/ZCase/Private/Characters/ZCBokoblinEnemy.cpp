// 版权所有 Epic Games, Inc，保留所有权利

#include "Characters/ZCBokoblinEnemy.h"

#include "AI/ZCBokoblinAIController.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
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
	static ConstructorHelpers::FObjectFinder<UAnimMontage> KnockdownFinder(
		TEXT("/Game/_Game/Animations/Enemy/Bokoblin/Animation/Montage/AM_Bokoblin_Death.AM_Bokoblin_Death"));
	ParryKnockdownMontage = KnockdownFinder.Object;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> GetupFinder(
		TEXT("/Game/_Game/Animations/Enemy/Bokoblin/Animation/Montage/AM_Down_Getup.AM_Down_Getup"));
	ParryGetupMontage = GetupFinder.Object;
	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		// 由骨骼驱动的 Trace 即使敌人离屏也必须持续更新
		EnemyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// Bokoblin 的徒手 Trace 只能伤害玩家，基类 Combat
	// 默认值保持为 false，以保留现有玩家攻击行为
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

	// Bokoblin 没有武器网格，复用角色骨骼作为
	// Trace 来源，并让 Combat 将这些名称解析为 Socket 或骨骼端点
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
	if (bParryRecovering) return false;
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

void AZCBokoblinEnemy::HandleAttackParried()
{
	if (bParryRecovering || !CanBeTargetLocked() || !Combat
		|| !Combat->CanAcceptCombatInput() || !Combat->IsAttackActive())
	{
		return;
	}

	// 先暂停 AI，再关闭攻击：OnAttackEnded 会同步完成行为树攻击任务
	bParryRecovering = true;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent(); Brain && !Brain->IsPaused())
		{
			ParryPausedBrain = Brain;
			Brain->PauseLogic(TEXT("Bokoblin attack parried"));
		}
		AI->StopMovement();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		PreParryMovementMode = Movement->MovementMode;
		PreParryCustomMovementMode = Movement->CustomMovementMode;
		ConsumeMovementInputVector();
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	if (!Combat->InterruptForHitReaction() || !CanBeTargetLocked())
	{
		FinishParryRecovery(CanBeTargetLocked());
		return;
	}
	StartParryRecoveryStage(false);
}

void AZCBokoblinEnemy::StartParryRecoveryStage(const bool bGetup)
{
	if (!bParryRecovering || !CanBeTargetLocked())
	{
		FinishParryRecovery(false);
		return;
	}
	GetWorldTimerManager().ClearTimer(ParryRecoveryTimer);
	ParryStage = bGetup ? EZCBokoblinParryStage::GettingUp : EZCBokoblinParryStage::Knockdown;
	bParryDownPose = bGetup;
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = bGetup ? ParryGetupMontage.Get() : ParryKnockdownMontage.Get();
	if (Anim && ActiveParryMontage)
	{
		Anim->Montage_Stop(0.05f, ActiveParryMontage.Get());
	}
	ActiveParryMontage = Montage;
	const float Duration = Anim && Montage ? Anim->Montage_Play(Montage) : 0.0f;
	if (!FMath::IsFinite(Duration) || Duration <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s cannot play parry %s montage."),
			*GetName(), bGetup ? TEXT("getup") : TEXT("knockdown"));
		if (bGetup) FinishParryRecovery(true);
		else EnterParryDownWait();
		return;
	}
	// Death 蒙太奇可能关闭自动淡出并停在末帧；观察播放位置衔接起身，
	// 不修改共享资产超时同时兜底循环 Section 或外部暂停
	ParryStageDeadline = GetWorld()->GetTimeSeconds()
		+ Duration / FMath::Max(FMath::Abs(Montage->RateScale), 0.01f) + 1.0;
	GetWorldTimerManager().SetTimer(ParryRecoveryTimer, this,
		&AZCBokoblinEnemy::UpdateParryRecovery, 0.02f, true);
}

void AZCBokoblinEnemy::EnterParryDownWait()
{
	if (!bParryRecovering || !CanBeTargetLocked())
	{
		FinishParryRecovery(false);
		return;
	}
	ParryStage = EZCBokoblinParryStage::DownWait;
	bParryDownPose = true;
	// 先切换底层姿态再淡出倒地 Montage，关闭自动淡出的死亡资产也能退出
	if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		Anim && ActiveParryMontage)
	{
		Anim->Montage_Stop(0.12f, ActiveParryMontage.Get());
	}
	ActiveParryMontage = nullptr;
	const float WaitDuration = FMath::IsFinite(ParryDownWaitDuration)
		? FMath::Max(ParryDownWaitDuration, 0.0f) : 0.6f;
	ParryStageDeadline = GetWorld()->GetTimeSeconds() + WaitDuration;
	GetWorldTimerManager().SetTimer(ParryRecoveryTimer, this,
		&AZCBokoblinEnemy::UpdateParryRecovery, 0.02f, true);
}

void AZCBokoblinEnemy::UpdateParryRecovery()
{
	if (!bParryRecovering || !CanBeTargetLocked())
	{
		FinishParryRecovery(false);
		return;
	}
	if (ParryStage == EZCBokoblinParryStage::DownWait)
	{
		if (GetWorld()->GetTimeSeconds() >= ParryStageDeadline)
		{
			StartParryRecoveryStage(true);
		}
		return;
	}
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !ActiveParryMontage)
	{
		FinishParryRecovery(true);
		return;
	}
	const float Position = Anim->Montage_GetPosition(ActiveParryMontage.Get());
	const float Length = ActiveParryMontage->GetPlayLength();
	// 蒙太奇开始淡出前准备好基础姿态：倒地接躺姿，起身接站姿
	// 起身结束仍以 Down_Wait 为底层会再次向躺姿混合，造成“又倒了一次”
	const float BlendLead = FMath::Max(ActiveParryMontage->BlendOut.GetBlendTime(),
		ActiveParryMontage->BlendOutTriggerTime) * FMath::Max(FMath::Abs(ActiveParryMontage->RateScale), 0.01f);
	if (Position >= FMath::Max(0.0f, Length - BlendLead - 0.06f))
	{
		bParryDownPose = ParryStage == EZCBokoblinParryStage::Knockdown;
	}
	const bool bStageFinished = !Anim->Montage_IsPlaying(ActiveParryMontage.Get())
		|| Position >= Length - 0.02f
		|| GetWorld()->GetTimeSeconds() >= ParryStageDeadline;
	if (bStageFinished)
	{
		if (ParryStage == EZCBokoblinParryStage::GettingUp) FinishParryRecovery(true);
		else EnterParryDownWait();
	}
}

void AZCBokoblinEnemy::FinishParryRecovery(const bool bRestoreControl)
{
	GetWorldTimerManager().ClearTimer(ParryRecoveryTimer);
	if (!bParryRecovering) return;
	bParryRecovering = false;
	ParryStage = EZCBokoblinParryStage::None;
	bParryDownPose = false;
	if (bRestoreControl && CanBeTargetLocked())
	{
		if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
			Anim && ActiveParryMontage)
		{
			Anim->Montage_Stop(0.1f, ActiveParryMontage.Get());
		}
		if (UCharacterMovementComponent* Movement = GetCharacterMovement();
			Movement && Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(PreParryMovementMode, PreParryCustomMovementMode);
		}
		if (Combat) Combat->ResumeAfterHitReaction();
		const AAIController* AI = Cast<AAIController>(GetController());
		const AZCBokoblinAIController* BokoblinAI = Cast<AZCBokoblinAIController>(AI);
		if (UBrainComponent* Brain = ParryPausedBrain.Get(); Brain && AI
			&& AI->GetBrainComponent() == Brain && Brain->IsPaused()
			&& (!BokoblinAI || !BokoblinAI->IsReturningHome()))
		{
			Brain->ResumeLogic(TEXT("Bokoblin parry getup ended"));
		}
	}
	ActiveParryMontage = nullptr;
	ParryPausedBrain.Reset();
}

void AZCBokoblinEnemy::PlayHitReact()
{
	// 倒地/起身期间仍然扣血，但普通受击不能覆盖整套恢复动作
	if (!bParryRecovering) Super::PlayHitReact();
}

void AZCBokoblinEnemy::HandleDeath(AActor* DeadActor)
{
	if (DeadActor && DeadActor != this) return;
	FinishParryRecovery(false);
	Super::HandleDeath(DeadActor);
}

void AZCBokoblinEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FinishParryRecovery(false);
	Super::EndPlay(EndPlayReason);
}
