// 请在项目设置的说明页面填写版权声明


#include "Actors/PressureSwitch.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"


APressureSwitch::APressureSwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>("SceneRoot");
	SetRootComponent(SceneRoot);

	BoxCollider = CreateDefaultSubobject<UBoxComponent>("BoxCollider");
	BoxCollider->SetupAttachment(SceneRoot);
	BoxCollider->SetCollisionResponseToAllChannels(ECR_Overlap);//通道设为仅检测

	Switcher = CreateDefaultSubobject<UStaticMeshComponent>("Switcher");
	Switcher->SetupAttachment(SceneRoot);

	Base = CreateDefaultSubobject<UStaticMeshComponent>("Base");
	Base->SetupAttachment(SceneRoot);

}

