// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/InteractBase.h"
#include "PickupActor.generated.h"

UCLASS()
class ZCASE_API APickupActor : public AInteractBase
{
	GENERATED_BODY()

public:
	APickupActor();

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> HintClass;



	UPROPERTY()
	UUserWidget* HintUI;

	FTimerHandle DelayRestSphereHandle;

protected:
	virtual void PlayerEnterCpp() override;
	virtual void PlayerLeftCpp() override;

	virtual void ToggleInteraction(AZCCharBase* playerRef) override;
	virtual void NextActionInInteractionActor() override;

public:
	void RemoveHintAndSetSphereInvalid();
	void ResetSphereAfterThrowing();
	void ThrowObject(const FVector& ThrowForce);

	virtual void NextAction() override;

private:
	void ResetSphere();
	
};
