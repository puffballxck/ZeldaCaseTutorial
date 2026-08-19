// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressureSwitch.generated.h"

class USceneComponent;
class UBoxComponent;
class UStaticMeshComponent;
UCLASS()
class ZCASE_API APressureSwitch : public AActor
{
	GENERATED_BODY()
	
public:	
	APressureSwitch();

	UPROPERTY()
	USceneComponent* SceneRoot;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UBoxComponent* BoxCollider;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UStaticMeshComponent* Switcher;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UStaticMeshComponent* Base;
	
};
