// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZCTargetLockIndicatorWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Combat/ZCTargetable.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool IsFiniteIndicatorVector(const FVector& Value)
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
	// 先隐藏绘制内容并初始化尺寸，防止首次显示时闪出全屏默认布局。
	// 暂时无法投影时仍保留可 Tick 的布局。
	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetPositionInViewport(FVector2D::ZeroVector, false);
	SetIndicatorTickEnabled(true);
	InitializeViewportLayout();
	SetPositionInViewport(GetDesiredSize() * 0.5f, false);

	if (UpdateIndicatorPosition())
	{
		SetRenderOpacity(1.0f);
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

	if (!IsValidIndicatorTarget(Target.Get()))
	{
		ClearTarget();
		return;
	}

	if (!UpdateIndicatorPosition())
	{
		// 离屏/短暂投影失败不是目标失效。保留引用和 Tick，并把透明控件
		// 留在视口内，避免 Slate 裁剪掉屏幕外控件后停止其更新。
		SetRenderOpacity(0.0f);
		SetPositionInViewport(GetDesiredSize() * 0.5f, false);
		return;
	}

	SetRenderOpacity(1.0f);
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
	if (!IsFiniteIndicatorVector(TargetLocation) || !IsFiniteIndicatorVector(IndicatorLocation))
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

	const FVector2D ViewSize = UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PlayerController).GetLocalSize();
	if (!FMath::IsFinite(ScreenPosition.X) || !FMath::IsFinite(ScreenPosition.Y)
		|| !FMath::IsFinite(ViewSize.X) || !FMath::IsFinite(ViewSize.Y)
		|| ViewSize.X <= 0.0f || ViewSize.Y <= 0.0f
		|| ScreenPosition.X < 0.0f || ScreenPosition.X > ViewSize.X
		|| ScreenPosition.Y < 0.0f || ScreenPosition.Y > ViewSize.Y)
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
