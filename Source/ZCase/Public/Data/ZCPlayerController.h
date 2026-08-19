// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ZCPlayerController.generated.h"

class AZCCharBase;
class UZCLayout;

/**
 * Owns the local player's presentation state and root HUD lifecycle.
 *
 * RootLayoutClass is now the preferred composition point. The character's
 * legacy LayoutClassRef remains a fallback so existing BP_Player assets keep
 * working while their defaults are migrated.
 */
UCLASS()
class ZCASE_API AZCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Creates or adopts the possessed player's root layout. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void InitializePlayerPresentation();

	/** Opens or closes the rune selection menu and applies the complete input policy. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void SetRuneMenuOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void ToggleRuneMenu();

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	bool IsRuneMenuOpen() const { return bRuneMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	UZCLayout* GetRootLayout() const { return RootLayout; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	void RequestPresentationInitialization();
	void HandleDeferredPresentationInitialization();
	void ApplyRuneMenuPolicy();
	void ReleasePlayerPresentation(AZCCharBase* PreviousPlayer);

	/** Preferred root layout class. Legacy BP_Player configuration is a fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation")
	TSubclassOf<UZCLayout> RootLayoutClass;

	/** The one root widget associated with the currently possessed local player. */
	UPROPERTY(Transient)
	TObjectPtr<UZCLayout> RootLayout;

	/** Character whose legacy LayoutRef currently backs RootLayout. */
	UPROPERTY(Transient)
	TObjectPtr<AZCCharBase> PresentedPlayer;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZCase|Player Presentation", meta = (AllowPrivateAccess = "true"))
	bool bRuneMenuOpen = false;

	/** True only when this controller was responsible for pausing the game. */
	bool bPausedByRuneMenu = false;

	bool bPresentationInitializationPending = false;
	FTimerHandle PresentationInitializationTimer;
	
};
