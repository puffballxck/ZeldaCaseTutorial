// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Characters/ZCEnemyBase.h"
#include "ZCBokoblinEnemy.generated.h"

class AActor;
class UAnimMontage;
class UBehaviorTree;

/**
 * Bokoblin-specific combat and AI data.
 *
 * The enemy base owns damage, hit reaction, death, and the reusable Combat
 * component. This class supplies the small amount of attack policy required
 * by the Bokoblin behavior tree.
 */
UCLASS()
class ZCASE_API AZCBokoblinEnemy : public AZCEnemyBase
{
	GENERATED_BODY()

public:
	AZCBokoblinEnemy();

	/** Behavior tree started by AZCBokoblinAIController when this enemy is possessed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	/** Candidate attack montages; one valid entry is selected for each attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Animation")
	TArray<UAnimMontage*> AttackMontages;

	/** Distance at which Chase succeeds and an attack can be started, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 130.0f;

	/** Patrol movement speed, in centimeters per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Movement", meta = (ClampMin = "0.0"))
	float PatrolSpeed = 180.0f;

	/** Chase movement speed, in centimeters per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Movement", meta = (ClampMin = "0.0"))
	float ChaseSpeed = 400.0f;

	/** Start one selected attack against a living player-controlled Pawn in range. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Bokoblin|Combat")
	bool TryAttack(AActor* Target);

protected:
	virtual void ConfigureCombat() override;

	/** Punch damage and trace configuration shared by this enemy's attack montages. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace", meta = (ClampMin = "0.0"))
	float AttackDamage = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace", meta = (ClampMin = "0.0"))
	float AttackTraceRadius = 20.0f;

	/** First and last bones/socket names for the unarmed right punch trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace|Points")
	FName AttackTraceBasePoint = TEXT("Wrist_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Bokoblin|Combat|Trace|Points")
	FName AttackTraceTipPoint = TEXT("Finger_B_2_R");
};
