// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindTunnel.generated.h"

class AZCCharBase;
class UBoxComponent;
/** 风场 Actor，在玩家滑翔时持续施加向上的位移 */
UCLASS()
class ZCASE_API AWindTunnel : public AActor
{
	GENERATED_BODY()
	
public:	
	AWindTunnel();
	/** 是否为爆炸后生成并自动销毁的临时风场 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	bool bTemporaryWT = false;

	/** 当前处于风场中的玩家 */
	UPROPERTY(EditAnywhere)
	AZCCharBase* PlayerRef = nullptr;

	/** 风场范围盒体 */
	UPROPERTY(EditAnywhere)
	UBoxComponent* Box;

protected:
	/** 初始化临时风场生命周期相关状态 */
	virtual void BeginPlay() override;

public:	
	/** 在玩家滑翔时按风场方向推动玩家 */
	virtual void Tick(float DeltaTime) override;

	/** 记录进入风场的玩家 */
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult & SweepResult);
	
	/** 清除离开风场的玩家引用 */
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


};



