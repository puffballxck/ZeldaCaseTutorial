// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindTunnel.generated.h"

class AZCCharBase;
class UBoxComponent;
UCLASS()
class ZCASE_API AWindTunnel : public AActor
{
	GENERATED_BODY()
	
public:	
	AWindTunnel();
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTemporaryWT = false;

	UPROPERTY(EditAnywhere)
	AZCCharBase* PlayerRef = nullptr;

	UPROPERTY(EditAnywhere)
	UBoxComponent* Box;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult & SweepResult);
	
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


};



