// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/ZCPlayerController.h"

#include "Characters/ZCCharBase.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/ZCLayout.h"
#include "UI/ZCTargetLockIndicatorWidget.h"
#include "UI/ZCHeartHealthWidget.h"

void AZCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	RequestPresentationInitialization();
}

void AZCPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(PresentationInitializationTimer);
	bPresentationInitializationPending = false;
	ReleasePlayerPresentation(PresentedPlayer);
	if (IsValid(TargetLockIndicator))
	{
		TargetLockIndicator->RemoveFromParent();
	}
	TargetLockIndicator = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AZCPlayerController::OnPossess(APawn* InPawn)
{
	// A pawn can be replaced without being destroyed. Clear its gameplay lock
	// before the controller starts presenting the newly possessed pawn.
	if (AZCCharBase* PreviousPlayer = Cast<AZCCharBase>(GetPawn());
		IsValid(PreviousPlayer) && PreviousPlayer != InPawn && PreviousPlayer->TargetLock)
	{
		PreviousPlayer->TargetLock->ClearTarget();
	}

	Super::OnPossess(InPawn);

	if (IsLocalController())
	{
		RequestPresentationInitialization();
	}
}

void AZCPlayerController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(PresentationInitializationTimer);
	bPresentationInitializationPending = false;

	AZCCharBase* PreviousPlayer = PresentedPlayer;
	if (!IsValid(PreviousPlayer))
	{
		PreviousPlayer = BoundTargetLockPlayer.Get();
	}
	if (!IsValid(PreviousPlayer))
	{
		PreviousPlayer = Cast<AZCCharBase>(GetPawn());
	}
	ReleasePlayerPresentation(PreviousPlayer);

	Super::OnUnPossess();
}

void AZCPlayerController::InitializePlayerPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	AZCCharBase* PlayerCharacter = Cast<AZCCharBase>(GetPawn());
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	// Wait until the pawn's BeginPlay has completed so its gameplay state and
	// Blueprint defaults are ready before the presentation binds to it.
	if (!PlayerCharacter->HasActorBegunPlay())
	{
		RequestPresentationInitialization();
		return;
	}

	if (PresentedPlayer && PresentedPlayer != PlayerCharacter)
	{
		ReleasePlayerPresentation(PresentedPlayer);
	}
	else if (BoundTargetLockPlayer.IsValid() && BoundTargetLockPlayer.Get() != PlayerCharacter)
	{
		ReleasePlayerPresentation(BoundTargetLockPlayer.Get());
	}

	PresentedPlayer = PlayerCharacter;
	InitializeTargetLockPresentation(PlayerCharacter);
	if (!IsValid(HeartHealthWidget))
	{
		const TSubclassOf<UZCHeartHealthWidget> WidgetClass = HeartHealthWidgetClass
			? HeartHealthWidgetClass.Get() : UZCHeartHealthWidget::StaticClass();
		HeartHealthWidget = CreateWidget<UZCHeartHealthWidget>(this, WidgetClass);
	}
	if (IsValid(HeartHealthWidget))
	{
		HeartHealthWidget->SetAttributes(PlayerCharacter->Attributes);
		if (!HeartHealthWidget->IsInViewport())
		{
			HeartHealthWidget->SetVisibility(ESlateVisibility::Hidden);
			HeartHealthWidget->AddToPlayerScreen(20);
			HeartHealthWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
			HeartHealthWidget->SetDesiredSizeInViewport(HeartHealthWidget->GetHeartBarSize());
			HeartHealthWidget->SetPositionInViewport(HeartHealthMargin, false);
			HeartHealthWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// Adopt a legacy layout only when no controller-owned class is configured.
	// This keeps old BP_Player assets working while allowing RootLayoutClass to
	// become the authoritative composition root as assets are migrated.
	if (!RootLayoutClass && IsValid(PlayerCharacter->LayoutRef) && PlayerCharacter->LayoutRef != RootLayout)
	{
		if (IsValid(RootLayout))
		{
			RootLayout->RemoveFromParent();
		}

		RootLayout = PlayerCharacter->LayoutRef;
	}

	TSubclassOf<UUserWidget> LayoutClass = PlayerCharacter->LayoutClassRef;
	if (RootLayoutClass)
	{
		LayoutClass = RootLayoutClass;
	}
	if (!IsValid(RootLayout) && LayoutClass)
	{
		RootLayout = CreateWidget<UZCLayout>(this, LayoutClass);
		if (IsValid(RootLayout))
		{
			PlayerCharacter->LayoutRef = RootLayout;
			RootLayout->ConstructDeferred(PlayerCharacter);
		}
	}

	if (!IsValid(RootLayout))
	{
		return;
	}

	RootLayout->SetOwningPlayer(this);
	if (!RootLayout->IsInViewport())
	{
		RootLayout->AddToPlayerScreen();
	}

	// Keep the pawn reference synchronized for stamina/rune code that still
	// talks to LayoutRef directly during the staged migration.
	PlayerCharacter->LayoutRef = RootLayout;
	ApplyRuneMenuPolicy();
}

void AZCPlayerController::SetRuneMenuOpen(const bool bOpen)
{
	if (bRuneMenuOpen == bOpen)
	{
		// Reapplying is intentional: callers may invoke this before the layout is
		// created, or after focus was taken by another widget.
		ApplyRuneMenuPolicy();
		return;
	}

	bRuneMenuOpen = bOpen;
	ApplyRuneMenuPolicy();
}

void AZCPlayerController::ToggleRuneMenu()
{
	SetRuneMenuOpen(!bRuneMenuOpen);
}

void AZCPlayerController::RequestPresentationInitialization()
{
	if (bPresentationInitializationPending || !GetWorld())
	{
		return;
	}

	bPresentationInitializationPending = true;
	PresentationInitializationTimer = GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &AZCPlayerController::HandleDeferredPresentationInitialization));
}

void AZCPlayerController::HandleDeferredPresentationInitialization()
{
	bPresentationInitializationPending = false;
	InitializePlayerPresentation();
}

void AZCPlayerController::ApplyRuneMenuPolicy()
{
	if (!IsLocalController())
	{
		return;
	}

	if (IsValid(RootLayout))
	{
		RootLayout->SetRuneMenuOpen(bRuneMenuOpen);
	}
	else if (bRuneMenuOpen)
	{
		// Do not pause before there is a focus target: game-time timers do not
		// advance while paused, so initialization could otherwise deadlock.
		RequestPresentationInitialization();
		return;
	}

	if (bRuneMenuOpen)
	{
		SetShowMouseCursor(true);

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		if (IsValid(RootLayout))
		{
			InputMode.SetWidgetToFocus(RootLayout->TakeWidget());
		}
		SetInputMode(InputMode);

		if (!UGameplayStatics::IsGamePaused(this))
		{
			bPausedByRuneMenu = SetPause(true);
		}
		return;
	}

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	if (bPausedByRuneMenu)
	{
		SetPause(false);
		bPausedByRuneMenu = false;
	}
}

void AZCPlayerController::ReleasePlayerPresentation(AZCCharBase* PreviousPlayer)
{
	ReleaseTargetLockPresentation(PreviousPlayer);
	if (IsValid(HeartHealthWidget))
	{
		HeartHealthWidget->SetAttributes(nullptr);
		HeartHealthWidget->RemoveFromParent();
	}
	HeartHealthWidget = nullptr;

	if (bRuneMenuOpen || bPausedByRuneMenu)
	{
		bRuneMenuOpen = false;
		ApplyRuneMenuPolicy();
	}

	if (IsValid(PreviousPlayer) && PreviousPlayer->LayoutRef == RootLayout)
	{
		PreviousPlayer->LayoutRef = nullptr;
	}

	if (IsValid(RootLayout))
	{
		RootLayout->RemoveFromParent();
	}

	RootLayout = nullptr;
	PresentedPlayer = nullptr;
}

void AZCPlayerController::InitializeTargetLockPresentation(AZCCharBase* PlayerCharacter)
{
	if (!IsLocalController() || !IsValid(PlayerCharacter))
	{
		return;
	}

	if (!IsValid(TargetLockIndicator) && TargetLockIndicatorClass)
	{
		TargetLockIndicator = CreateWidget<UZCTargetLockIndicatorWidget>(this, TargetLockIndicatorClass);
		if (IsValid(TargetLockIndicator))
		{
			// Keep the indicator above the controller-owned root layout and its
			// rune menu while remaining a single reusable widget instance.
			TargetLockIndicator->AddToPlayerScreen(100);
		}
	}

	UZCTargetLockComponent* NewTargetLock = PlayerCharacter->TargetLock;
	if (BoundTargetLock.Get() != NewTargetLock)
	{
		if (BoundTargetLock.IsValid())
		{
			BoundTargetLock->OnTargetChanged.RemoveDynamic(this, &AZCPlayerController::HandleTargetChanged);
		}

		BoundTargetLock = NewTargetLock;
		BoundTargetLockPlayer = PlayerCharacter;
		if (BoundTargetLock.IsValid())
		{
			BoundTargetLock->OnTargetChanged.AddUniqueDynamic(this, &AZCPlayerController::HandleTargetChanged);
		}
	}

	// Synchronize an already-active target in case possession/presentation
	// initialization happened after the pawn's first target change event.
	HandleTargetChanged(nullptr, BoundTargetLock.IsValid() ? BoundTargetLock->GetCurrentTarget() : nullptr);
}

void AZCPlayerController::ReleaseTargetLockPresentation(AZCCharBase* PreviousPlayer)
{
	if (!IsValid(PreviousPlayer))
	{
		PreviousPlayer = BoundTargetLockPlayer.Get();
	}

	if (BoundTargetLock.IsValid())
	{
		BoundTargetLock->OnTargetChanged.RemoveDynamic(this, &AZCPlayerController::HandleTargetChanged);
	}

	UZCTargetLockComponent* PreviousTargetLock = IsValid(PreviousPlayer) ? PreviousPlayer->TargetLock : nullptr;
	if (PreviousTargetLock)
	{
		PreviousTargetLock->ClearTarget();
	}
	if (BoundTargetLock.IsValid() && BoundTargetLock.Get() != PreviousTargetLock)
	{
		BoundTargetLock->ClearTarget();
	}

	BoundTargetLock = nullptr;
	BoundTargetLockPlayer = nullptr;
	if (IsValid(TargetLockIndicator))
	{
		TargetLockIndicator->ClearTarget();
	}
}

void AZCPlayerController::HandleTargetChanged(AActor* PreviousTarget, AActor* CurrentTarget)
{
	if (!IsValid(TargetLockIndicator))
	{
		return;
	}

	if (IsValid(CurrentTarget))
	{
		TargetLockIndicator->SetTarget(CurrentTarget);
	}
	else
	{
		TargetLockIndicator->ClearTarget();
	}
}
