// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/StaticActor.h"
#include "Components/ArrowComponent.h"
#include "Kismet/KismetMathLibrary.h"

AStaticActor::AStaticActor()
{
	PrimaryActorTick.bCanEverTick = false;

	IndicatorArrow = CreateDefaultSubobject<UArrowComponent>("Indicator Arrow");
	SetRootComponent(IndicatorArrow);
	IndicatorArrow->bHiddenInGame = false;
	
}

FVector AStaticActor::GteImpulse()
{
	//把箭头方向转换成向量
	FVector ArrowForce = UKismetMathLibrary::Conv_RotatorToVector(IndicatorArrow->GetRelativeRotation());

	return ArrowForce * Impulse;
}

void AStaticActor::UpdateForceInfo()
{
	//设置箭头的视角大小和颜色

	Hits = FMath::Clamp(Hits +1 , 0 ,5);
	FVector tempScale = FVector(Hits+1, 1.0f,1.0f);
	IndicatorArrow->SetRelativeScale3D(tempScale);

	float tempScaleX = IndicatorArrow->GetRelativeScale3D().X;
	float AlphaColor = UKismetMathLibrary::MapRangeClamped(tempScaleX,1.0f,5.0f,0.0f,1.0f);
	FLinearColor tempColor = FLinearColor::LerpUsingHSV(FLinearColor::Yellow,FLinearColor::Red,AlphaColor);
	IndicatorArrow->SetArrowColor(tempColor);

	//更新Impules变量的值
	Impulse = Hits *2500;
}


