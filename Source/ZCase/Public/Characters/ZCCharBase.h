// 请在项目设置的说明页面填写版权声明

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
class UAnimMontage;

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
	/** 角色锁定与跟随相机共用的 SpringArm */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Comps")
	USpringArmComponent* CameraBoom;
	//TObjectPtr<USpringArmComponent> CameraBoom; 等价
	
	/** 挂载玩家视角的相机组件 */
	UPROPERTY(EditAnywhere,Category="Comps")
	UCameraComponent* FollowCamera;

	/** 用于举起和拖拽物体的物理约束组件 */
	UPROPERTY(EditAnywhere,Category="Comps")
	UPhysicsHandleComponent* PhysicsHandle;

	/** 物理拖拽物体跟随角色的目标位置组件 */
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
	
	/** 该角色使用的 Enhanced Input 映射上下文 */
	UPROPERTY(EditAnywhere,Category="Inputs")
	UInputMappingContext* IMC_ZC;
	
	UPROPERTY(EditAnywhere,category="Inputs")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere,category="Inputs")
	UInputAction* LookAction;

	/** Enhanced Input 的目标锁定动作；Started 时按距离循环可见候选 */
	UPROPERTY(EditAnywhere, category="Inputs")
	UInputAction* TargetLockAction;

	/** Enhanced Input 的独立目标解除动作 */
	UPROPERTY(EditAnywhere, category="Inputs")
	UInputAction* TargetUnlockAction;

	UPROPERTY(EditAnywhere,category="Inputs")
	UInputAction* SprintAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* JumpGlideAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* ToggleUIAction;

	/** 独立的背包动作，由玩家控制器在 Possess 后绑定 */
	UPROPERTY(EditAnywhere, category="Inputs")
	UInputAction* InventoryAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* ActiveRuneAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* ReleaseRuneAction;

	UPROPERTY(editAnywhere,category="Inputs")
	UInputAction* InteractAction;

	/** Enhanced Input 的攻击动作，接线后交给 Combat 组件按武器状态解释 */
	UPROPERTY(EditAnywhere, Category="Inputs")
	UInputAction* AttackAction;

	/** 手动收起武器，复用 Combat 的收刀动画和状态机 */
	UPROPERTY(EditAnywhere, Category="Inputs")
	UInputAction* OffWeaponAction;

	/** 可选的 IA_Guard 动作，创建资源后在 IMC_ZC 中绑定到鼠标右键 */
	UPROPERTY(EditAnywhere, Category="Inputs")
	UInputAction* GuardAction;

	/** 当前动作是否允许举盾；供伤害和动画即时查询 */
	bool CanMaintainGuard() const;

	/** 当前移动状态，体力与动画系统都以它为状态来源 */
	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,category="Movements")
	EMovementTypes CurrentMT{ EMovementTypes ::MT_EMAX};

	UPROPERTY(EditAnywhere,BlueprintReadWrite,category="Rune | Magnesis")
	TSubclassOf<AActor> StaticMeshClass;

	/** 当前场景中允许被磁铁吸附的静态网格 Actor 缓存 */
	UPROPERTY()
	TArray<AStaticMeshActor*> AllMagSMs;//存放当前场景中所有可吸附物体

	/** 当前被磁铁吸附的物理组件 */
	UPROPERTY()
	UPrimitiveComponent* MagnesisObj = nullptr;
	/** 最近一次磁铁 Trace 命中的组件，用于恢复材质 */
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

	/** 当前冰柱引用，激活期间用于位置更新和释放 */
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

	/** 当前静止技能施加力的组件 */
	UPROPERTY()
	UPrimitiveComponent* StasisComp;

	FTimerHandle StasisTimerHandle;

	/** 当前静止技能生成的箭头表现 Actor */
	UPROPERTY()
	AStaticActor* StasisForceActor;

	/** 磁铁技能拖拽特效组件 */
	UPROPERTY()
	UParticleSystemComponent* BeamParticleComp;
	
	/** 当前交互范围内的 Actor，投掷和普通交互共用 */
	UPROPERTY()
	AInteractBase* InteractingActor;
	
	/** 当前是否持有可投掷的炸弹 */
	bool bReadyToThrow = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runes")
	TObjectPtr<UZCRuneRuntimeComponent> RuneRuntime;

	/** 负责生命值、伤害钳制和一次性死亡事件 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UZCAttributeComponent> Attributes;

	/** 负责武器状态机、攻击命中窗口和装备挂点 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UZCCombatComponent> Combat;

	/** 可切换到手部或剑鞘挂点的剑网格 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Equipment")
	TObjectPtr<UStaticMeshComponent> SwordMesh;

	/** 始终附着在背部剑鞘挂点的剑鞘网格 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Equipment")
	TObjectPtr<UStaticMeshComponent> SheathMesh;

	/** 在左手盾牌挂点与背部挂点之间切换的盾牌网格 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Equipment")
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	/** 持有并验证当前目标锁定对象 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Target Lock")
	TObjectPtr<UZCTargetLockComponent> TargetLock;
	
	/** 遥控炸弹是否已激活 */
	bool bRBActivated = false;//炸弹是否被激活
	/** 磁铁技能是否已激活 */
	bool bMagActivated = false;//磁铁是否被激活
	/** 静止技能是否已激活 */
	bool bStasisActivated = false;
	/** 制冰技能是否已激活 */
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
	
	/** 是否有炸弹处于角色手持状态 */
	bool bHoldingBomb = false;
	/** 当前手持的是球形炸弹而不是方块炸弹 */
	bool bSphereBomb = false;

	
#pragma endregion

	/** 旧版角色蓝图提供的布局类，现由 PlayerController 作为回退 */
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> LayoutClassRef;

	/** 旧版角色持有的布局实例，迁移期间保持兼容 */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="UI")
	UZCLayout* LayoutRef;

	UPROPERTY(EditDefaultsOnly,category="Movements")
	FVector EnableGlideDistance = FVector(0.0f, 0.0f, 150.0f);

	UPROPERTY(BlueprintAssignable, Category="Movements")
	FZCMovementTypeChangedSignature OnMovementTypeChanged;

	/** 是否正在风场范围内，滑翔逻辑据此跳过地面限制 */
	bool bIsInWindTunnel = false; // 标识是否在风场中


protected:
	/** 创建运行时组件并把装备交给 Combat 统一管理 */
	virtual void BeginPlay() override;
	/** 清理技能、交互、目标锁定和战斗回调 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 非致死伤害播放的全身受击蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Player|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** 首次致死伤害播放并保持最终姿势的全身死亡蒙太奇 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Player|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** 播放非致命受击 Montage 并设置受击锁定 */
	void PlayHitReact();
	/** 受击 Montage 结束后恢复战斗可用状态 */
	void HandleHitReactMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void HandleDeath(AActor* DeadActor);

	/** 当前是否存在尚未结束的受击 Montage */
	bool bHitReactActive = false;
	/** 是否已经进入死亡表现生命周期 */
	bool bDeathStarted = false;
	/** 防止同步伤害或生命值回调递归扣除多颗半心 */
	bool bDamageProcessing = false;
	bool bHitReactDiagnosticIssued = false;
	bool bDeathDiagnosticIssued = false;

	/** 着陆时结束滑翔并恢复地面移动状态 */
	virtual void Landed(const FHitResult& Hit) override;

#pragma region Inputs Node
	UFUNCTION()
	/** 处理连续移动输入并同步目标锁定方向 */
	void Move_Triggered(const FInputActionValue& val);

	UFUNCTION()
	void Move_Completed(const FInputActionValue& val);

	UFUNCTION()
	/** 处理视角输入，锁定时仍允许自由相机旋转 */
	void Look_Triggered(const FInputActionValue& val);

	/** Enhanced Input 的目标锁定 Started 回调 */
	UFUNCTION()
	void TargetLock_Started(const FInputActionValue& val);

	/** Enhanced Input 的独立目标解除 Started 回调 */
	UFUNCTION()
	void TargetUnlock_Started(const FInputActionValue& val);

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

	/** Enhanced Input 的统一左键 Started 回调；按交互/技能状态分流 */
	UFUNCTION()
	void Attack_Started(const FInputActionValue& val);

	UFUNCTION()
	/** 请求 Combat 执行收刀，输入层不直接修改武器状态 */
	void OffWeapon_Started(const FInputActionValue& val);

	/** Enhanced Input 的右键 Started 回调；交给 Combat 尝试有限窗口招架 */
	UFUNCTION()
	void Guard_Started(const FInputActionValue& val);
	
#pragma endregion
	
public:	
	/** 更新技能、移动和目标锁定的每帧状态 */
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 将引擎伤害先交给父类，再由 Attributes 统一处理生命值和死亡状态 */
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser) override;

	/** 死亡角色不再允许新的目标锁定 */
	virtual bool CanBeTargetLocked() const override;
	/** 返回目标锁定使用的角色世界位置 */
	virtual FVector GetTargetLockLocation() const override;

	/** 目标锁定时角色每秒最多旋转的 Yaw 角度 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float TargetLockRotationSpeed = 720.0f;

	/** 获取/切换目标时的最大转身速度；正常持续跟随仍使用 TargetLockRotationSpeed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "0.0"))
	float TargetSwitchRotationSpeed = 420.0f;

	/** 切敌转身的角加速度（度/秒平方），同时用于减速和反向制动 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "1.0"))
	float TargetSwitchRotationAcceleration = 1800.0f;

	/** 剩余角度小于该值时逐渐减速 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ZCase|Target Lock", meta = (ClampMin = "1.0"))
	float TargetSwitchSlowdownAngle = 60.0f;

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	bool IsDeathStarted() const { return bDeathStarted; }
	
#pragma region Locomotion
	UFUNCTION()
	void LocomotionManager(EMovementTypes NewMovement);

	/** 从特殊移动状态回到普通行走 */
	void ResetToWalk();
	
	/** 进入冲刺并开始体力消耗 */
	void SetSprinting();

	/** 进入普通行走并停止体力消耗 */
	void SetWalking();

	/** 体力耗尽后进入限制移动状态 */
	void SetExhausted();

	/** 进入滑翔并启动空中重力补偿 */
	void SetGliding();

	/** 进入自由下落状态 */
	void SetFalling();

	UFUNCTION(BlueprintCallable,BlueprintPure)
	bool IsCharacterExhausted();

	UFUNCTION(BlueprintPure, Category="Movements")
	EMovementTypes GetMovementType() const { return CurrentMT; }

	FVector CalculateDropLocation(float ForwardOffset , float TraceDistance) const;
#pragma endregion

#pragma region Stamina

	UPROPERTY(VisibleInstanceOnly,BlueprintReadOnly,category="Stamina")
	/** 当前体力值，范围由 MaxStamina 和运行时逻辑限制 */
	float CurStamina = 0.0f;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="Stamina")
	/** 体力上限，单位为体力点 */
	float MaxStamina = 100.0f;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="Stamina")
	float StaminaDepletionRate = 0.05f; //每0.05s执行一次消耗精力的逻辑

	UPROPERTY(EditAnywhere,BlueprintReadOnly,category="Stamina")
	float StaminaDeletionAmount = 0.5f; //每次执行消耗精力逻辑时消耗多少
	
	UPROPERTY(BlueprintAssignable, Category="Stamina")
	FZCStaminaChangedSignature OnStaminaChanged;

	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetStaminaRatio() const;

	/** 按固定 Timer 扣除体力并广播变化 */
	void DrainStamina();
	
	FTimerHandle DrainStaminaTimerHandle;

	/** 开始体力消耗 Timer */
	void StartDrainStamina();

	/** 按恢复 Timer 增加体力 */
	void RecoverStaminaTimer();

	FTimerHandle RecoverStaminaTimerHandle;

	/** 开始体力恢复 Timer */
	void StartRecoverStamina();

	/** 清理消耗与恢复 Timer，避免两个方向同时运行 */
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
	/** 选择符文并交给 Runtime 组件维护激活不变量 */
	bool SelectRune(ERunes RuneType);

	UFUNCTION(BlueprintPure, Category="Runes")
	ERunes GetSelectedRune() const;

	UFUNCTION(BlueprintPure, Category="Runes")
	ERunes GetActivatedRune() const;

	/** 取消所有符文效果并清理对应的场景表现 */
	void AutoDeactivateAllRunes();
	
	UFUNCTION()
	/** 切换当前选择符文的激活状态 */
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

	/** 目标锁定组件变更目标时切换角色朝向模式 */
	UFUNCTION()
	void HandleTargetChanged(AActor* PreviousTarget, AActor* CurrentTarget);

	/** 在角色 Tick 中按锁定目标更新水平朝向 */
	void UpdateTargetLockOrientation(float DeltaTime);

	/** 统一判断目标锁定能否进入或继续维持 */
	bool CanUseTargetLock() const;

	/** 设置移动组件的旋转模式；TargetLockComponent 不负责角色旋转 */
	void SetTargetLockRotationMode(bool bEnableTargetLockRotation);

	/** 每帧把移动、技能和持物状态同步为 Combat 的守卫抑制条件 */
	void UpdateGuardSuppression();

	bool bTargetLockRotationActive = false;
	bool bTargetSwitchTurnActive = false;
	float TargetSwitchYawVelocity = 0.0f;
};

