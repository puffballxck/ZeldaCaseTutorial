#include "Actors/InteractBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

AInteractBase::AInteractBase()
{
	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>("BaseMesh");
	SetRootComponent(BaseMesh);

	InteractSphere = CreateDefaultSubobject<USphereComponent>("InteractSphere");
	InteractSphere->SetupAttachment(BaseMesh);
	InteractSphere->SetSphereRadius(150.0f);
	InteractSphere->OnComponentBeginOverlap.AddDynamic(this,&AInteractBase::OnBeginOverlap);
	InteractSphere->OnComponentEndOverlap.AddDynamic(this,&AInteractBase::OnEndOverlap);
	
}

void AInteractBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (IsValid(OtherActor) && OtherActor->ActorHasTag(FName("tag_player")))
	{
		//检查是否是玩家
		PlayerEntered();
		PlayerEnterCpp();
	}
}

void AInteractBase::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (IsValid(OtherActor) && OtherActor->ActorHasTag(FName("tag_player")))
	{
		//检查是否是玩家
		PlayerLeft();
		PlayerLeftCpp();
	}
}

void AInteractBase::PlayerEnterCpp()
{
	
}

void AInteractBase::PlayerLeftCpp()
{
	
}

void AInteractBase::NextActionInInteractionActor()
{
	
}

void AInteractBase::NextAction()
{
	NextActionInInteractionActor();
}

void AInteractBase::ToggleInteraction(AZCCharBase* playerRef)
{
	
}


