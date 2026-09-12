// 版权所有 Epic Games, Inc，保留所有权利

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
	// 先隐藏绘制内容并初始化尺寸，防止首次显示时闪出全屏默认布局
	// 暂时无法投影时仍保留可 Tick 的布局
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

	// 视口位置表示箭头中心，蓝图图片
	// 继续负责视觉 Brush 和最终 48x48 的呈现
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
		// 离屏/短暂投影失败不是目标失效保留引用和 Tick，并把透明控件
		// 留在视口内，避免 Slate 裁剪掉屏幕外控件后停止其更新
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
		// 替换布局时保留 WBP_TargetLockIndicator 设定的尺寸
		// 用自动尺寸 Slot 替代 AddToPlayerScreen 初始创建的全屏 Slot
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
	// UE 5.8 的 UUserWidget 会根据这个公开的
	// 原生或脚本 Tick 标记重新计算 Slate Tick 状态，类元数据关闭始终开启的原生
	// 路径，切换该标记后指示器只在持有目标时 Tick
	bHasScriptImplementedTick = bEnabled;
	UpdateCanTick();
}
