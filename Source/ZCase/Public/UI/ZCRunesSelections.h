// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "ZCRunesSelections.generated.h"

class AZCCharBase;
UCLASS()
class ZCASE_API UZCRunesSelections : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	AZCCharBase* PlayerRef;

	UFUNCTION(BlueprintCallable,category="Test")
	void SelectRuneType(TEnumAsByte<ERunes> RuneType);
	
	
};
