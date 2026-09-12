// 版权所有 Epic Games, Inc，保留所有权利

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

/** 原生三心生命条，每颗心代表两个半心单位 */
UCLASS()
class ZCASE_API UZCHeartHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UZCHeartHealthWidget(const FObjectInitializer& ObjectInitializer);

	/** 将控件绑定到属性组件并刷新生命条，不播放受伤闪烁 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|UI|Health")
	void SetAttributes(UZCAttributeComponent* InAttributes);

	/** 将生命条下方常驻的符文图标绑定到玩家选择状态 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|UI|Runes")
	void SetRuneRuntime(UZCRuneRuntimeComponent* InRuneRuntime);

	/** 返回当前显示的半心单位数量，范围为 0 到 6 */
	UFUNCTION(BlueprintPure, Category = "ZCase|UI|Health")
	int32 GetDisplayedHalfHearts() const { return DisplayedHalfHearts; }

	/** 返回控制器应为该生命条预留的视口尺寸 */
	UFUNCTION(BlueprintPure, Category = "ZCase|UI|Health")
	FVector2D GetHeartBarSize() const;

	/** 原生心形行中单个心形缩略图的尺寸 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "1.0"))
	FVector2D HeartSize = FVector2D(56.0f, 52.0f);

	/** 相邻心形缩略图之间的水平间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	float HeartSpacing = 6.0f;

	/** 三心行周围的内边距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	FVector2D HeartBarPadding = FVector2D(5.0f, 10.0f);

	/** 一次白色半心受伤闪烁的持续时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.01"))
	float FlashDuration = 0.4f;

	/** 受伤闪烁淡出时在屏幕空间上升的距离 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	float FlashRiseDistance = 8.0f;

	/** 受伤闪烁在动画中段的最大缩放膨胀值 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Health", meta = (ClampMin = "0.0"))
	float FlashScaleAmount = 0.06f;

	/** 生命条下方选中符文图标的尺寸 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Runes", meta = (ClampMin = "1.0"))
	FVector2D RuneIconSize = FVector2D(64.0f, 64.0f);

	/** 生命条与选中符文图标之间的间距 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|UI|Runes", meta = (ClampMin = "0.0"))
	float RuneIconGap = 8.0f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** 单个半心闪烁实例的播放进度与激活状态 */
	struct FHalfFlashState
	{
		/** 当前闪烁已经播放的时间，单位为秒 */
		float Elapsed = 0.0f;
		/** 该半心是否正在播放受伤闪烁 */
		bool bActive = false;
	};

	static constexpr int32 HeartCount = 3;
	static constexpr int32 HalfHeartCount = 6;

	/** 在 Slate 根控件创建前补齐原生生命条布局 */
	void EnsureWidgetTree();
	/** 创建所有半心覆盖层共享的柔光纹理 */
	void CreateFlashTexture();
	/** 从属性组件读取生命值并决定是否触发受伤表现 */
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

	/** 由设计器提供的根容器，WBP_HeartHealth 必须提供该名称的控件 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> RootSizeBox;

	/** 由设计器提供的画布，供原生代码更新生命与闪烁图片 */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> HeartCanvas;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeartImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> FlashImages;

	/** 由设计器提供的选中符文图标，其布局由 WBP_HeartHealth 控制 */
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

	/** 一个由代码生成并供六个半心覆盖层复用的柔光纹理 */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FlashTexture;

	TArray<FHalfFlashState> FlashStates;
	/** 当前正在播放的半心闪烁数量 */
	int32 ActiveFlashCount = 0;
	/** 最近一次刷新后应显示的半心数量 */
	int32 DisplayedHalfHearts = HalfHeartCount;
	/** 是否已经收到过有效生命值快照 */
	bool bHasDisplayedHealth = false;
	/** 是否已经建立原生 WidgetTree，避免重复创建 */
	bool bWidgetTreeBuilt = false;
};
