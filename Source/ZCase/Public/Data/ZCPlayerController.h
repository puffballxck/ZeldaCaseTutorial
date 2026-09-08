// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "ZCPlayerController.generated.h"

class AZCCharBase;
class UDataTable;
class UInputAction;
class UZCLayout;
class UZCTargetLockComponent;
class UZCTargetLockIndicatorWidget;
class UZCHeartHealthWidget;
class UZCInventoryWidget;

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

	/** Opens or closes the inventory and applies the same focus/pause policy as the rune menu. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void SetInventoryMenuOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void ToggleInventoryMenu();

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	bool IsRuneMenuOpen() const { return bRuneMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	UZCLayout* GetRootLayout() const { return RootLayout; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	bool IsInventoryMenuOpen() const { return bInventoryMenuOpen; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void SetupInputComponent() override;

private:
	void RequestPresentationInitialization();
	void HandleDeferredPresentationInitialization();
	void ApplyRuneMenuPolicy();
	void ReleasePlayerPresentation(AZCCharBase* PreviousPlayer);
	void InitializeInventoryPresentation();
	void ReleaseInventoryPresentation();
	void BindInventoryInput();

	UFUNCTION()
	void Inventory_Started(const FInputActionValue& Value);

	void InitializeTargetLockPresentation(AZCCharBase* PlayerCharacter);
	void ReleaseTargetLockPresentation(AZCCharBase* PreviousPlayer);

	UFUNCTION()
	void HandleTargetChanged(AActor* PreviousTarget, AActor* CurrentTarget);

	/** Preferred root layout class. Legacy BP_Player configuration is a fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation")
	TSubclassOf<UZCLayout> RootLayoutClass;

	/** One controller-owned indicator reused for every target switch. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation")
	TSubclassOf<UZCTargetLockIndicatorWidget> TargetLockIndicatorClass;

	/** 可选蓝图子类，用于调整心形尺寸和受伤动画；未配置时使用原生默认值。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Health")
	TSubclassOf<UZCHeartHealthWidget> HeartHealthWidgetClass;

	/** DPI 缩放前的左上角边距。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Health")
	FVector2D HeartHealthMargin = FVector2D(40.0f, 40.0f);

	/** Optional Blueprint replacement; native widget is used when unset. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory")
	TSubclassOf<UZCInventoryWidget> InventoryWidgetClass;

	/** DPI-scaled top-left position for the native/fallback inventory panel. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory")
	FVector2D InventoryMargin = FVector2D(40.0f, 180.0f);

	/** Optional FZCItemData table; assigning it initializes the subsystem on first presentation. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory|Data")
	TObjectPtr<UDataTable> InventoryDataTable;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory|Data", meta = (ClampMin = "1"))
	int32 InventoryCapacity = 6;

	/** Independent Enhanced Input action. It may also be assigned on BP_Player; see BindInventoryInput. */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory|Input")
	TObjectPtr<UInputAction> InventoryAction;

	/** The one root widget associated with the currently possessed local player. */
	UPROPERTY(Transient)
	TObjectPtr<UZCLayout> RootLayout;

	/** Character whose legacy LayoutRef currently backs RootLayout. */
	UPROPERTY(Transient)
	TObjectPtr<AZCCharBase> PresentedPlayer;

	/** The single indicator instance shown for the locally possessed player. */
	UPROPERTY(Transient)
	TObjectPtr<UZCTargetLockIndicatorWidget> TargetLockIndicator;

	/** 始终位于菜单之上的三心生命值 HUD。 */
	UPROPERTY(Transient)
	TObjectPtr<UZCHeartHealthWidget> HeartHealthWidget;

	/** One controller-owned inventory widget reused across open/close cycles. */
	UPROPERTY(Transient)
	TObjectPtr<UZCInventoryWidget> InventoryWidget;

	TWeakObjectPtr<AZCCharBase> BoundTargetLockPlayer;
	TWeakObjectPtr<UZCTargetLockComponent> BoundTargetLock;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZCase|Player Presentation", meta = (AllowPrivateAccess = "true"))
	bool bRuneMenuOpen = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZCase|Player Presentation", meta = (AllowPrivateAccess = "true"))
	bool bInventoryMenuOpen = false;

	/** True only when this controller was responsible for pausing the game. */
	bool bPausedByPresentation = false;

	bool bPresentationInitializationPending = false;
	bool bInventoryInputBound = false;
	FTimerHandle PresentationInitializationTimer;
	
};
