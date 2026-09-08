// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "Styling/SlateBrush.h"
#include "ZCHeartHealthWidget.generated.h"

class UCanvasPanel;
class UImage;
class USizeBox;
class UTexture2D;
class UZCAttributeComponent;
class UZCRuneRuntimeComponent;

/** Native three-heart health bar. Each heart represents two half-heart units. */
UCLASS()
class ZCASE_API UZCHeartHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UZCHeartHealthWidget(const FObjectInitializer& ObjectInitializer);

	/** Binds this widget to an attribute component and refreshes the bar without a damage flash. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|UI|Health")
	void SetAttributes(UZCAttributeComponent* InAttributes);

	/** Binds the always-visible rune icon below the health bar to the player's selection state. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|UI|Runes")
	void SetRuneRuntime(UZCRuneRuntimeComponent* InRuneRuntime);

	/** Returns the currently displayed number of half-heart units, from zero through six. */
	UFUNCTION(BlueprintPure, Category = "ZCase|UI|Health")
	int32 GetDisplayedHalfHearts() const { return DisplayedHalfHearts; }

	/** Returns the viewport size the controller should reserve for this bar. */
	UFUNCTION(BlueprintPure, Category = "ZCase|UI|Health")
	FVector2D GetHeartBarSize() const;

	/** Size of a single heart thumbnail in the native row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "1.0"))
	FVector2D HeartSize = FVector2D(56.0f, 52.0f);

	/** Horizontal gap between adjacent heart thumbnails. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	float HeartSpacing = 6.0f;

	/** Padding around the three-heart row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	FVector2D HeartBarPadding = FVector2D(5.0f, 10.0f);

	/** Lifetime of one white half-heart damage flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.01"))
	float FlashDuration = 0.4f;

	/** Distance in screen space that a damage flash rises while fading. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	float FlashRiseDistance = 8.0f;

	/** Maximum scale swell of a damage flash at the middle of its animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	float FlashScaleAmount = 0.06f;

	/** Size of the selected-rune icon rendered below the heart row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Runes", meta = (ClampMin = "1.0"))
	FVector2D RuneIconSize = FVector2D(64.0f, 64.0f);

	/** Gap between the heart row and the selected-rune icon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Runes", meta = (ClampMin = "0.0"))
	float RuneIconGap = 8.0f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FHalfFlashState
	{
		float Elapsed = 0.0f;
		bool bActive = false;
	};

	static constexpr int32 HeartCount = 3;
	static constexpr int32 HalfHeartCount = 6;

	void EnsureWidgetTree();
	void CreateFlashTexture();
	void RefreshFromAttributes(bool bAnimateDamage);
	void UpdateHeartImages(int32 NewDisplayedHalfHearts);
	void TriggerHalfFlash(int32 HalfIndex);
	void ResetFlashAnimations();
	void BindToAttributes();
	void UnbindFromAttributes();
	void BindToRuneRuntime();
	void UnbindFromRuneRuntime();
	int32 CalculateDisplayedHalfHearts(float Health, float MaxHealth) const;
	FSlateBrush MakeHeartBrush(UTexture2D* Texture) const;
	FSlateBrush MakeFlashBrush() const;
	FSlateBrush MakeRuneIconBrush(UTexture2D* Texture) const;
	void UpdateRuneIcon(ERunes NewRune);

	UFUNCTION()
	void HandleHealthChanged(float PreviousHealth, float CurrentHealth);

	UFUNCTION()
	void HandleSelectedRuneChanged(ERunes PreviousRune, ERunes CurrentRune);

	/** Designer-owned root container. WBP_HeartHealth must provide this named widget. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> RootSizeBox;

	/** Designer-owned canvas used by native health/flash image updates. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> HeartCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeartImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> FlashImages;

	/** Designer-owned selected-rune icon; its layout is controlled by WBP_HeartHealth. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SelectedRuneIcon;

	UPROPERTY(Transient)
	TObjectPtr<UZCAttributeComponent> BoundAttributes;

	UPROPERTY(Transient)
	TObjectPtr<UZCRuneRuntimeComponent> BoundRuneRuntime;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Health")
	TObjectPtr<UTexture2D> HeartFullTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Health")
	TObjectPtr<UTexture2D> HeartHalfTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Health")
	TObjectPtr<UTexture2D> HeartEmptyTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Runes")
	TObjectPtr<UTexture2D> RuneRBSTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Runes")
	TObjectPtr<UTexture2D> RuneRBBTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Runes")
	TObjectPtr<UTexture2D> RuneMagTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Runes")
	TObjectPtr<UTexture2D> RuneStasisTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Runes")
	TObjectPtr<UTexture2D> RuneIceTexture;

	/** One generated soft-light texture reused by all six half-heart overlays. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FlashTexture;

	TArray<FHalfFlashState> FlashStates;
	int32 ActiveFlashCount = 0;
	int32 DisplayedHalfHearts = HalfHeartCount;
	bool bHasDisplayedHealth = false;
	bool bWidgetTreeBuilt = false;
};
