// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/WindTunnel.h"
#include "Characters/ZCCharBase.h"
#include "Components/BoxComponent.h"


AWindTunnel::AWindTunnel()
{
	PrimaryActorTick.bCanEverTick = true;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("BOX"));
	SetRootComponent(Box);
	//绑定回调事件
	Box->OnComponentBeginOverlap.AddDynamic(this, &AWindTunnel::OnOverlapBegin);
	Box->OnComponentEndOverlap.AddDynamic(this, &AWindTunnel::OnOverlapEnd);

}

void AWindTunnel::BeginPlay()
{
	Super::BeginPlay();

	if (bTemporaryWT)
	{
		//InitialLifeSpan在BeginPlay后无效，必须使用SpawnActorDeferred指令
		//InitialLifeSpan = 30.0f;
	}
	
}

void AWindTunnel::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!IsValid(PlayerRef))
	{
		PlayerRef = nullptr;
		return;
	}

	if (PlayerRef->CurrentMT != EMovementTypes::MT_Gliding) return;

    FVector LocalUpVector = GetActorUpVector() *20.0f; //代表相对向上的变量
	PlayerRef->AddActorWorldOffset(LocalUpVector);
}
void AWindTunnel::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AZCCharBase* Character = Cast<AZCCharBase>(OtherActor))
	{
		PlayerRef = Character;
		PlayerRef->bIsInWindTunnel = true;
	}
	
}

void AWindTunnel::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AZCCharBase* Character = Cast<AZCCharBase>(OtherActor))
	{
		Character->bIsInWindTunnel = false;
		if (PlayerRef == Character)
		{
			PlayerRef = nullptr;
		}
	}
	//PlayerRef->bIsInWindTunnel = false;
	//PlayerRef = nullptr;
}

