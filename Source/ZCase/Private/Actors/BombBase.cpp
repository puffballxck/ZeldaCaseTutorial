// 请在项目设置的说明页面填写版权声明

#include "Actors/BombBase.h"
#include "Actors/WindTunnel.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/RadialForceComponent.h"

ABombBase::ABombBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SM = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bomb Mesh"));
	SetRootComponent(SM);
	SM->SetLinearDamping(0.35f);//设置线性阻尼
	SM->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
	SM->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);

	RFComp = CreateDefaultSubobject<URadialForceComponent>("radialForce");
	RFComp->SetupAttachment(SM);
	RFComp->Radius = 300.0f;
	RFComp->ImpulseStrength = 3000.0f;
	RFComp->bImpulseVelChange = true;
	RFComp->ForceStrength = 100.0f;
}

void ABombBase::BeginPlay()
{
	Super::BeginPlay();
	//生成炸弹特效
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),BombSpawnVFX,GetActorLocation());
	
}

void ABombBase::Detonate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	//引爆炸弹
	//播放爆炸特效
	UGameplayStatics::SpawnEmitterAtLocation(World,BombExplosionVFX,SM->GetComponentLocation());
	
	//播放声音
	UGameplayStatics::PlaySoundAtLocation(World,ExplosionSFX,SM->GetComponentLocation());

	//镜头震动
	if (ExplosionShake)
	{
		APlayerCameraManager::PlayWorldCameraShake(
			World,ExplosionShake,SM->GetComponentLocation(),0.0f,
			2000.0f,0.5f,false);
	}

	//炸碎静态网格体冲击力
	if (FieldActorClass)
	{
		World->SpawnActor<AActor>(FieldActorClass, SM->GetComponentLocation(), FRotator::ZeroRotator);
	}
	RFComp->FireImpulse();//触发爆炸
	
	//爆炸在草地上则产生风场,此处检测材质
	FVector Start = SM->GetComponentLocation();
	FVector End = Start;
	TArray<FHitResult> OutResults;
	FCollisionShape MySphere = FCollisionShape::MakeSphere(200.0f);
	FCollisionQueryParams Params;
	Params.bReturnPhysicalMaterial = true;//重要
	World->SweepMultiByChannel(OutResults,Start,End,FQuat::Identity,
		ECC_Visibility,MySphere,Params);

	bool bSpawnWind = false;
	for (auto ArrayElem : OutResults)
	{
		if (ArrayElem.bBlockingHit && ArrayElem.PhysMaterial != nullptr)
		{
			switch (UGameplayStatics::GetSurfaceType(ArrayElem))
			{
			case SurfaceType1:
				break;
			case SurfaceType2:
				//发现草地
				bSpawnWind = true;
				break;
			}
		}
	}

	if (bSpawnWind && WindTunnelClass)
	{
		//生成的是临时风场，需将风场中的bTemporaryWT改为true
		//C++中用延时生成函数
		FVector WTLocation = FVector(SM->GetComponentLocation().X,SM->GetComponentLocation().Y,SM->GetComponentLocation().Z+800.0f);
		FTransform CustomTransform(FRotator(0,0,0),
			WTLocation,FVector(4,4,4));
		if (AWindTunnel* WT = World->SpawnActorDeferred<AWindTunnel>(WindTunnelClass,CustomTransform))
		{
			//初始化TemporaryWT
			WT->bTemporaryWT = true;
			WT->InitialLifeSpan = 5.0f;
			//再完全生成该Actor
			WT->FinishSpawning(CustomTransform);
		}
	}
	
	//销毁自身
	Destroy();
}

