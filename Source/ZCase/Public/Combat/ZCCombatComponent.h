// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ZCCombatComponent.generated.h"

/** Owns the lifetime and hit de-duplication rules for one attack at a time. */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCCombatComponent();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void StartAttack();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool BeginTrace();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void EndTrace();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool TryApplyHit(AActor* Target, float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	bool IsTraceActive() const { return bTraceActive; }

private:
	bool bAttackActive = false;
	bool bTraceActive = false;
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
