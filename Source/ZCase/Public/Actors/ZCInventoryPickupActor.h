#pragma once

#include "CoreMinimal.h"
#include "Actors/InteractBase.h"
#include "Inventory/ZCInventoryTypes.h"
#include "ZCInventoryPickupActor.generated.h"

class UAnimMontage;

/** 入包拾取物；与举起或投掷物体的 PickupActor 分开 */
UCLASS()
class ZCASE_API AZCInventoryPickupActor : public AInteractBase
{
	GENERATED_BODY()

	public:
	AZCInventoryPickupActor();
	/** 播放拾取动画，并在动画结束后提交物品到背包 */
	virtual void ToggleInteraction(AZCCharBase* Character) override;

	/** 要加入背包的数据表物品编号 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 ItemId = 0;

	/** 本次拾取的物品数量 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Amount = 1;

	/** 播放拾取动作的 Montage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UAnimMontage> PickupMontage;

	/** 在蓝图中显示背包已满或物品配置错误等提示 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void PickupFailed(EZCInventoryResult Result);

private:
	/** 处理拾取 Montage 结束，并在未中断时提交背包变更 */
	void HandlePickupEnded(UAnimMontage* Montage, bool bInterrupted);
	/** 防止拾取动作重复进入 */
	bool bPickupInProgress = false;
};
