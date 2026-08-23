// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ZCAttributeComponent.generated.h"

USTRUCT(BlueprintType)
struct ZCASE_API FZCDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	float HealthBefore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	float HealthAfter = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	float AppliedDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Attributes")
	bool bBecameDead = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCHealthChangedSignature,
	float, PreviousHealth,
	float, CurrentHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FZCDeathSignature,
	AActor*, DeadActor);

/** Owns health clamping and the one-way transition into death. */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCAttributeComponent();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Attributes")
	void InitializeAttributes(float InMaxHealth, float InCurrentHealth);

	UFUNCTION(BlueprintCallable, Category = "ZCase|Attributes")
	FZCDamageResult ApplyDamage(float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "ZCase|Attributes")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Attributes")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Attributes")
	bool IsDead() const { return CurrentHealth <= 0.0f; }

	UPROPERTY(BlueprintAssignable, Category = "ZCase|Attributes")
	FZCHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "ZCase|Attributes")
	FZCDeathSignature OnDeath;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZCase|Attributes", meta = (ClampMin = "1.0"))
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ZCase|Attributes", meta = (ClampMin = "0.0"))
	float DefaultHealth = 100.0f;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Attributes")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Attributes")
	float CurrentHealth = 100.0f;

	bool bDeathBroadcast = false;
};
