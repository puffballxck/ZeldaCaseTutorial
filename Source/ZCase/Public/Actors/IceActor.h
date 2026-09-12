// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "IceActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UCurveFloat;

/** 可生长的冰柱 Actor，负责放置检测、碰撞启用和外观动画 */
UCLASS()
class ZCASE_API AIceActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AIceActor();

	/** 冰柱组件的根节点 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	USceneComponent* BaseSceneRoot;

	/** 冰柱可视网格 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UStaticMeshComponent* IceMesh;

	/** 用于检测新冰柱是否与静态物体重叠的盒体 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UBoxComponent* CheckOverlapComp;

	/** 冰柱实际阻挡其他物体的盒体 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UBoxComponent* SolidBoxComp;

	/** 继续生成冰柱时使用的 Actor 类 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<AIceActor> SpawnClass;

	/** 控制碰撞体生长的时间轴曲线 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UCurveFloat* CollisionCurve = nullptr;

	/** 驱动碰撞体生长的时间轴 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UTimelineComponent* CollisionTimeline;

	/** 盒型碰撞体边长的起始和终止值 */
	FVector	ExtentStart;
	FVector ExtentEnd;
	/** 碰撞体相对位置的起始和终止值 */
	FVector RelativeStart;
    FVector RelativeEnd;
	
	/** 控制可视冰柱生长的时间轴曲线 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UCurveFloat* VisualCubeCurve = nullptr;
	
	/** 驱动可视冰柱生长的时间轴 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UTimelineComponent* VisualCubeTimeline;

	/** 可视冰柱缩放的起始和终止值 */
	FVector	WorldScaleStart;
	FVector WorldScaleEnd;
	
	/** 当前冰柱位置是否允许生成 */
	bool bCanPlace = false;

public:	
	/** 生成后绑定时间轴回调并计算生长范围 */
	virtual void BeginPlay() override;

	/** 检测冰柱位置是否与静态物体重叠 */
	bool CheckOverlap();

	/** 启用实体碰撞并播放生长动画 */
	void EnableCollision();

	/** 在允许的位置生成下一根冰柱 */
	void SpawnIce();

	/** 更新碰撞体尺寸和位置 */
	UFUNCTION()
	void CollisionUpdate(float DeltaTime);

	/** 碰撞生长时间轴结束回调 */
	UFUNCTION()
	void CollisionFinished();

	/** 更新可视网格缩放 */
	UFUNCTION()
	void VisualCubeUpdate(float DeltaTime);

    /** 循环播放用于预览放置状态的外观动画 */
    void StartPlayAnimationLoop();
	/** 停止预览放置状态的外观动画 */
	void StopPlayAnimation();
	

};






