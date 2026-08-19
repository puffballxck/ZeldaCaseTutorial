// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/MyInterface.h"
#include "InteractBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AZCCharBase;
UCLASS()
class ZCASE_API AInteractBase : public AActor, public IMyInterface
{
	GENERATED_BODY()
	
public:	
	AInteractBase();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* BaseMesh;

	UPROPERTY(editAnywhere, BlueprintReadWrite)
	USphereComponent* InteractSphere;

	UPROPERTY()
	AZCCharBase* PlayerRef;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent)
	void PlayerEntered();

	virtual void PlayerEnterCpp();

	UFUNCTION(BlueprintImplementableEvent)
	void PlayerLeft();

	virtual void PlayerLeftCpp();
	
    
	virtual void ToggleInteraction(AZCCharBase* PlayerRef);

	UFUNCTION(BlueprintImplementableEvent)
	void ToggleInteractionBP(AZCCharBase* Player);
	
	virtual void NextActionInInteractionActor();

	virtual void NextAction() override;
	
};
