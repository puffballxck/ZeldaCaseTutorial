// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/MyInterface.h"
#include "InteractBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AZCCharBase;
/** 交互 Actor 基类，统一处理玩家进入、离开和下一步交互回调 */
UCLASS()
class ZCASE_API AInteractBase : public AActor, public IMyInterface
{
	GENERATED_BODY()
	
public:	
	AInteractBase();

	/** 交互 Actor 的基础网格 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UStaticMeshComponent* BaseMesh;

	/** 检测玩家进入交互范围的球体 */
	UPROPERTY(editAnywhere, BlueprintReadWrite)
	USphereComponent* InteractSphere;

	/** 当前处于交互范围内的玩家 */
	UPROPERTY()
	AZCCharBase* PlayerRef;

	/** 玩家进入交互范围时同时触发蓝图和 C++ 回调 */
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

	/** 玩家离开交互范围时同时触发蓝图和 C++ 回调 */
	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 玩家进入范围后的蓝图扩展点 */
	UFUNCTION(BlueprintImplementableEvent)
	void PlayerEntered();

	/** 玩家进入范围后的 C++ 扩展点 */
	virtual void PlayerEnterCpp();

	/** 玩家离开范围后的蓝图扩展点 */
	UFUNCTION(BlueprintImplementableEvent)
	void PlayerLeft();

	/** 玩家离开范围后的 C++ 扩展点 */
	virtual void PlayerLeftCpp();
	
	/** 执行一次交互动作，供子类覆盖 */
	virtual void ToggleInteraction(AZCCharBase* PlayerRef);

	/** 蓝图侧的交互动作扩展点 */
	UFUNCTION(BlueprintImplementableEvent)
	void ToggleInteractionBP(AZCCharBase* Player);
	
	/** 执行交互 Actor 的下一步动作 */
	virtual void NextActionInInteractionActor();

	/** 接口统一入口，转发到交互 Actor 的下一步动作 */
	virtual void NextAction() override;
	
};
