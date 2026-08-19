// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TimelineComponent.h"
#include "IceActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UCurveFloat;

UCLASS()
class ZCASE_API AIceActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AIceActor();

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	USceneComponent* BaseSceneRoot;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UStaticMeshComponent* IceMesh;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UBoxComponent* CheckOverlapComp;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UBoxComponent* SolidBoxComp;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<AIceActor> SpawnClass;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UCurveFloat* CollisionCurve = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UTimelineComponent* CollisionTimeline;

	FVector	ExtentStart;//盒型碰撞体边长的起始和终止
	FVector ExtentEnd;
	FVector RelativeStart;//中心位置
    FVector RelativeEnd;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UCurveFloat* VisualCubeCurve = nullptr;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UTimelineComponent* VisualCubeTimeline;

	FVector	WorldScaleStart;//可视冰柱边长的起始和终止
	FVector WorldScaleEnd;
	
	bool bCanPlace = false;

public:	
	virtual void BeginPlay() override;

	bool CheckOverlap();

	void EnableCollision();

	void SpawnIce();

	UFUNCTION()
	void CollisionUpdate(float DeltaTime);

    //测试
	UFUNCTION()
	void CollisionFinished();

	UFUNCTION()
	void VisualCubeUpdate(float DeltaTime);

    void StartPlayAnimationLoop();
	void StopPlayAnimation();
	

};






