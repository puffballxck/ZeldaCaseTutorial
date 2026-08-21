// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "ZCRuneRuntimeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCSelectedRuneChangedSignature,
	ERunes, PreviousRune,
	ERunes, CurrentRune);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCActiveRuneChangedSignature,
	ERunes, PreviousRune,
	ERunes, CurrentRune);

/**
 * Owns rune selection and activation state.
 *
 * ActiveRune is always either R_EMAX or the currently selected rune. Callers
 * express intent through SelectRune, ToggleSelectedRune, and CancelAll rather
 * than maintaining individual activation booleans.
 */
UCLASS(ClassGroup=(ZCase), meta=(BlueprintSpawnableComponent))
class ZCASE_API UZCRuneRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCRuneRuntimeComponent();

	UFUNCTION(BlueprintPure, Category="ZCase|Runes")
	ERunes GetSelectedRune() const { return SelectedRune; }

	UFUNCTION(BlueprintPure, Category="ZCase|Runes")
	ERunes GetActiveRune() const { return ActiveRune; }

	/** Selects a rune. Selecting a different rune first cancels the active one. */
	UFUNCTION(BlueprintCallable, Category="ZCase|Runes")
	bool SelectRune(ERunes NewRune);

	/** Activates the selected rune, or cancels it when it is already active. */
	UFUNCTION(BlueprintCallable, Category="ZCase|Runes")
	bool ToggleSelectedRune();

	/** Cancels the active rune without changing the current selection. */
	UFUNCTION(BlueprintCallable, Category="ZCase|Runes")
	bool CancelAll();

	UPROPERTY(BlueprintAssignable, Category="ZCase|Runes")
	FZCSelectedRuneChangedSignature OnSelectedRuneChanged;

	UPROPERTY(BlueprintAssignable, Category="ZCase|Runes")
	FZCActiveRuneChangedSignature OnActiveRuneChanged;

private:
	void SetActiveRune(ERunes NewActiveRune);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ZCase|Runes", meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERunes> SelectedRune = ERunes::R_EMAX;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ZCase|Runes", meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERunes> ActiveRune = ERunes::R_EMAX;
};
