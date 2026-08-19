// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BombBase.generated.h"

class UStaticMeshComponent;
class UParticleSystem;
class USoundBase;
class UCameraShakeBase;
class URadialForceComponent;

UCLASS()
class ZCASE_API ABombBase : public AActor
{
	GENERATED_BODY()
	
public:	
	ABombBase();

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "Components")
	UStaticMeshComponent* SM;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	UParticleSystem* BombSpawnVFX;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	UParticleSystem* BombExplosionVFX;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	USoundBase* ExplosionSFX;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="VFX")
	TSubclassOf<UCameraShakeBase> ExplosionShake;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field")
	TSubclassOf<AActor> FieldActorClass;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Field")
	URadialForceComponent* RFComp;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Wind")
	TSubclassOf<AActor> WindTunnelClass;

protected:
	virtual void BeginPlay() override;

public:
	void Detonate();
	

};
