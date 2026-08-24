// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "ZCCombatComponent.generated.h"

class UAnimMontage;
class USkeletalMeshComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EZCWeaponState : uint8
{
	Sheathed,
	Drawing,
	Equipped,
	Attacking,
	Sheathing
};

UENUM(BlueprintType)
enum class EZCWeaponCommand : uint8
{
	None,
	Draw,
	Attack
};

UENUM(BlueprintType)
enum class EZCWeaponAttachmentState : uint8
{
	Sheathed,
	Equipped
};

/** Owns the lifetime and hit de-duplication rules for one attack at a time. */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCCombatComponent();

	void InitializeEquipment(
		USkeletalMeshComponent* InCharacterMesh,
		UStaticMeshComponent* InSwordMesh,
		UStaticMeshComponent* InSheathMesh,
		UStaticMeshComponent* InShieldMesh);

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	bool HandleAttackInput();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	bool RequestSheath();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	void SetEquipmentAttachmentState(EZCWeaponAttachmentState AttachmentState);

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Weapon")
	EZCWeaponState GetWeaponState() const { return WeaponState; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Weapon")
	bool IsWeaponEquippedForAnimation() const;

	static EZCWeaponCommand ResolveAttackCommand(EZCWeaponState State);

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void StartAttack();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool BeginTrace();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void EndTrace();

	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool TryApplyHit(AActor* Target, float DamageAmount);

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	bool IsTraceActive() const { return bTraceActive; }

private:
	bool StartDraw();
	bool StartWeaponAttack();
	bool PlayMontage(UAnimMontage* Montage, void (UZCCombatComponent::*EndCallback)(UAnimMontage*, bool));
	void HandleDrawMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleSheathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void ScheduleAutoSheath();
	void ClearAutoSheathTimer();
	void HandleAutoSheathElapsed();
	void FinishAttack();

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> DrawSwordMontage;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> SheathSwordMontage;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon", meta = (ClampMin = "0.1"))
	float AutoSheathDelay = 5.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Combat|Weapon")
	EZCWeaponState WeaponState = EZCWeaponState::Sheathed;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SwordMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SheathMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName WeaponHandSocket = TEXT("WeaponHand_R");

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName WeaponSheathSocket = TEXT("WeaponSheath");

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName ShieldHandSocket = TEXT("ShieldHand_L");

	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName ShieldBackSocket = TEXT("ShieldBack");

	FTimerHandle AutoSheathTimerHandle;
	bool bAttackActive = false;
	bool bTraceActive = false;
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
