// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StaticActor.generated.h"

class UArrowComponent;
/** 可被多次施力的静态物体，提供方向、力度和受击次数信息 */
UCLASS()
class ZCASE_API AStaticActor : public AActor
{
	GENERATED_BODY()

public:	
	AStaticActor();

	/** 显示受力方向并作为根组件的箭头 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UArrowComponent* IndicatorArrow;

	/** 当前每次受力的冲量强度 */
	int32 Impulse = 0;
	/** 已累计施加力的次数 */
	int32 Hits = 0;

	/** 返回当前累计受力后的冲量方向与大小 */
	FVector GteImpulse();

	/** 根据受力次数更新箭头尺寸、颜色和冲量 */
	void UpdateForceInfo();

	/** 返回受力方向箭头组件 */
	FORCEINLINE  UArrowComponent* GetArrowComponent() const{ return IndicatorArrow; }
	
};
