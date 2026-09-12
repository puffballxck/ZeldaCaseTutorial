// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PressureSwitch.generated.h"

class USceneComponent;
class UBoxComponent;
class UStaticMeshComponent;
/** 压力开关 Actor，提供可被重叠检测的开关组件 */
UCLASS()
class ZCASE_API APressureSwitch : public AActor
{
	GENERATED_BODY()
	
public:	
	APressureSwitch();

	/** 开关的根场景组件 */
	UPROPERTY()
	USceneComponent* SceneRoot;

	/** 检测施压物体的盒体，仅产生重叠事件 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UBoxComponent* BoxCollider;

	/** 开关上方可替换的活动网格 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UStaticMeshComponent* Switcher;

	/** 压力开关底座网格 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UStaticMeshComponent* Base;
	
};
