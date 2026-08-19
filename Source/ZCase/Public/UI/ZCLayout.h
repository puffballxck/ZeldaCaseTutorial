// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZCLayout.generated.h"

class AZCCharBase;
class UWidgetSwitcher;

/**
 * Root player layout. Visual composition stays in Blueprint; presentation
 * state changes enter through semantic functions instead of reaching into the
 * Blueprint's WidgetSwitcher from gameplay code.
 */
UCLASS()
class ZCASE_API UZCLayout : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Existing Blueprint initialization hook retained for asset compatibility. */
	UFUNCTION(BlueprintImplementableEvent)
	void ConstructDeferred(AZCCharBase* PlayerRef);

	/** Existing Blueprint stamina animation hook retained for asset compatibility. */
	UFUNCTION(BlueprintImplementableEvent)
	void ShowGaugeAnim(bool bShow);

	/**
	 * Switches between gameplay (index 0) and rune selection (index 1).
	 * Blueprints may override this, while the native fallback works with the
	 * first WidgetSwitcher found in the existing WidgetTree.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "ZCase|Player Presentation")
	void SetRuneMenuOpen(bool bOpen);
	virtual void SetRuneMenuOpen_Implementation(bool bOpen);

protected:
	/** Preferred explicit binding for the existing UI_Layout switcher. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher;
	
};
