// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Combat/ZCTargetable.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ZCCharBase.generated.h"

class AIceActor;
//前置声明
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UZCLayout;
class USkeletalMeshComponent;
class ABombBase;
class USceneComponent;
class AStaticMeshActor;
class UPhysicsHandleComponent;
class UParticleSystem;
class AStaticActor;
class AInteractBase;
class UZCRuneRuntimeComponent;
class UZCAttributeComponent;
class UZCCombatComponent;
class UZCTargetLockComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FZCStaminaChangedSignature,
	float, CurrentStamina,
	float, MaximumStamina,
	bool, bExhausted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCMovementTypeChangedSignature,
	EMovementTypes, PreviousMovement,
	EMovementTypes, CurrentMovement);

UCLASS()
class ZCASE_API AZCCharBase : public ACharacter, public IZCTargetable
{
	GENERATED_BODY()

public:
	AZCCharBase();

#pragma region	Variables
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Comps")
	USpringArmComponent* CameraBoom;
	//TObjectPtr<USpringArmComponent> CameraBoom; 等价
	
	UPROPERTY(EditAnywhere,Category="Comps")
	UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere,Category="Comps")
	UPhysicsHandleComponent* PhysicsHandle;

	UPROPERTY(EditAnywhere,Category="Comps")
	USceneComponent* PhysicsObjectHolder;

	UPROPERTY(EditAnywhere,Category="Comps")
	USkeletalMeshComponent* Parachute;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Cloth")
	USkeletalMeshComponent* Hair;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Cloth")
	USkeletalMeshComponent* Upper;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Cloth")
	USkeletalMeshComponent* Lower;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Cloth")
	UPhysicsAsset* HairPhysicsAsset;

	UPROPERTY(EditAnywhere,Category="Comps")
	USceneComponent* HeadPos;

	UPROPERTY(EditAnywhere,Category="Comps")
	USceneComponent* DropPos;
	
	UPROPERTY(EditAnywhere,Category="Inputs")
	UInputMappingContext* IMC_ZC;
	
	UPROPERTY(EditAnywhere,category="Inputs")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere,category="Inputs")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere,category="Inputs")
	UInputAction* SprintAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* JumpGlideAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* ToggleUIAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* ActiveRuneAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* ReleaseRuneAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* InteractAction;

	/** Enhanced Input 的攻击动作，接线后交给 Combat 组件按武器状态解释。 */
	UPROPERTY(EditAnywhere, Category="Inputs")
	UInputAction* AttackAction;

	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,category="Movements")
	EMovementTypes CurrentMT{ EMovementTypes ::MT_EMAX};

	UPROPERTY(EditAnywhere,BlueprintReadWrite,category="Rune | Magnesis")
	TSubclassOf<AActor> StaticMeshClass;

	UPROPERTY()
	TArray<AStaticMeshActor*> AllMagSMs;//存放当前场景中所有可吸附物体

	UPROPERTY()
	UPrimitiveComponent* MagnesisObj = nullptr;
	UPROPERTY()
	UPrimitiveComponent* TempMagHitComp = nullptr;//更改磁铁吸附材质用

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,category="Rune | Magnesis")
	UMaterialInterface* MagHovered = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,category="Rune | Magnesis")
	UMaterialInterface* MagNormal = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,category="Rune | Magnesis")
	UMaterialInterface* MagDeactivated = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,category="Rune | Magnesis")
	UParticleSystem* MagDraggingVFX;

	UPROPERTY()
	AIceActor* IceRef = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,category="Rune | Ice")
	TSubclassOf<AActor> IceActorClass;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,category="Rune | Ice")
	UMaterialInterface* IceDisabled = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,category="Rune | Ice")
	UMaterialInterface* IceEnabled = nullptr;

	UPROPERTY()
	UMaterialInterface* OriginMatStasis = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,category="Rune | Stasis")
	UMaterialInterface* StasisMat = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,category="Rune | Stasis")
	TSubclassOf<AActor> StasisClass;//是一个类

	UPROPERTY()
	UPrimitiveComponent* StasisComp;

	FTimerHandle StasisTimerHandle;

	UPROPERTY()
	AStaticActor* StasisForceActor;

	UPROPERTY()
	UParticleSystemComponent* BeamParticleComp;
	
	UPROPERTY()
	AInteractBase* InteractingActor;
	
	bool bReadyToThrow = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runes")
	TObjectPtr<UZCRuneRuntimeComponent> RuneRuntime;

	/** 负责生命值、伤害钳制和一次性死亡事件。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UZCAttributeComponent> Attributes;

	/** 负责武器状态机、攻击命中窗口和装备挂点。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UZCCombatComponent> Combat;

	/** 可切换到手部或剑鞘挂点的剑网格。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Equipment")
	TObjectPtr<UStaticMeshComponent> SwordMesh;

	/** 始终附着在背部剑鞘挂点的剑鞘网格。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Equipment")
	TObjectPtr<UStaticMeshComponent> SheathMesh;

	/** 在左手盾牌挂点与背部挂点之间切换的盾牌网格。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Equipment")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	/** 持有并验证当前目标锁定对象。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target Lock")
	TObjectPtr<UZCTargetLockComponent> TargetLock;
	
    bool bRBActivated = false;//炸弹是否被激活
	bool bMagActivated = false;//磁铁是否被激活
	bool bStasisActivated = false;
	bool bIceActivated = false;

	UPROPERTY()
    ABombBase* BombRef;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Runes")
	TSubclassOf<ABombBase> SphereBomb;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Runes")
	TSubclassOf<ABombBase> CubeBomb;
	
	float Vel_X;
	float Vel_Y;

	bool bFlipflopCrosshair = false;
	
	bool bHoldingBomb = false;
	bool bSphereBomb = false;

	
#pragma endregion

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> LayoutClassRef;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="UI")
	UZCLayout* LayoutRef;

	UPROPERTY(EditDefaultsOnly,category="Movements")
	FVector EnableGlideDistance = FVector(0.0f, 0.0f, 150.0f);

	UPROPERTY(BlueprintAssignable, Category="Movements")
	FZCMovementTypeChangedSignature OnMovementTypeChanged;

	bool bIsInWindTunnel = false; // 标识是否在风场中


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Landed(const FHitResult& Hit) override;

#pragma region Inputs Node
	UFUNCTION()
	void Move_Triggered(const FInputActionValue& val);

	UFUNCTION()
	void Move_Completed(const FInputActionValue& val);

	UFUNCTION()
	void Look_Triggered(const FInputActionValue& val);

	UFUNCTION()
	void Sprint_Triggered(const FInputActionValue& val);

	UFUNCTION()
	void Sprint_Started(const FInputActionValue& val);

	UFUNCTION()
	void Sprint_Completed(const FInputActionValue& val);

	UFUNCTION()
	void JumpGlide_Started(const FInputActionValue& val);

	UFUNCTION()
	void JumpGlide_Completed(const FInputActionValue& val);

	UFUNCTION()
	void ToggleUI_Started(const FInputActionValue& val);

	UFUNCTION()
	void ActiveRune_Started(const FInputActionValue& val);

	UFUNCTION()
	void ReleaseRune_Started(const FInputActionValue& val);

	UFUNCTION()
	void Interact_Started(const FInputActionValue& val);

	/** Enhanced Input 的攻击 Started 回调；仅把输入转发给战斗组件。 */
	UFUNCTION()
	void Attack_Started(const FInputActionValue& val);
	
#pragma endregion
	
public:	
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 将引擎伤害先交给父类，再由 Attributes 统一处理生命值和死亡状态。 */
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	/** 死亡角色不再允许新的目标锁定。 */
	virtual bool CanBeTargetLocked() const override;
	/** 返回目标锁定使用的角色世界位置。 */
	virtual FVector GetTargetLockLocation() const override;
	
#pragma region Locomotion
	UFUNCTION()
	void LocomotionManager(EMovementTypes NewMovement);

    void ResetToWalk();
	
	void SetSprinting();

	void SetWalking();

	void SetExhausted();

	void SetGliding();

	void SetFalling();

	UFUNCTION(BlueprintCallable,BlueprintPure)
	bool IsCharacterExhausted();

	UFUNCTION(BlueprintPure, Category="Movements")
	EMovementTypes GetMovementType() const { return CurrentMT; }

	FVector CalculateDropLocation(float ForwardOffset , float TraceDistance) const;
#pragma endregion

#pragma region Stamina

	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,category="Stamina")
	float CurStamina = 0.0f;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="Stamina")
	float StaminaDepletionRate = 0.05f; //每0.05s执行一次消耗精力的逻辑

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="Stamina")
	float StaminaDeletionAmount = 0.5f; //每次执行消耗精力逻辑时消耗多少
	
	UPROPERTY(BlueprintAssignable, Category="Stamina")
	FZCStaminaChangedSignature OnStaminaChanged;

	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetStaminaRatio() const;

	void DrainStamina();
	
	FTimerHandle DrainStaminaTimerHandle;

	void StartDrainStamina();

	void RecoverStaminaTimer();

	FTimerHandle RecoverStaminaTimerHandle;

	void StartRecoverStamina();

	void ClearDrainRecoverStamina();

	FTimerHandle AddGravityForFlyingTimerHandle;

	void AddGravityForFlying();
	
#pragma endregion

#pragma region UI

	UFUNCTION(BlueprintNativeEvent, meta=(DeprecatedFunction, DeprecationMessage="Use AZCPlayerController::IsRuneMenuOpen."))
	int32 GetWSIndexInfo();

	UFUNCTION(BlueprintImplementableEvent, meta=(DeprecatedFunction, DeprecationMessage="Use AZCPlayerController::SetRuneMenuOpen."))
	void SetWSIndex(int32 Index);
	
#pragma endregion

#pragma region Runes
	UFUNCTION(BlueprintCallable, Category="Runes")
	bool SelectRune(ERunes RuneType);

	UFUNCTION(BlueprintPure, Category="Runes")
	ERunes GetSelectedRune() const;

	UFUNCTION(BlueprintPure, Category="Runes")
	ERunes GetActivatedRune() const;

    void AutoDeactivateAllRunes();
	
	UFUNCTION()
	void ToggleRuneActivity();

	UFUNCTION(BlueprintImplementableEvent)
	void CrossHairAndCameraMode(bool Display);

	// Remote Bomb 遥控炸弹
	UFUNCTION()
	void ToggleRemoteBomb();
	
	void ReleaseRemoteBombSphere();
	void ReleaseRemoteBombCube();
	void ThrowAndIgniteBomb(bool bSphere);
	const FVector GetThrowDirection();

	//Magnesis 磁铁吸附
	void FilterOutAllMagSM();

	UFUNCTION()
	void ToggleMagnesis();
	void ReleaseMagnesis();
	void UpdateMagnesisHintMats(TArray<AStaticMeshActor*> Array,UPrimitiveComponent* HoverObj);
	void SelectOrReleaseObject();
	void GrabMagObj();
	void CameraLineTraceDir(FVector &Start,FVector &End, const float Length);
	void MagDragObjTick();

	//生成冰柱
	UFUNCTION()
	void ToggleIceMode();

	bool ActivateIceMode();
	void DeactivateIceMode();

	void UpdateIcePositionTick(float DeltaTime);
	void CreateIce();

	//时间暂停
	UFUNCTION()
	void ToggleStasisMode();

	void AddStasisForce();
	void StartStasis();
	void StasisTrace(UPrimitiveComponent* &HitComp, bool &bSimulatePhysc);
	void AddForceForStasisActor();
	void BreakStasis();
	void RestoreStasisState(bool bApplyStoredImpulse);
#pragma endregion

	UFUNCTION()
	void HandleActiveRuneChanged(ERunes PreviousRune, ERunes CurrentRune);

	bool ApplyRuneActivation(ERunes RuneType, bool bShouldActivate);
	void BroadcastStaminaChanged();

	void ReadyToThrow(UStaticMeshComponent* SMRef);
};





