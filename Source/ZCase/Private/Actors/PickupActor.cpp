// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/PickupActor.h"
#include "Blueprint/UserWidget.h"
#include "Characters/ZCCharBase.h"
#include "Components/SphereComponent.h"

APickupActor::APickupActor()
{
	
}

void APickupActor::PlayerEnterCpp()
{
	//创建UI提示
	Super::PlayerEnterCpp();
	if (HintClass == nullptr) return;
	HintUI = CreateWidget<UUserWidget>(GetWorld(),HintClass);
    HintUI->AddToViewport();
}

void APickupActor::PlayerLeftCpp()
{
	//消除UI提示
	Super::PlayerLeftCpp();
	if (HintUI == nullptr) return;
	HintUI->RemoveFromParent();
	HintUI = nullptr;
}

void APickupActor::ToggleInteraction(AZCCharBase* playerRef)
{
	Super::ToggleInteraction(playerRef);
	if (playerRef == nullptr) return;
	PlayerRef = playerRef;

	RemoveHintAndSetSphereInvalid();

	//举起物体后物理模拟设为假
	SetActorEnableCollision(false);
	BaseMesh->SetSimulatePhysics(false);
	BaseMesh->AttachToComponent(playerRef->HeadPos,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	PlayerRef->CrossHairAndCameraMode(true);
	PlayerRef->bReadyToThrow = true;
}

void APickupActor::NextActionInInteractionActor()
{
	Super::NextActionInInteractionActor();
	//放下抬起的物品
	//if (PlayerRef == nullptr) return;
	if (!PlayerRef) return;

	// 解除附着的物理设置
	BaseMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    
	// 启动物理模拟
	BaseMesh->SetSimulatePhysics(true); 
	SetActorEnableCollision(true);
	// 放置到角色脚前
	FVector DropLocation = PlayerRef->DropPos->GetComponentLocation();
	SetActorLocation(DropLocation);

	ResetSphereAfterThrowing();
	// 重置角色状态
	PlayerRef->CrossHairAndCameraMode(false);
	PlayerRef->bReadyToThrow = false;
	PlayerRef = nullptr;

	
	

	
	
	//PlayerRef->ReadyToThrow(BaseMesh);
	//SetActorEnableCollision(true);
	//ResetSphereAfterThrowing();
	//PlayerRef->bReadyToThrow = false;
	//PlayerRef = nullptr;
}

void APickupActor::RemoveHintAndSetSphereInvalid()
{
	//拿起的时候隐藏UI提示
	if (HintUI == nullptr) return;
	HintUI->RemoveFromParent();
	HintUI = nullptr;

	InteractSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APickupActor::ResetSphereAfterThrowing()
{
	//延迟2秒后重置Sphere
	GetWorld()->GetTimerManager().SetTimer(DelayRestSphereHandle,this,
		&APickupActor::ResetSphere,2.0f,false);
	
}

void APickupActor::ThrowObject(const FVector& ThrowForce)
{
	// 解除附着并启动物理
	BaseMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	BaseMesh->SetSimulatePhysics(true);
	SetActorEnableCollision(true);
	// 施加投掷力
	BaseMesh->AddImpulse(ThrowForce, NAME_None, true);
	// 重置 Sphere 状态
	ResetSphereAfterThrowing();
	// 清空角色引用
	if (PlayerRef)
	{
		PlayerRef->CrossHairAndCameraMode(false);
		PlayerRef->bReadyToThrow = false;
		PlayerRef = nullptr;
	}

}

void APickupActor::NextAction()
{
	Super::NextAction();
	//NextActionInInteractionActor();
}

void APickupActor::ResetSphere()
{
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}
