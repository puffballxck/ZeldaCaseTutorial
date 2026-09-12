// 请在项目设置的说明页面填写版权声明

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
 * 持有本地玩家的表现状态和根 HUD 生命周期
 *
 * RootLayoutClass 现在是首选组合入口，角色的
 * 旧 LayoutClassRef 保留为回退，以便现有 BP_Player 资产继续
 * 工作并逐步迁移默认值
 */
UCLASS()
class ZCASE_API AZCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** 创建或接管被 Possess 玩家角色的根布局，可重复调用 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void InitializePlayerPresentation();

	/** 打开或关闭符文选择菜单，并应用完整输入策略 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void SetRuneMenuOpen(bool bOpen);

	/** 按当前状态切换符文菜单 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void ToggleRuneMenu();

	/** 打开或关闭背包，并应用与符文菜单相同的焦点与暂停策略 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void SetInventoryMenuOpen(bool bOpen);

	/** 按当前状态切换背包菜单 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Player Presentation")
	void ToggleInventoryMenu();

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	bool IsRuneMenuOpen() const { return bRuneMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	UZCLayout* GetRootLayout() const { return RootLayout; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Player Presentation")
	bool IsInventoryMenuOpen() const { return bInventoryMenuOpen; }

protected:
	/** 创建表现层并绑定本地玩家的输入与目标锁定 */
	virtual void BeginPlay() override;
	/** 释放所有 UI、Delegate 和暂停租约 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** 重新接管 Pawn，并迁移表现层绑定 */
	virtual void OnPossess(APawn* InPawn) override;
	/** 解除 Pawn 时先释放旧表现层再交给引擎处理 */
	virtual void OnUnPossess() override;
	/** 绑定控制器独立的背包输入 */
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

	/** 首选根布局类，旧 BP_Player 配置作为回退 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation")
	TSubclassOf<UZCLayout> RootLayoutClass;

	/** 由控制器持有并在每次目标切换时复用的指示器 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation")
	TSubclassOf<UZCTargetLockIndicatorWidget> TargetLockIndicatorClass;

	/** 可选蓝图子类，用于调整心形尺寸和受伤动画；未配置时使用原生默认值 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Health")
	TSubclassOf<UZCHeartHealthWidget> HeartHealthWidgetClass;

	/** DPI 缩放前的左上角边距 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Health")
	FVector2D HeartHealthMargin = FVector2D(40.0f, 40.0f);

	/** 可选的蓝图替代控件，未设置时使用原生控件 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory")
	TSubclassOf<UZCInventoryWidget> InventoryWidgetClass;

	/** 原生或备用背包面板经 DPI 缩放后的左上角位置 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory")
	FVector2D InventoryMargin = FVector2D(40.0f, 180.0f);

	/** 可选的 FZCItemData 数据表，设置后首次显示界面时初始化子系统 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory|Data")
	TObjectPtr<UDataTable> InventoryDataTable;

	/** 背包固定格数量，必须与子系统初始化容量一致 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory|Data", meta = (ClampMin = "1"))
	int32 InventoryCapacity = 6;

	/** 独立的 Enhanced Input 动作，也可以在 BP_Player 中设置，详见 BindInventoryInput */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Player Presentation|Inventory|Input")
	TObjectPtr<UInputAction> InventoryAction;

	/** 当前被 Possess 的本地玩家对应的唯一根控件 */
	UPROPERTY(Transient)
	TObjectPtr<UZCLayout> RootLayout;

	/** 当前为 RootLayout 提供旧 LayoutRef 的角色 */
	UPROPERTY(Transient)
	TObjectPtr<AZCCharBase> PresentedPlayer;

	/** 为本地被 Possess 玩家显示的唯一指示器实例 */
	UPROPERTY(Transient)
	TObjectPtr<UZCTargetLockIndicatorWidget> TargetLockIndicator;

	/** 始终位于菜单之上的三心生命值 HUD */
	UPROPERTY(Transient)
	TObjectPtr<UZCHeartHealthWidget> HeartHealthWidget;

	/** 由控制器持有并在打开与关闭之间复用的唯一背包控件 */
	UPROPERTY(Transient)
	TObjectPtr<UZCInventoryWidget> InventoryWidget;

	TWeakObjectPtr<AZCCharBase> BoundTargetLockPlayer;
	TWeakObjectPtr<UZCTargetLockComponent> BoundTargetLock;

	/** 符文菜单是否占用输入焦点并持有暂停租约 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZCase|Player Presentation", meta = (AllowPrivateAccess = "true"))
	bool bRuneMenuOpen = false;

	/** 背包菜单是否占用输入焦点并持有暂停租约 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "ZCase|Player Presentation", meta = (AllowPrivateAccess = "true"))
	bool bInventoryMenuOpen = false;

	/** 仅当本控制器负责暂停游戏时才为 true */
	bool bPausedByPresentation = false;

	bool bPresentationInitializationPending = false;
	bool bInventoryInputBound = false;
	FTimerHandle PresentationInitializationTimer;
	
};
