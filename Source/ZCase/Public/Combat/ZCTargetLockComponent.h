// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ZCTargetLockComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCTargetChangedSignature,
	AActor*, PreviousTarget,
	AActor*, CurrentTarget);

/** Owns the current target and guarantees that it remains targetable. */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCTargetLockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCTargetLockComponent();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	bool SetTarget(AActor* Candidate);

	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	void ClearTarget();

	UFUNCTION(BlueprintPure, Category = "ZCase|Target Lock")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "ZCase|Target Lock")
	bool HasTarget() const { return CurrentTarget.IsValid(); }

	UPROPERTY(BlueprintAssignable, Category = "ZCase|Target Lock")
	FZCTargetChangedSignature OnTargetChanged;

protected:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleTargetDestroyed(AActor* DestroyedActor);

	bool IsValidTarget(const AActor* Candidate) const;
	void ReplaceTarget(AActor* NewTarget);

	TWeakObjectPtr<AActor> CurrentTarget;
};
