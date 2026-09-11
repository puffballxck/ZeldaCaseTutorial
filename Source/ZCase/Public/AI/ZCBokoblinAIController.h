// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AIController.h"
#include "Engine/TimerHandle.h"
#include "Perception/AIPerceptionTypes.h"
#include "ZCBokoblinAIController.generated.h"

class AZCBokoblinEnemy;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UZCAttributeComponent;

/**
 * Perception and behavior-tree owner for a Bokoblin.
 *
 * The controller keeps the blackboard contract intentionally small: the
 * runtime writes TargetActor and the patrol task writes PatrolLocation.
 */
UCLASS()
class ZCASE_API AZCBokoblinAIController : public AAIController
{
	GENERATED_BODY()

public:
	AZCBokoblinAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "ZCase|Bokoblin|AI")
	bool IsReturningHome() const { return bReturningHome; }

	/** Sight callback that acquires or releases a living player-controlled Pawn. */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Tuned sight configuration used by the controller's UAIPerceptionComponent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (ClampMin = "0.0"))
	float SightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (ClampMin = "0.0"))
	float LoseSightRadius = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float PeripheralVisionAngleDegrees = 70.0f;

private:
	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor);

	UFUNCTION()
	void HandleTargetDeath(AActor* DeadActor);

	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	bool IsLivingPlayerTarget(const AActor* Actor) const;
	void BindTargetLifecycle(AActor* Target);
	void UnbindTargetLifecycle();
	void HandleTargetAcquired(AActor* Target);
	void ClearTarget();
	void SetPatrolMovement();
	void SetChaseMovement();
	void StopAI();
	void UpdateHomeReturn();
	void BeginHomeReturn();
	void FinishHomeReturn();
	void AcquireVisiblePlayer();
	bool HasReachedHome() const;

	/** 每次接管时记录起点，返程和重新索敌不会更新它。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI", meta = (AllowPrivateAccess = "true"))
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI", meta = (AllowPrivateAccess = "true"))
	bool bReturningHome = false;

	FTimerHandle HomeReturnTimer;
	double NextReturnAttemptTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	TWeakObjectPtr<AZCBokoblinEnemy> ControlledEnemy;
	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<UZCAttributeComponent> CurrentTargetAttributes;

	/** Once death or unpossess stops this controller, a later callback cannot restart its tree. */
	bool bPermanentlyStopped = false;
	bool bAIStopped = false;
};
