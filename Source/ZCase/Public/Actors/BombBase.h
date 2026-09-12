// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BombBase.generated.h"

class UStaticMeshComponent;
class UParticleSystem;
class USoundBase;
class UCameraShakeBase;
class URadialForceComponent;

/** 炸弹 Actor，负责引爆表现以及爆炸后的力场和临时风场 */
UCLASS()
class ZCASE_API ABombBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ABombBase();

	/** 炸弹本体网格，同时作为根组件 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "Components")
	UStaticMeshComponent* SM;

	/** 炸弹生成时播放的粒子效果 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	UParticleSystem* BombSpawnVFX;

	/** 爆炸时播放的粒子效果 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	UParticleSystem* BombExplosionVFX;

	/** 爆炸时播放的声音 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	USoundBase* ExplosionSFX;

	/** 爆炸时使用的镜头震动类 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	TSubclassOf<UCameraShakeBase> ExplosionShake;

	/** 爆炸后生成的冲击力场 Actor 类 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field")
	TSubclassOf<AActor> FieldActorClass;

	/** 施加爆炸冲击力的径向力组件 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field")
	URadialForceComponent* RFComp;

	/** 爆炸检测到草地时生成的风场类 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wind")
	TSubclassOf<AActor> WindTunnelClass;

protected:
	/** 生成炸弹时播放生成特效 */
	virtual void BeginPlay() override;

public:
	/** 执行爆炸表现、冲击力、草地风场生成并销毁自身 */
	void Detonate();
	

};
