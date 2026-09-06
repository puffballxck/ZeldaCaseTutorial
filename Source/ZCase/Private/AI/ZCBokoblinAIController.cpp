// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZCBokoblinAIController.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Characters/ZCBokoblinEnemy.h"
#include "Characters/ZCCharBase.h"
#include "Combat/ZCAttributeComponent.h"
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

void AZCBokoblinAIController::HandleTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
	if (bPermanentlyStopped || bAIStopped || !IsValid(Actor))
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
	if (!IsLivingPlayerTarget(Target))
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

void AZCBokoblinAIController::StopAI()
{
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
