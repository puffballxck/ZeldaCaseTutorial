// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Actors/InteractBase.h"
#include "PickupActor.generated.h"

/** 可被玩家举起、投掷并在短暂延迟后恢复交互的物体 */
UCLASS()
class ZCASE_API APickupActor : public AInteractBase
{
	GENERATED_BODY()

public:
	APickupActor();

	/** 玩家靠近物体时显示的提示控件类 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> HintClass;



	/** 当前显示的交互提示控件 */
	UPROPERTY()
	UUserWidget* HintUI;

	/** 投掷或放下后恢复交互球体的延迟计时器 */
	FTimerHandle DelayRestSphereHandle;

protected:
	/** 创建交互提示 */
	virtual void PlayerEnterCpp() override;
	/** 移除交互提示 */
	virtual void PlayerLeftCpp() override;

	/** 将物体附着到玩家头部并进入可投掷状态 */
	virtual void ToggleInteraction(AZCCharBase* playerRef) override;
	/** 放下当前举起的物体 */
	virtual void NextActionInInteractionActor() override;

public:
	/** 移除提示并暂时关闭交互检测 */
	void RemoveHintAndSetSphereInvalid();
	/** 投掷或放下后延迟恢复交互检测 */
	void ResetSphereAfterThrowing();
	/** 解除附着、施加投掷冲量并清理玩家状态 */
	void ThrowObject(const FVector& ThrowForce);

	/** 执行交互接口的下一步动作 */
	virtual void NextAction() override;

private:
	/** 恢复交互球体的查询碰撞 */
	void ResetSphere();
	
};
