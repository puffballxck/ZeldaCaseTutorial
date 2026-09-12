// 请在项目设置的说明页面填写版权声明


#include "Data/ZCPlayerController.h"

#include "Characters/ZCCharBase.h"
#include "Combat/ZCTargetLockComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/ZCInventoryWidget.h"
#include "Inventory/ZCInventorySubsystem.h"
#include "UI/ZCLayout.h"
#include "UI/ZCTargetLockIndicatorWidget.h"
#include "UI/ZCHeartHealthWidget.h"
#include "UObject/UObjectGlobals.h"

void AZCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	BindInventoryInput();
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
	// Pawn 可能在未销毁时被替换，因此先清除旧 Pawn 的玩法锁定
	// 再由控制器开始表现新被 Possess 的 Pawn
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

void AZCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BindInventoryInput();
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

	// 等待 Pawn 的 BeginPlay 完成，使其玩法状态和
	// 蓝图默认值准备完成后再绑定表现层
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
	if (!InventoryAction)
	{
		InventoryAction = PlayerCharacter->InventoryAction;
	}
	BindInventoryInput();
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
		HeartHealthWidget->SetRuneRuntime(PlayerCharacter->RuneRuntime);
		if (!HeartHealthWidget->IsInViewport())
		{
			HeartHealthWidget->SetVisibility(ESlateVisibility::Hidden);
			HeartHealthWidget->AddToPlayerScreen(5);
			HeartHealthWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
			HeartHealthWidget->SetDesiredSizeInViewport(HeartHealthWidget->GetHeartBarSize());
			HeartHealthWidget->SetPositionInViewport(HeartHealthMargin, false);
			HeartHealthWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	// 仅当未配置控制器持有的布局类时才接管旧布局
	// 这样既兼容旧 BP_Player 资产，也允许 RootLayoutClass
	// 在资产迁移过程中成为权威组合根节点
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
		InitializeInventoryPresentation();
		return;
	}

	RootLayout->SetOwningPlayer(this);
	if (!RootLayout->IsInViewport())
	{
		RootLayout->AddToPlayerScreen();
	}

	// 保持 Pawn 引用同步，供迁移期间仍然
	// 直接访问 LayoutRef 的体力与符文代码使用
	PlayerCharacter->LayoutRef = RootLayout;
	InitializeInventoryPresentation();
	ApplyRuneMenuPolicy();
}

void AZCPlayerController::SetRuneMenuOpen(const bool bOpen)
{
	if (bOpen)
	{
		// 符文面板和背包面板是互斥的表现状态
		bInventoryMenuOpen = false;
	}

	if (bRuneMenuOpen == bOpen)
	{
		// 重复应用是有意设计，调用方可能在布局
		// 创建前调用，也可能在焦点被其他控件夺走后调用
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

void AZCPlayerController::SetInventoryMenuOpen(const bool bOpen)
{
	if (bOpen)
	{
		// 两个菜单共享一个焦点目标和一份暂停租约
		bRuneMenuOpen = false;
	}

	bInventoryMenuOpen = bOpen;
	if (bOpen)
	{
		InitializeInventoryPresentation();
	}
	ApplyRuneMenuPolicy();
}

void AZCPlayerController::ToggleInventoryMenu()
{
	SetInventoryMenuOpen(!bInventoryMenuOpen);
}

void AZCPlayerController::Inventory_Started(const FInputActionValue& Value)
{
	ToggleInventoryMenu();
}

void AZCPlayerController::BindInventoryInput()
{
	if (bInventoryInputBound || !InputComponent)
	{
		return;
	}

	if (!InventoryAction)
	{
		InventoryAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/Game/_Game/Data/Inputs/IA_Inventory.IA_Inventory"));
	}

	if (!InventoryAction)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInput->BindAction(InventoryAction, ETriggerEvent::Started, this,
			&AZCPlayerController::Inventory_Started);
		bInventoryInputBound = true;
	}
}

void AZCPlayerController::InitializeInventoryPresentation()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IsValid(InventoryWidget))
	{
		const TSubclassOf<UZCInventoryWidget> WidgetClass = InventoryWidgetClass
			? InventoryWidgetClass.Get()
			: UZCInventoryWidget::StaticClass();
		InventoryWidget = CreateWidget<UZCInventoryWidget>(this, WidgetClass);
	}

	if (!IsValid(InventoryWidget))
	{
		return;
	}

	if (!InventoryWidget->IsInViewport())
	{
		InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
		InventoryWidget->AddToPlayerScreen(40);
		InventoryWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UZCInventorySubsystem* InventorySubsystem = GameInstance->GetSubsystem<UZCInventorySubsystem>())
		{
			if (IsValid(InventoryDataTable)
				&& (!InventorySubsystem->IsInitialized()
					|| InventorySubsystem->ItemDataTable != InventoryDataTable))
			{
				InventorySubsystem->InitializeFromDataTable(InventoryDataTable, InventoryCapacity);
			}

			InventoryWidget->SetInventorySubsystem(InventorySubsystem);
			InventorySubsystem->OnInventoryChanged.AddUniqueDynamic(
				InventoryWidget, &UZCInventoryWidget::RefreshInventory);
		}
	}

	InventoryWidget->SetDesiredSizeInViewport(InventoryWidget->GetDesiredInventorySize());
	InventoryWidget->SetPositionInViewport(InventoryMargin, false);
	InventoryWidget->SetVisibility(bInventoryMenuOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void AZCPlayerController::ReleaseInventoryPresentation()
{
	if (!IsValid(InventoryWidget))
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UZCInventorySubsystem* InventorySubsystem = GameInstance->GetSubsystem<UZCInventorySubsystem>())
		{
			InventorySubsystem->OnInventoryChanged.RemoveDynamic(
				InventoryWidget, &UZCInventoryWidget::RefreshInventory);
		}
	}

	InventoryWidget->RemoveFromParent();
	InventoryWidget = nullptr;
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
	if (IsValid(InventoryWidget))
	{
		InventoryWidget->SetVisibility(bInventoryMenuOpen
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	const bool bAnyMenuOpen = bRuneMenuOpen || bInventoryMenuOpen;
	if (bAnyMenuOpen && !IsValid(RootLayout) && !IsValid(InventoryWidget))
	{
		// 在存在焦点目标前不要暂停，游戏时间 Timer 不会
		// 在暂停时推进，否则初始化可能死锁
		RequestPresentationInitialization();
		return;
	}

	if (bAnyMenuOpen)
	{
		SetShowMouseCursor(true);

		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		if (bInventoryMenuOpen && IsValid(InventoryWidget))
		{
			InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
		}
		else if (IsValid(RootLayout))
		{
			InputMode.SetWidgetToFocus(RootLayout->TakeWidget());
		}
		SetInputMode(InputMode);

		if (!UGameplayStatics::IsGamePaused(this))
		{
			bPausedByPresentation = SetPause(true);
		}
		return;
	}

	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	if (bPausedByPresentation)
	{
		SetPause(false);
		bPausedByPresentation = false;
	}
}

void AZCPlayerController::ReleasePlayerPresentation(AZCCharBase* PreviousPlayer)
{
	ReleaseTargetLockPresentation(PreviousPlayer);
	ReleaseInventoryPresentation();
	if (IsValid(HeartHealthWidget))
	{
		HeartHealthWidget->SetAttributes(nullptr);
		HeartHealthWidget->SetRuneRuntime(nullptr);
		HeartHealthWidget->RemoveFromParent();
	}
	HeartHealthWidget = nullptr;

	if (bRuneMenuOpen || bInventoryMenuOpen || bPausedByPresentation)
	{
		bRuneMenuOpen = false;
		bInventoryMenuOpen = false;
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
			// 让指示器位于控制器持有的根布局和
			// 符文菜单之上，同时保持为唯一可复用的控件实例
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

	// 同步已经激活的目标，防止 Possess 或表现层
	// 初始化晚于 Pawn 第一次目标变更事件
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
