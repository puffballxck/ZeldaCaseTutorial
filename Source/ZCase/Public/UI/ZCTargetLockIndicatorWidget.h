// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "ZCTargetLockIndicatorWidget.generated.h"

/**
 * Displays the local player's current target lock anchor without owning any
 * gameplay lock state.
 */
UCLASS(meta = (DisableNativeTick))
class ZCASE_API UZCTargetLockIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Assigns the actor whose target-lock anchor should be displayed. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	void SetTarget(AActor* NewTarget);

	/** Hides the indicator and releases its weak target reference. */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	void ClearTarget();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** World-space height above the target-lock anchor where the arrow is placed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float WorldHeightOffset = 60.0f;

private:
	bool IsValidIndicatorTarget(const AActor* Candidate) const;
	bool UpdateIndicatorPosition();
	void InitializeViewportLayout();
	void SetIndicatorTickEnabled(bool bEnabled);

	TWeakObjectPtr<AActor> Target;
	bool bIndicatorTickEnabled = false;
};
