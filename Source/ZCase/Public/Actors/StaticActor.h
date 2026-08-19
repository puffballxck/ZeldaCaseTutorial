// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StaticActor.generated.h"

class UArrowComponent;
UCLASS()
class ZCASE_API AStaticActor : public AActor
{
	GENERATED_BODY()

public:	
	AStaticActor();

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UArrowComponent* IndicatorArrow;

	int32 Impulse = 0;
	int32 Hits = 0;//施加力的次数

	FVector GteImpulse();

	void UpdateForceInfo();

	FORCEINLINE  UArrowComponent* GetArrowComponent() const{ return IndicatorArrow; }
	
};
