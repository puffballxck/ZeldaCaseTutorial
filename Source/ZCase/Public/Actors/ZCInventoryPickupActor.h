#pragma once

#include "CoreMinimal.h"
#include "Actors/InteractBase.h"
#include "Inventory/ZCInventoryTypes.h"
#include "ZCInventoryPickupActor.generated.h"

class UAnimMontage;

/** 入包拾取物；与举起/投掷物体的 PickupActor 分开。 */
UCLASS()
class ZCASE_API AZCInventoryPickupActor : public AInteractBase
{
	GENERATED_BODY()

public:
	AZCInventoryPickupActor();
	virtual void ToggleInteraction(AZCCharBase* Character) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 ItemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Amount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UAnimMontage> PickupMontage;

	/** 可在蓝图中显示背包已满或物品配置错误等提示。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void PickupFailed(EZCInventoryResult Result);

private:
	void HandlePickupEnded(UAnimMontage* Montage, bool bInterrupted);
	bool bPickupInProgress = false;
};
