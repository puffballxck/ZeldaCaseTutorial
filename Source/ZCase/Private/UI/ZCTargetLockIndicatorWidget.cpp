// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZCTargetLockIndicatorWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Combat/ZCTargetable.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}
}

void UZCTargetLockIndicatorWidget::SetTarget(AActor* NewTarget)
{
	if (!IsValidIndicatorTarget(NewTarget))
	{
		ClearTarget();
		return;
	}

	Target = NewTarget;
	// AddToPlayerScreen initially uses a full-screen slot. Normalize the slot
	// and resolve the first screen position before allowing Slate to paint this
	// widget, otherwise the first frame can show the WBP's default geometry.
	SetVisibility(ESlateVisibility::Collapsed);
	SetIndicatorTickEnabled(true);
	InitializeViewportLayout();

	if (UpdateIndicatorPosition())
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UZCTargetLockIndicatorWidget::ClearTarget()
{
	Target.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIndicatorTickEnabled(false);
}

void UZCTargetLockIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The viewport position represents the arrow center; the Blueprint image
	// remains responsible for its visual brush and final 48x48 presentation.
	SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
	ClearTarget();
}

void UZCTargetLockIndicatorWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!UpdateIndicatorPosition())
	{
		// This is a presentation failure only; gameplay lock cleanup remains in
		// UZCTargetLockComponent so the UI cannot drive gameplay state backwards.
		ClearTarget();
		return;
	}

	// SetTarget keeps the widget hidden until its first valid position exists.
	// This also covers a transient projection failure during initial setup.
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

bool UZCTargetLockIndicatorWidget::UpdateIndicatorPosition()
{
	AActor* CurrentTarget = Target.Get();
	if (!IsValidIndicatorTarget(CurrentTarget))
	{
		return false;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	const IZCTargetable* Targetable = Cast<IZCTargetable>(CurrentTarget);
	if (!PlayerController || !Targetable)
	{
		return false;
	}

	const FVector TargetLocation = Targetable->GetTargetLockLocation();
	const float SafeHeightOffset = FMath::IsFinite(WorldHeightOffset) ? WorldHeightOffset : 0.0f;
	const FVector IndicatorLocation = TargetLocation + FVector::UpVector * SafeHeightOffset;
	if (!IsFiniteVector(TargetLocation) || !IsFiniteVector(IndicatorLocation))
	{
		return false;
	}

	FVector2D ScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		IndicatorLocation,
		ScreenPosition,
		true))
	{
		return false;
	}

	SetPositionInViewport(ScreenPosition, false);
	return true;
}

void UZCTargetLockIndicatorWidget::InitializeViewportLayout()
{
	ForceLayoutPrepass();

	const FVector2D DesiredSize = GetDesiredSize();
	if (FMath::IsFinite(DesiredSize.X) && FMath::IsFinite(DesiredSize.Y)
		&& DesiredSize.X > 0.0f && DesiredSize.Y > 0.0f)
	{
		// Preserve the size authored by WBP_TargetLockIndicator while replacing
		// AddToPlayerScreen's initial full-screen slot with an auto-sized slot.
		SetDesiredSizeInViewport(DesiredSize);
	}
}

bool UZCTargetLockIndicatorWidget::IsValidIndicatorTarget(const AActor* Candidate) const
{
	if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(UZCTargetable::StaticClass()))
	{
		return false;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(Candidate);
	return Targetable && Targetable->CanBeTargetLocked();
}

void UZCTargetLockIndicatorWidget::SetIndicatorTickEnabled(const bool bEnabled)
{
	if (bIndicatorTickEnabled == bEnabled)
	{
		return;
	}

	bIndicatorTickEnabled = bEnabled;
	// UE 5.8 UUserWidget recomputes the Slate tick state from this public
	// native/script tick flag. The class metadata disables the always-on native
	// path; toggling the flag lets the indicator tick only while it has a target.
	bHasScriptImplementedTick = bEnabled;
	UpdateCanTick();
}
