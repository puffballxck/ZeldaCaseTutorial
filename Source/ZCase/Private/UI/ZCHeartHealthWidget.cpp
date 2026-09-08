// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZCHeartHealthWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Combat/ZCAttributeComponent.h"
#include "Gameplay/ZCRuneRuntimeComponent.h"
#include "Engine/Texture2D.h"
#include "Math/UnrealMathUtility.h"
#include "Styling/SlateBrush.h"
#include "UObject/ConstructorHelpers.h"

UZCHeartHealthWidget::UZCHeartHealthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FullHeartTexture(
		TEXT("/Game/Assets/Icon/New_Heart/heart_full.heart_full"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> HalfHeartTexture(
		TEXT("/Game/Assets/Icon/New_Heart/heart_half.heart_half"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> EmptyHeartTexture(
		TEXT("/Game/Assets/Icon/New_Heart/heart_empty.heart_empty"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> RBSTexture(
		TEXT("/Game/Assets/Icon/New_Runes/bomb_sphere.bomb_sphere"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> RBBTexture(
		TEXT("/Game/Assets/Icon/New_Runes/bomb_box.bomb_box"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> MagTexture(
		TEXT("/Game/Assets/Icon/New_Runes/mega.mega"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> StasisTexture(
		TEXT("/Game/Assets/Icon/New_Runes/timelock.timelock"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> IceTexture(
		TEXT("/Game/Assets/Icon/New_Runes/icemaker.icemaker"));

	HeartFullTexture = FullHeartTexture.Object;
	HeartHalfTexture = HalfHeartTexture.Object;
	HeartEmptyTexture = EmptyHeartTexture.Object;
	RuneRBSTexture = RBSTexture.Object;
	RuneRBBTexture = RBBTexture.Object;
	RuneMagTexture = MagTexture.Object;
	RuneStasisTexture = StasisTexture.Object;
	RuneIceTexture = IceTexture.Object;
	FlashStates.SetNum(HalfHeartCount);
}

void UZCHeartHealthWidget::SetAttributes(UZCAttributeComponent* InAttributes)
{
	if (BoundAttributes != InAttributes)
	{
		UnbindFromAttributes();
		BoundAttributes = InAttributes;
	}

	// AddUniqueDynamic also repairs a binding after NativeDestruct without duplicating it
	// when the same component is supplied repeatedly.
	BindToAttributes();
	EnsureWidgetTree();
	ResetFlashAnimations();
	RefreshFromAttributes(false);
}

void UZCHeartHealthWidget::SetRuneRuntime(UZCRuneRuntimeComponent* InRuneRuntime)
{
	if (BoundRuneRuntime != InRuneRuntime)
	{
		UnbindFromRuneRuntime();
		BoundRuneRuntime = InRuneRuntime;
	}

	BindToRuneRuntime();
	EnsureWidgetTree();
	UpdateRuneIcon(BoundRuneRuntime ? BoundRuneRuntime->GetSelectedRune() : ERunes::R_EMAX);
}

FVector2D UZCHeartHealthWidget::GetHeartBarSize() const
{
	const FVector2D SafeHeartSize(FMath::Max(1.0f, HeartSize.X), FMath::Max(1.0f, HeartSize.Y));
	const float SafeSpacing = FMath::Max(0.0f, HeartSpacing);
	const FVector2D SafePadding(FMath::Max(0.0f, HeartBarPadding.X), FMath::Max(0.0f, HeartBarPadding.Y));
	return FVector2D(
		SafePadding.X * 2.0f + SafeHeartSize.X * HeartCount + SafeSpacing * (HeartCount - 1),
		SafePadding.Y * 2.0f + SafeHeartSize.Y + FMath::Max(0.0f, RuneIconGap)
			+ FMath::Max(1.0f, RuneIconSize.Y));
}

void UZCHeartHealthWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureWidgetTree();
	BindToRuneRuntime();
}

void UZCHeartHealthWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	BindToAttributes();
	BindToRuneRuntime();
	ResetFlashAnimations();
	RefreshFromAttributes(false);
	UpdateRuneIcon(BoundRuneRuntime ? BoundRuneRuntime->GetSelectedRune() : ERunes::R_EMAX);
}

void UZCHeartHealthWidget::NativeDestruct()
{
	UnbindFromAttributes();
	UnbindFromRuneRuntime();
	ResetFlashAnimations();
	Super::NativeDestruct();
}

void UZCHeartHealthWidget::EnsureWidgetTree()
{
	if (bWidgetTreeBuilt || !WidgetTree)
	{
		return;
	}

	CreateFlashTexture();
	const bool bHasBoundRoot = RootSizeBox != nullptr;
	const bool bHasBoundCanvas = HeartCanvas != nullptr;
	if (!RootSizeBox)
	{
		RootSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HeartBarSizeBox"));
	}
	if (!HeartCanvas)
	{
		HeartCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HeartBarCanvas"));
	}
	RootSizeBox->SetWidthOverride(GetHeartBarSize().X);
	RootSizeBox->SetHeightOverride(GetHeartBarSize().Y);
	if (!bHasBoundRoot || !bHasBoundCanvas)
	{
		RootSizeBox->AddChild(HeartCanvas);
		WidgetTree->RootWidget = RootSizeBox;
	}

	HeartImages.SetNum(HeartCount);
	FlashImages.SetNum(HalfHeartCount);
	for (int32 HeartIndex = 0; HeartIndex < HeartCount; ++HeartIndex)
	{
		const FName HeartName = *FString::Printf(TEXT("Heart_%d"), HeartIndex);
		UImage* HeartImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), HeartName);
		HeartImages[HeartIndex] = HeartImage;
		UCanvasPanelSlot* HeartSlot = HeartCanvas->AddChildToCanvas(HeartImage);
		const FVector2D Position(
			FMath::Max(0.0f, HeartBarPadding.X) + HeartIndex * (FMath::Max(1.0f, HeartSize.X) + FMath::Max(0.0f, HeartSpacing)),
			FMath::Max(0.0f, HeartBarPadding.Y));
		HeartSlot->SetPosition(Position);
		HeartSlot->SetSize(FVector2D(FMath::Max(1.0f, HeartSize.X), FMath::Max(1.0f, HeartSize.Y)));
	}

	const FVector2D SafeHeartSize(FMath::Max(1.0f, HeartSize.X), FMath::Max(1.0f, HeartSize.Y));
	const float HalfWidth = SafeHeartSize.X * 0.5f;
	for (int32 HalfIndex = 0; HalfIndex < HalfHeartCount; ++HalfIndex)
	{
		const FName FlashName = *FString::Printf(TEXT("HalfFlash_%d"), HalfIndex);
		UImage* FlashImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FlashName);
		FlashImages[HalfIndex] = FlashImage;
		FlashImage->SetBrush(MakeFlashBrush());
		FlashImage->SetColorAndOpacity(FLinearColor::White);
		FlashImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		FlashImage->SetVisibility(ESlateVisibility::Hidden);

		UCanvasPanelSlot* FlashSlot = HeartCanvas->AddChildToCanvas(FlashImage);
		const int32 HeartIndex = HalfIndex / 2;
		const bool bRightHalf = (HalfIndex % 2) == 1;
		const FVector2D HeartPosition(
			FMath::Max(0.0f, HeartBarPadding.X) + HeartIndex * (SafeHeartSize.X + FMath::Max(0.0f, HeartSpacing)),
			FMath::Max(0.0f, HeartBarPadding.Y));
		const FVector2D FlashSize(HalfWidth + 8.0f, SafeHeartSize.Y + 8.0f);
		const FVector2D FlashPosition(
			HeartPosition.X + (bRightHalf ? HalfWidth : 0.0f) - 4.0f,
			HeartPosition.Y - 4.0f);
		FlashSlot->SetPosition(FlashPosition);
		FlashSlot->SetSize(FlashSize);
	}

	const bool bHasBoundRuneIcon = SelectedRuneIcon != nullptr;
	if (!SelectedRuneIcon)
	{
		SelectedRuneIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SelectedRuneIcon"));
	}
	SelectedRuneIcon->SetVisibility(ESlateVisibility::Hidden);
	SelectedRuneIcon->SetColorAndOpacity(FLinearColor::White);
	SelectedRuneIcon->SetBrush(MakeRuneIconBrush(nullptr));
	if (!bHasBoundRuneIcon)
	{
		if (UCanvasPanelSlot* RuneSlot = HeartCanvas->AddChildToCanvas(SelectedRuneIcon))
		{
			const FVector2D SafeRuneSize(
				FMath::Max(1.0f, RuneIconSize.X),
				FMath::Max(1.0f, RuneIconSize.Y));
			const FVector2D HeartBarSize = GetHeartBarSize();
			RuneSlot->SetPosition(FVector2D(
				(HeartBarSize.X - SafeRuneSize.X) * 0.5f,
				FMath::Max(0.0f, HeartBarPadding.Y) + FMath::Max(1.0f, HeartSize.Y)
					+ FMath::Max(0.0f, RuneIconGap)));
			RuneSlot->SetSize(SafeRuneSize);
		}
	}

	bWidgetTreeBuilt = true;
	UpdateHeartImages(DisplayedHalfHearts);
	UpdateRuneIcon(BoundRuneRuntime ? BoundRuneRuntime->GetSelectedRune() : ERunes::R_EMAX);
}

void UZCHeartHealthWidget::CreateFlashTexture()
{
	if (FlashTexture)
	{
		return;
	}

	constexpr int32 TextureWidth = 32;
	constexpr int32 TextureHeight = 64;
	FlashTexture = UTexture2D::CreateTransient(TextureWidth, TextureHeight, PF_B8G8R8A8);
	if (!FlashTexture || !FlashTexture->GetPlatformData() || FlashTexture->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	FlashTexture->NeverStream = true;
	FlashTexture->SRGB = false;
	FlashTexture->Filter = TF_Bilinear;
	FlashTexture->AddressX = TA_Clamp;
	FlashTexture->AddressY = TA_Clamp;

	FTexture2DMipMap& Mip = FlashTexture->GetPlatformData()->Mips[0];
	FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	if (Pixels)
	{
		for (int32 Y = 0; Y < TextureHeight; ++Y)
		{
			const float Y01 = static_cast<float>(Y) / static_cast<float>(TextureHeight - 1);
			const float VerticalFade = FMath::Sin(PI * Y01);
			for (int32 X = 0; X < TextureWidth; ++X)
			{
				const float X01 = (static_cast<float>(X) + 0.5f) / static_cast<float>(TextureWidth);
				const float CenteredX = (X01 - 0.5f) / 0.20f;
				const float HorizontalFade = FMath::Exp(-0.5f * CenteredX * CenteredX);
				const uint8 Alpha = static_cast<uint8>(FMath::Clamp(255.0f * VerticalFade * HorizontalFade, 0.0f, 255.0f));
				Pixels[Y * TextureWidth + X] = FColor(255, 255, 255, Alpha);
			}
		}
	}
	Mip.BulkData.Unlock();
	FlashTexture->UpdateResource();
}

void UZCHeartHealthWidget::RefreshFromAttributes(const bool bAnimateDamage)
{
	if (!BoundAttributes)
	{
		if (!bHasDisplayedHealth)
		{
			DisplayedHalfHearts = HalfHeartCount;
			bHasDisplayedHealth = true;
		}
		UpdateHeartImages(DisplayedHalfHearts);
		return;
	}

	const int32 PreviousDisplayedHalfHearts = DisplayedHalfHearts;
	const int32 NewDisplayedHalfHearts = CalculateDisplayedHalfHearts(
		BoundAttributes->GetHealth(), BoundAttributes->GetMaxHealth());
	DisplayedHalfHearts = NewDisplayedHalfHearts;
	bHasDisplayedHealth = true;
	UpdateHeartImages(NewDisplayedHalfHearts);

	if (bAnimateDamage && NewDisplayedHalfHearts < PreviousDisplayedHalfHearts)
	{
		for (int32 HalfIndex = PreviousDisplayedHalfHearts - 1; HalfIndex >= NewDisplayedHalfHearts; --HalfIndex)
		{
			TriggerHalfFlash(HalfIndex);
		}
	}
}

void UZCHeartHealthWidget::UpdateHeartImages(const int32 NewDisplayedHalfHearts)
{
	if (!bWidgetTreeBuilt)
	{
		return;
	}

	const int32 ClampedHalfHearts = FMath::Clamp(NewDisplayedHalfHearts, 0, HalfHeartCount);
	for (int32 HeartIndex = 0; HeartIndex < HeartCount; ++HeartIndex)
	{
		const int32 HeartHalfUnits = FMath::Clamp(ClampedHalfHearts - HeartIndex * 2, 0, 2);
		UTexture2D* HeartTexture = HeartHalfUnits == 2
			? HeartFullTexture.Get()
			: HeartHalfUnits == 1 ? HeartHalfTexture.Get() : HeartEmptyTexture.Get();
		if (HeartImages.IsValidIndex(HeartIndex) && HeartImages[HeartIndex])
		{
			HeartImages[HeartIndex]->SetBrush(MakeHeartBrush(HeartTexture));
		}
	}
}

void UZCHeartHealthWidget::TriggerHalfFlash(const int32 HalfIndex)
{
	if (!FlashStates.IsValidIndex(HalfIndex) || !FlashImages.IsValidIndex(HalfIndex) || !FlashImages[HalfIndex])
	{
		return;
	}

	FHalfFlashState& State = FlashStates[HalfIndex];
	if (!State.bActive)
	{
		++ActiveFlashCount;
	}
	State.Elapsed = 0.0f;
	State.bActive = true;
	UImage* FlashImage = FlashImages[HalfIndex];
	FlashImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	FlashImage->SetRenderOpacity(1.0f);
	FWidgetTransform Transform;
	Transform.Scale = FVector2D(1.0f, 1.0f);
	Transform.Translation = FVector2D::ZeroVector;
	FlashImage->SetRenderTransform(Transform);
}

void UZCHeartHealthWidget::ResetFlashAnimations()
{
	ActiveFlashCount = 0;
	for (int32 HalfIndex = 0; HalfIndex < FlashStates.Num(); ++HalfIndex)
	{
		FlashStates[HalfIndex].Elapsed = 0.0f;
		FlashStates[HalfIndex].bActive = false;
		if (FlashImages.IsValidIndex(HalfIndex) && FlashImages[HalfIndex])
		{
			FlashImages[HalfIndex]->SetVisibility(ESlateVisibility::Hidden);
			FlashImages[HalfIndex]->SetRenderOpacity(0.0f);
			FWidgetTransform Transform;
			FlashImages[HalfIndex]->SetRenderTransform(Transform);
		}
	}
}

void UZCHeartHealthWidget::BindToAttributes()
{
	if (BoundAttributes)
	{
		BoundAttributes->OnHealthChanged.AddUniqueDynamic(this, &UZCHeartHealthWidget::HandleHealthChanged);
	}
}

void UZCHeartHealthWidget::UnbindFromAttributes()
{
	if (BoundAttributes)
	{
		BoundAttributes->OnHealthChanged.RemoveDynamic(this, &UZCHeartHealthWidget::HandleHealthChanged);
	}
}

void UZCHeartHealthWidget::BindToRuneRuntime()
{
	if (BoundRuneRuntime)
	{
		BoundRuneRuntime->OnSelectedRuneChanged.AddUniqueDynamic(
			this, &UZCHeartHealthWidget::HandleSelectedRuneChanged);
	}
}

void UZCHeartHealthWidget::UnbindFromRuneRuntime()
{
	if (BoundRuneRuntime)
	{
		BoundRuneRuntime->OnSelectedRuneChanged.RemoveDynamic(
			this, &UZCHeartHealthWidget::HandleSelectedRuneChanged);
	}
}

int32 UZCHeartHealthWidget::CalculateDisplayedHalfHearts(const float Health, const float MaxHealth) const
{
	if (!FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f)
	{
		return 0;
	}

	const float Ratio = FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f);
	// The small bias keeps values such as (100 - 100 / 6) on the intended fifth half.
	const int32 HalfHearts = FMath::FloorToInt(Ratio * static_cast<float>(HalfHeartCount) + 0.0005f);
	return FMath::Clamp(HalfHearts, 0, HalfHeartCount);
}

FSlateBrush UZCHeartHealthWidget::MakeHeartBrush(UTexture2D* Texture) const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = HeartSize;
	Brush.SetUVRegion(FBox2D(FVector2D(0.18f, 0.20f), FVector2D(0.82f, 0.80f)));
	return Brush;
}

FSlateBrush UZCHeartHealthWidget::MakeFlashBrush() const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(FlashTexture);
	Brush.ImageSize = FVector2D(FMath::Max(1.0f, HeartSize.X * 0.5f + 8.0f), FMath::Max(1.0f, HeartSize.Y + 8.0f));
	return Brush;
}

FSlateBrush UZCHeartHealthWidget::MakeRuneIconBrush(UTexture2D* Texture) const
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = FVector2D(
		FMath::Max(1.0f, RuneIconSize.X),
		FMath::Max(1.0f, RuneIconSize.Y));
	return Brush;
}

void UZCHeartHealthWidget::UpdateRuneIcon(const ERunes NewRune)
{
	if (!SelectedRuneIcon)
	{
		return;
	}

	UTexture2D* RuneTexture = nullptr;
	switch (NewRune)
	{
	case ERunes::R_RBS:
		RuneTexture = RuneRBSTexture;
		break;
	case ERunes::R_RBB:
		RuneTexture = RuneRBBTexture;
		break;
	case ERunes::R_Mag:
		RuneTexture = RuneMagTexture;
		break;
	case ERunes::R_Stasis:
		RuneTexture = RuneStasisTexture;
		break;
	case ERunes::R_Ice:
		RuneTexture = RuneIceTexture;
		break;
	case ERunes::R_EMAX:
	default:
		break;
	}

	SelectedRuneIcon->SetBrush(MakeRuneIconBrush(RuneTexture));
	SelectedRuneIcon->SetVisibility(RuneTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UZCHeartHealthWidget::HandleHealthChanged(const float PreviousHealth, const float CurrentHealth)
{
	if (!BoundAttributes)
	{
		return;
	}

	const int32 PreviousDisplayedHalfHearts = CalculateDisplayedHalfHearts(
		PreviousHealth, BoundAttributes->GetMaxHealth());
	const int32 CurrentDisplayedHalfHearts = CalculateDisplayedHalfHearts(
		CurrentHealth, BoundAttributes->GetMaxHealth());
	const int32 DisplayedBeforeEvent = bHasDisplayedHealth ? DisplayedHalfHearts : PreviousDisplayedHalfHearts;

	DisplayedHalfHearts = CurrentDisplayedHalfHearts;
	bHasDisplayedHealth = true;
	UpdateHeartImages(CurrentDisplayedHalfHearts);

	if (CurrentDisplayedHalfHearts < DisplayedBeforeEvent)
	{
		for (int32 HalfIndex = DisplayedBeforeEvent - 1; HalfIndex >= CurrentDisplayedHalfHearts; --HalfIndex)
		{
			TriggerHalfFlash(HalfIndex);
		}
	}
	else if (CurrentDisplayedHalfHearts > DisplayedBeforeEvent)
	{
		ResetFlashAnimations();
	}
}

void UZCHeartHealthWidget::HandleSelectedRuneChanged(const ERunes PreviousRune, const ERunes CurrentRune)
{
	UpdateRuneIcon(CurrentRune);
}

void UZCHeartHealthWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ActiveFlashCount <= 0)
	{
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, InDeltaTime);
	const float SafeFlashDuration = FMath::Max(FlashDuration, KINDA_SMALL_NUMBER);
	const bool bFlashIsInstant = FlashDuration <= KINDA_SMALL_NUMBER;
	for (int32 HalfIndex = 0; HalfIndex < FlashStates.Num(); ++HalfIndex)
	{
		FHalfFlashState& State = FlashStates[HalfIndex];
		if (!State.bActive || !FlashImages.IsValidIndex(HalfIndex) || !FlashImages[HalfIndex])
		{
			continue;
		}

		State.Elapsed += SafeDeltaTime;
		const float Progress = bFlashIsInstant ? 1.0f : State.Elapsed / SafeFlashDuration;
		if (Progress >= 1.0f)
		{
			State.bActive = false;
			--ActiveFlashCount;
			FlashImages[HalfIndex]->SetVisibility(ESlateVisibility::Hidden);
			FlashImages[HalfIndex]->SetRenderOpacity(0.0f);
			continue;
		}

		const float ClampedProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
		const float Fade = 1.0f - ClampedProgress;
		const float Swell = FMath::Sin(PI * ClampedProgress) * FMath::Max(0.0f, FlashScaleAmount);
		FWidgetTransform Transform;
		Transform.Scale = FVector2D(1.0f + Swell, 1.0f + Swell);
		Transform.Translation = FVector2D(0.0f, -FMath::Max(0.0f, FlashRiseDistance) * ClampedProgress);
		FlashImages[HalfIndex]->SetRenderOpacity(Fade);
		FlashImages[HalfIndex]->SetRenderTransform(Transform);
	}
}
