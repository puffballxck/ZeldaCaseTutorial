// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "ZCHeartHealthWidget.generated.h"

class UCanvasPanel;
class UImage;
class USizeBox;
class UTexture2D;
class UZCAttributeComponent;

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
	int32 CalculateDisplayedHalfHearts(float Health, float MaxHealth) const;
	FSlateBrush MakeHeartBrush(UTexture2D* Texture) const;
	FSlateBrush MakeFlashBrush() const;

	UFUNCTION()
	void HandleHealthChanged(float PreviousHealth, float CurrentHealth);

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> HeartCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeartImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> FlashImages;

	UPROPERTY(Transient)
	TObjectPtr<UZCAttributeComponent> BoundAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Health")
	TObjectPtr<UTexture2D> HeartFullTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Health")
	TObjectPtr<UTexture2D> HeartHalfTexture;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|UI|Health")
	TObjectPtr<UTexture2D> HeartEmptyTexture;

	/** One generated soft-light texture reused by all six half-heart overlays. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FlashTexture;

	TArray<FHalfFlashState> FlashStates;
	int32 ActiveFlashCount = 0;
	int32 DisplayedHalfHearts = HalfHeartCount;
	bool bHasDisplayedHealth = false;
	bool bWidgetTreeBuilt = false;
};
