// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZCBokoblinAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Characters/ZCBokoblinEnemy.h"
#include "Characters/ZCCharBase.h"
#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

namespace
{
	const FName TargetActorKeyName(TEXT("TargetActor"));
}

AZCBokoblinAIController::AZCBokoblinAIController()
{
	PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightRadius = 1800.0f;
	LoseSightRadius = 2200.0f;
	PeripheralVisionAngleDegrees = 70.0f;

	if (SightConfig)
	{
		SightConfig->SightRadius = SightRadius;
		SightConfig->LoseSightRadius = LoseSightRadius;
		SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngleDegrees;
		SightConfig->DetectionByAffiliation.bDetectEnemies = true;
		SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
		SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	}

	if (PerceptionComponent && SightConfig)
	{
		PerceptionComponent->ConfigureSense(*SightConfig);
		PerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
	}
}

void AZCBokoblinAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (bPermanentlyStopped)
	{
		return;
	}

	ControlledEnemy = Cast<AZCBokoblinEnemy>(InPawn);
	if (!ControlledEnemy.IsValid())
	{
		StopAI();
		return;
	}

	bAIStopped = false;
	AZCBokoblinEnemy* Enemy = ControlledEnemy.Get();
	HomeLocation = Enemy->GetActorLocation();
	bReturningHome = false;
	GetWorldTimerManager().SetTimer(HomeReturnTimer, this,
		&AZCBokoblinAIController::UpdateHomeReturn, 0.2f, true);
	if (SightConfig && PerceptionComponent)
	{
		// Reapply CDO/Blueprint overrides when a controller instance is possessed.
		SightConfig->SightRadius = FMath::Max(SightRadius, 0.0f);
		SightConfig->LoseSightRadius = FMath::Max(LoseSightRadius, SightConfig->SightRadius);
		SightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(
			PeripheralVisionAngleDegrees,
			0.0f,
			180.0f);
		PerceptionComponent->ConfigureSense(*SightConfig);
	}
	if (Enemy->Attributes)
	{
		Enemy->Attributes->OnDeath.AddUniqueDynamic(this, &AZCBokoblinAIController::HandleEnemyDeath);
		if (Enemy->Attributes->IsDead())
		{
			HandleEnemyDeath(Enemy);
			return;
		}
	}

	SetPatrolMovement();
	if (Enemy->BehaviorTree)
	{
		RunBehaviorTree(Enemy->BehaviorTree);
	}

	if (PerceptionComponent)
	{
		PerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
		PerceptionComponent->OnTargetPerceptionUpdated.AddUniqueDynamic(
			this,
			&AZCBokoblinAIController::HandleTargetPerceptionUpdated);
		PerceptionComponent->SetActive(true);

		// Sight may already know about a player before OnPossess binds the
		// callback. Replay currently perceived actors after the blackboard exists.
		TArray<AActor*> PerceivedActors;
		PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
		for (AActor* PerceivedActor : PerceivedActors)
		{
			if (IsLivingPlayerTarget(PerceivedActor))
			{
				HandleTargetAcquired(PerceivedActor);
				break;
			}
		}
	}
}

void AZCBokoblinAIController::OnUnPossess()
{
	bPermanentlyStopped = true;
	StopAI();
	ControlledEnemy.Reset();
	Super::OnUnPossess();
}

void AZCBokoblinAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HomeReturnTimer);
	Super::EndPlay(EndPlayReason);
}

void AZCBokoblinAIController::HandleTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
	if (bPermanentlyStopped || bAIStopped || bReturningHome || !IsValid(Actor))
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed() && IsLivingPlayerTarget(Actor))
	{
		HandleTargetAcquired(Actor);
		return;
	}

	if (CurrentTarget.Get() == Actor)
	{
		ClearTarget();
	}
}

void AZCBokoblinAIController::HandleEnemyDeath(AActor* DeadActor)
{
	if (DeadActor && ControlledEnemy.IsValid() && DeadActor != ControlledEnemy.Get())
	{
		return;
	}

	bPermanentlyStopped = true;
	StopAI();
}

void AZCBokoblinAIController::HandleTargetDeath(AActor* DeadActor)
{
	if (CurrentTarget.Get() == DeadActor)
	{
		ClearTarget();
	}
}

void AZCBokoblinAIController::HandleTargetDestroyed(AActor* DestroyedActor)
{
	if (CurrentTarget.Get() == DestroyedActor)
	{
		ClearTarget();
	}
}

bool AZCBokoblinAIController::IsLivingPlayerTarget(const AActor* Actor) const
{
	const APawn* TargetPawn = Cast<APawn>(Actor);
	if (!TargetPawn || !IsValid(TargetPawn) || !TargetPawn->IsPlayerControlled())
	{
		return false;
	}

	if (const UZCAttributeComponent* Attributes = TargetPawn->FindComponentByClass<UZCAttributeComponent>())
	{
		if (Attributes->IsDead())
		{
			return false;
		}
	}
	if (const AZCCharBase* Player = Cast<AZCCharBase>(TargetPawn))
	{
		return !Player->IsDeathStarted();
	}
	return true;
}

void AZCBokoblinAIController::BindTargetLifecycle(AActor* Target)
{
	UnbindTargetLifecycle();
	if (!Target)
	{
		return;
	}

	CurrentTarget = Target;
	Target->OnDestroyed.AddUniqueDynamic(this, &AZCBokoblinAIController::HandleTargetDestroyed);
	CurrentTargetAttributes = Target->FindComponentByClass<UZCAttributeComponent>();
	if (UZCAttributeComponent* Attributes = CurrentTargetAttributes.Get())
	{
		Attributes->OnDeath.AddUniqueDynamic(this, &AZCBokoblinAIController::HandleTargetDeath);
	}
}

void AZCBokoblinAIController::UnbindTargetLifecycle()
{
	if (AActor* Target = CurrentTarget.Get())
	{
		Target->OnDestroyed.RemoveDynamic(this, &AZCBokoblinAIController::HandleTargetDestroyed);
	}
	if (UZCAttributeComponent* Attributes = CurrentTargetAttributes.Get())
	{
		Attributes->OnDeath.RemoveDynamic(this, &AZCBokoblinAIController::HandleTargetDeath);
	}
	CurrentTargetAttributes.Reset();
	CurrentTarget.Reset();
}

void AZCBokoblinAIController::HandleTargetAcquired(AActor* Target)
{
	if (bPermanentlyStopped || bAIStopped || bReturningHome || !IsLivingPlayerTarget(Target))
	{
		return;
	}

	if (CurrentTarget.Get() != Target)
	{
		BindTargetLifecycle(Target);
	}

	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsObject(TargetActorKeyName, Target);
	}
	SetChaseMovement();
	SetFocus(Target, EAIFocusPriority::Gameplay);
}

void AZCBokoblinAIController::ClearTarget()
{
	UnbindTargetLifecycle();
	if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
	{
		BlackboardComp->SetValueAsObject(TargetActorKeyName, nullptr);
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	SetPatrolMovement();
}

void AZCBokoblinAIController::SetPatrolMovement()
{
	AZCBokoblinEnemy* Enemy = ControlledEnemy.Get();
	if (!Enemy)
	{
		return;
	}

	StopMovement();
	Enemy->bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->bUseControllerDesiredRotation = false;
		Movement->bOrientRotationToMovement = true;
		Movement->MaxWalkSpeed = FMath::Max(Enemy->PatrolSpeed, 0.0f);
	}
}

void AZCBokoblinAIController::SetChaseMovement()
{
	AZCBokoblinEnemy* Enemy = ControlledEnemy.Get();
	if (!Enemy)
	{
		return;
	}

	Enemy->bUseControllerRotationYaw = true;
	if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
	{
		Movement->bUseControllerDesiredRotation = true;
		Movement->bOrientRotationToMovement = false;
		Movement->MaxWalkSpeed = FMath::Max(Enemy->ChaseSpeed, 0.0f);
	}
}

bool AZCBokoblinAIController::HasReachedHome() const
{
	const AZCBokoblinEnemy* Enemy = ControlledEnemy.Get();
	if (!Enemy || !Enemy->GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}
	const float MaxDistance = FMath::IsFinite(Enemy->MaxChaseDistance)
		? FMath::Max(Enemy->MaxChaseDistance, 100.0f) : 2000.0f;
	const float Radius = FMath::IsFinite(Enemy->ReturnAcceptanceRadius)
		? FMath::Clamp(Enemy->ReturnAcceptanceRadius, 1.0f, MaxDistance * 0.5f) : 75.0f;
	const FVector Offset = Enemy->GetActorLocation() - HomeLocation;
	// 水平接近不能代表到家：避免在楼上、楼下或下落途中恢复索敌。
	return Offset.SizeSquared2D() <= FMath::Square(Radius)
		&& FMath::Abs(Offset.Z) <= 75.0f;
}

void AZCBokoblinAIController::UpdateHomeReturn()
{
	AZCBokoblinEnemy* Enemy = ControlledEnemy.Get();
	if (bPermanentlyStopped || bAIStopped || !Enemy)
	{
		return;
	}
	if (!Enemy->CanBeTargetLocked())
	{
		HandleEnemyDeath(Enemy);
		return;
	}
	if (!bReturningHome)
	{
		const float MaxDistance = FMath::IsFinite(Enemy->MaxChaseDistance)
			? FMath::Max(Enemy->MaxChaseDistance, 100.0f) : 2000.0f;
		if (FVector::DistSquared2D(Enemy->GetActorLocation(), HomeLocation) > FMath::Square(MaxDistance))
		{
			BeginHomeReturn();
		}
		return;
	}

	// 受击沿用原有 Combat 暂停状态；结束后重新请求路径，不改变生命值或给予无敌。
	if (Enemy->Combat && !Enemy->Combat->CanAcceptCombatInput())
	{
		StopMovement();
		return;
	}
	if (HasReachedHome())
	{
		FinishHomeReturn();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextReturnAttemptTime || GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	// 仅在请求结束/失败后重试，避免每次定时检查都重新寻路。不可达时保持返程态。
	NextReturnAttemptTime = Now + 1.0;
	FAIMoveRequest Request(HomeLocation);
	Request.SetAcceptanceRadius(1.0f);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetReachTestIncludesGoalRadius(false);
	Request.SetAllowPartialPath(false);
	Request.SetUsePathfinding(true);
	Request.SetProjectGoalLocation(true);
	Request.SetCanStrafe(false);
	MoveTo(Request);
}

void AZCBokoblinAIController::BeginHomeReturn()
{
	// 先锁住返程态，再停止旧任务，避免同步 Abort/感知回调重新获取玩家。
	bReturningHome = true;
	if (UBehaviorTreeComponent* Tree = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
	{
		Tree->StopTree(EBTStopMode::Forced);
	}
	else if (UBrainComponent* Brain = GetBrainComponent())
	{
		Brain->StopLogic(TEXT("Bokoblin returning home"));
	}
	ClearTarget();
	if (AZCBokoblinEnemy* Enemy = ControlledEnemy.Get())
	{
		if (Enemy->Combat)
		{
			Enemy->Combat->CancelAttack();
		}
		// 清除 Focus 后按移动方向转身，使用追击速度跑回出生点。
		Enemy->GetCharacterMovement()->MaxWalkSpeed = FMath::Max(Enemy->ChaseSpeed, 0.0f);
	}
	NextReturnAttemptTime = 0.0;
	UpdateHomeReturn();
}

void AZCBokoblinAIController::AcquireVisiblePlayer()
{
	if (!PerceptionComponent)
	{
		return;
	}
	TArray<AActor*> PerceivedActors;
	PerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);
	for (AActor* Actor : PerceivedActors)
	{
		if (IsLivingPlayerTarget(Actor))
		{
			HandleTargetAcquired(Actor);
			break;
		}
	}
}

void AZCBokoblinAIController::FinishHomeReturn()
{
	if (bPermanentlyStopped || bAIStopped || !ControlledEnemy.IsValid())
	{
		return;
	}
	StopMovement();
	bReturningHome = false;
	SetPatrolMovement();
	if (ControlledEnemy->BehaviorTree)
	{
		RunBehaviorTree(ControlledEnemy->BehaviorTree);
	}
	// 玩家可能始终在视野内，不会再产生“刚发现”事件，因此主动重查感知结果。
	AcquireVisiblePlayer();
}

void AZCBokoblinAIController::StopAI()
{
	GetWorldTimerManager().ClearTimer(HomeReturnTimer);
	bReturningHome = false;
	if (!bAIStopped)
	{
		bAIStopped = true;
		if (PerceptionComponent)
		{
			PerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
				this,
				&AZCBokoblinAIController::HandleTargetPerceptionUpdated);
			PerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
			PerceptionComponent->ForgetAll();
			PerceptionComponent->SetActive(false);
		}
		if (UBrainComponent* Brain = GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Bokoblin AI stopped"));
		}
	}

	ClearTarget();
	if (AZCBokoblinEnemy* Enemy = ControlledEnemy.Get())
	{
		if (Enemy->Attributes)
		{
			Enemy->Attributes->OnDeath.RemoveDynamic(this, &AZCBokoblinAIController::HandleEnemyDeath);
		}
	}
	StopMovement();
}
