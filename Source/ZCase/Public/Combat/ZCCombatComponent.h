// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ZCCombatComponent.generated.h"

class UAnimMontage;
class UAnimInstance;
class UAnimSequenceBase;
class UMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FActorComponentTickFunction;
struct FDamageEvent;

UENUM(BlueprintType)
enum class EZCCombatAvailability : uint8
{
	/** 接受战斗输入、动画通知和命中登记。 */
	Enabled,
	/** 正在播放受击反应；保留移动和镜头，但暂时拒绝战斗动作。 */
	Reacting,
	/** 死亡后的终止状态；陈旧回调不能再次启用战斗。 */
	Disabled
};

/** 独立于武器挂点的防御状态；挂剑不等于免伤。 */
UENUM(BlueprintType)
enum class EZCDefenseState : uint8
{
	Normal,
	Guarding,
	BlockHit,
	Parrying,
	Broken
};

/** 角色收到伤害前的防御判定结果。 */
UENUM(BlueprintType)
enum class EZCDefenseHitResult : uint8
{
	None,
	Blocked,
	Parried,
	GuardBroken,
	DamageThroughBroken
};

/** 一次攻击接触完成伤害结算后提供给表现层的稳定结果。 */
USTRUCT(BlueprintType)
struct ZCASE_API FZCCombatHitResult
{
	GENERATED_BODY()

	/** 是否登记为本次攻击对该目标的第一次有效接触。 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	bool bRegistered = false;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	float RequestedDamage = 0.0f;

	/** 目标 TakeDamage 返回的实际伤害。 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	float AppliedDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	FVector ImpactNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	FName HitBoneName = NAME_None;

	/** 已知 ZC 属性目标是否由本次接触从存活转为死亡。 */
	UPROPERTY(BlueprintReadOnly, Category = "ZCase|Combat|Hit")
	bool bBecameDead = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
FZCHitResolvedSignature,
	const FZCCombatHitResult&, Result);

/** 一次攻击生命周期结束时广播；参数表示蒙太奇是否被打断。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FZCAttackEndedSignature,
	bool,
	bInterrupted);

UENUM(BlueprintType)
enum class EZCWeaponState : uint8
{
	/** 武器和盾牌都在背部挂点。 */
	Sheathed,
	/** 正在播放拔刀动画，输入暂时不触发新动作。 */
	Drawing,
	/** 武器已在手中，可以开始攻击或收刀。 */
	Equipped,
	/** 正在播放攻击动画并可能打开命中窗口。 */
	Attacking,
	/** 正在播放收刀动画，输入暂时不触发新动作。 */
	Sheathing
};

UENUM(BlueprintType)
enum class EZCWeaponCommand : uint8
{
	/** 当前状态不接受攻击输入。 */
	None,
	/** 将攻击输入解释为拔刀。 */
	Draw,
	/** 将攻击输入解释为攻击。 */
	Attack
};

UENUM(BlueprintType)
enum class EZCWeaponAttachmentState : uint8
{
	/** 将剑放回剑鞘、盾牌放回背部。 */
	Sheathed,
	/** 将剑和盾牌切换到手部挂点。 */
	Equipped
};

/** 管理装备或骨骼端点驱动的攻击生命周期、命中窗口和一次攻击内的去重规则。 */
UCLASS(ClassGroup = (ZCase), meta = (BlueprintSpawnableComponent))
class ZCASE_API UZCCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCCombatComponent();

	/** 保存角色骨骼网格和三件装备，并将初始挂点设置为收刀状态。 */
	void InitializeEquipment(
		USkeletalMeshComponent* InCharacterMesh,
		UStaticMeshComponent* InSwordMesh,
		UStaticMeshComponent* InSheathMesh,
		UStaticMeshComponent* InShieldMesh);

	/**
	 * 初始化非装备攻击的动画网格和轨迹端点。
	 *
	 * Trace mesh 可以是静态网格或骨骼网格；骨骼网格端点允许直接使用骨骼名。
	 * 该初始化不会改变武器装备状态，只接管攻击蒙太奇所需的网格和轨迹来源。
	 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	void InitializeAttackSource(
		USkeletalMeshComponent* InCharacterMesh,
		UMeshComponent* InTraceMesh,
		FName InTraceBasePoint,
		FName InTraceTipPoint);

	/** 设置由 TryAttack 播放的攻击蒙太奇；敌人 AI 每次攻击前可覆盖它。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	void SetAttackMontage(UAnimMontage* InAttackMontage);

	/** 设置攻击轨迹每次有效接触请求的伤害值。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	void SetTraceDamage(float InTraceDamage);

	/** 设置攻击轨迹 Sweep 球体半径，单位为厘米。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	void SetTraceRadius(float InTraceRadius);

	/**
	 * 设置是否只允许玩家控制的 Pawn 通过本组件造成伤害。
	 * 默认关闭，保留玩家装备攻击对任意有效目标的原有行为；敌人可打开它来避免误伤同类。
	 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	void SetPlayerOnlyDamage(bool bInPlayerOnlyDamage);

	/** 在不要求武器处于 Equipped 的情况下开始一次攻击；敌人 AI 使用该入口。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	bool TryAttack();

	/** 取消当前攻击并关闭命中窗口；不会进入受击状态或改变玩家的输入状态机。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Attack")
	void CancelAttack();

	/** 将攻击输入按当前武器状态解释为拔刀或攻击。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	bool HandleAttackInput();

	/** 由 IA_Guard Started 调用；目标锁定守卫中尝试一次有限窗口招架。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Defense")
	bool HandleGuardInput();

	/** 通知组件当前是否存在有效目标锁定；锁定时装备好的剑会自动进入守卫。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Defense")
	void SetTargetLockActive(bool bInTargetLockActive);

	/** 暂停守卫表现但保留目标锁定意图，便于动作结束后恢复。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Defense")
	void SetGuardSuppressed(bool bInGuardSuppressed);

	/** 在 CharBase 扣血前调用；只处理正面守卫、招架、破防和破防期间的普通伤害。 */
	EZCDefenseHitResult ResolveIncomingDamage(const FDamageEvent& DamageEvent, AActor* DamageCauser);

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Defense")
	EZCDefenseState GetDefenseState() const { return DefenseState; }

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Defense")
	bool IsGuardPoseActive() const;

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Defense")
	bool IsGuardBroken() const { return DefenseState == EZCDefenseState::Broken; }

	/** 仅在武器已装备时请求收刀。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	bool RequestSheath();

	/** 立即把剑、剑鞘和盾牌附着到指定的一组骨骼挂点。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	void SetEquipmentAttachmentState(EZCWeaponAttachmentState AttachmentState);

	/** 返回武器状态机当前状态。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Weapon")
	EZCWeaponState GetWeaponState() const { return WeaponState; }

	/** 返回动画层应视为“武器已在手中”的状态。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Combat|Weapon")
	bool IsWeaponEquippedForAnimation() const;

	/** 返回战斗动作当前是否可用。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	bool CanAcceptCombatInput() const { return CombatAvailability == EZCCombatAvailability::Enabled && !IsGuardBroken(); }

	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	EZCCombatAvailability GetCombatAvailability() const { return CombatAvailability; }

	/** 打断当前战斗动作并进入受击锁定；死亡后调用不会恢复战斗。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool InterruptForHitReaction();

	/** 仅允许从受击锁定恢复。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void ResumeAfterHitReaction();

	/** 进入不可逆的死亡禁用状态。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void DisableCombat();

	/** 将攻击输入状态映射为动作；过渡状态统一返回 None。 */
	static EZCWeaponCommand ResolveAttackCommand(EZCWeaponState State);
	/** 按蒙太奇长度和归一化时刻计算装备切换 Timer 延迟。 */
	static float CalculateAttachmentDelay(float MontageLength, float NormalizedTime);
	/** 将采样段数限制在可控范围，避免配置值导致每帧产生过多 Sweep。 */
	static int32 NormalizeTraceSampleSegments(int32 RequestedSegments);
	/** 生成上一帧与当前帧的剑身采样点，供 Sweep 和自动化测试共用。 */
	static void BuildTraceSamplePositions(
		const FVector& PreviousBase,
		const FVector& PreviousTip,
		const FVector& CurrentBase,
		const FVector& CurrentTip,
		int32 SampleSegments,
		TArray<FVector>& OutPreviousSamples,
		TArray<FVector>& OutCurrentSamples);

	/** 开启一次攻击生命周期，并清空该次攻击的已命中集合。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void StartAttack();

	/** 打开本次攻击的命中检测窗口；未开始攻击时返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool BeginTrace();

	/** 关闭本次攻击的命中检测窗口。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void EndTrace();

	/**
	 * 由 ZC Weapon Trace NotifyState 的 NotifyEnd 调用。
	 * 玩家已经排队下一次攻击时，在当前命中窗口结束处直接切换下一段，跳过收刀尾段。
	 */
	void HandleAttackTraceWindowEnded(UAnimSequenceBase* Animation);

	/** 在有效命中窗口内结算碰撞；同一攻击不会重复命中同一目标。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	FZCCombatHitResult TryApplyHit(const FHitResult& Hit, float DamageAmount);

	/** 每次攻击中首次登记某目标后广播，实际伤害允许为零。 */
	UPROPERTY(BlueprintAssignable, Category = "ZCase|Combat|Hit")
	FZCHitResolvedSignature OnHitResolved;

	/** 攻击蒙太奇正常结束或被打断关闭攻击生命周期后广播一次。 */
	UPROPERTY(BlueprintAssignable, Category = "ZCase|Combat|Attack")
	FZCAttackEndedSignature OnAttackEnded;

	/** 返回当前是否处于可登记命中的窗口。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	bool IsTraceActive() const { return bTraceActive; }

	/** 当前攻击窗口是否已开启，供组件生命周期测试和调试使用。 */
	UFUNCTION(BlueprintPure, Category = "ZCase|Combat")
	bool IsAttackActive() const { return bAttackActive; }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	bool GetTraceSocketLocations(FVector& OutBase, FVector& OutTip);
	void DisableTraceTick();
	bool StartDraw();
	bool StartGuard();
	void ExitGuard(bool bClearRequests);
	bool StartParry();
	bool StartDefenseMontage(UAnimMontage* Montage, EZCDefenseState MontageState);
	void HandleDefenseMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint32 Generation);
	void StopDefenseMontage();
	void ClearDefenseMontageEndDelegate();
	void HandleBlockHit();
	void EnterGuardBroken();
	void ResetGuardBlockCount();
	void HandleParryWindowStart(uint32 Generation);
	void HandleParryWindowEnd(uint32 Generation);
	void HandleGuardBreakFallbackElapsed(uint32 Generation);
	void ClearDefenseTimers();
	void EnsureGuardState();
	bool IsGuardDesired() const;
	bool CanEnterGuard() const;
	bool IsDamageFromFront(const FDamageEvent& DamageEvent, AActor* DamageCauser) const;
	bool StartWeaponAttack(UAnimMontage* Montage, bool bUsePlayerCombo);
	bool ContinuePlayerAttackCombo();
	UAnimMontage* ResolvePlayerAttackMontage() const;
	void ResetPlayerAttackCombo();
	void AdvancePlayerAttackCombo();
	bool PlayMontage(
		UAnimMontage* Montage,
		void (UZCCombatComponent::*EndCallback)(UAnimMontage*, bool),
		float BlendInOverride = -1.0f);
	void HandleDrawMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void ClearDrawMontageEndDelegate();
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void HandleSheathMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void ClearAttackMontageEndDelegate(UAnimInstance* AnimInstance);
	void ScheduleAutoSheath();
	void ClearAutoSheathTimer();
	void HandleAutoSheathElapsed();
	/** 关闭攻击生命周期并返回关闭前是否存在活动攻击。 */
	bool FinishAttack();
	void ScheduleAttachmentSwitch(
		EZCWeaponAttachmentState AttachmentState,
		const UAnimMontage* Montage,
		float NormalizedTime);
	void ClearAttachmentTimer();
	void HandleAttachmentTimerElapsed();
	void ApplyEquipmentAttachmentState(EZCWeaponAttachmentState AttachmentState);

	/** 拔刀时播放的武器蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> DrawSwordMontage;

	/** 锁定目标时使用的附加拔刀蒙太奇；缺失时回退到普通拔刀。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> DrawSwordOnLockonAdditiveMontage;

	/** 收刀时播放的武器蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> SheathSwordMontage;

	/** 武器已装备时播放的攻击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 玩家第二次攻击使用的连段蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage02;

	/** 玩家第三次攻击使用的连段蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage03;

	/** 玩家第四段终结攻击；完整收招后开始新连段。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage04;

	/** 守卫受击表现。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Defense|Animation")
	TObjectPtr<UAnimMontage> GuardHitMontage;

	/** 招架表现；成功窗口由下方时间参数限定。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Defense|Animation")
	TObjectPtr<UAnimMontage> GuardParryMontage;

	/** 破防表现。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Defense|Animation")
	TObjectPtr<UAnimMontage> GuardBreakMontage;

	/** 武器保持 Equipped 后自动请求收刀的等待秒数。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon", meta = (ClampMin = "0.1"))
	float AutoSheathDelay = 5.0f;

	/** 拔刀蒙太奇中由手部接管装备的归一化时刻。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DrawAttachmentNormalizedTime = 0.35f;

	/** 收刀蒙太奇中装备回到背部挂点的归一化时刻。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SheathAttachmentNormalizedTime = 0.70f;

	/** 守卫正面判定的半角；90 度表示前半球。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Defense", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float GuardFrontHalfAngleDegrees = 90.0f;

	/** 连续成功挡住多少次后进入破防。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Defense", meta = (ClampMin = "1"))
	int32 GuardBlocksToBreak = 3;

	/** 多久没有新的格挡命中后清零连续格挡次数。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Defense", meta = (ClampMin = "0.0"))
	float GuardBlockResetTime = 3.0f;

	/** 招架蒙太奇开始后的成功判定起点（秒）。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Defense|Parry", meta = (ClampMin = "0.0"))
	float ParryWindowStartTime = 0.0f;

	/** 招架蒙太奇开始后的成功判定终点（秒）。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Defense|Parry", meta = (ClampMin = "0.0"))
	float ParryWindowEndTime = 0.20f;

	/** 缺少破防蒙太奇时仍需短暂锁住动作，避免状态永久卡在 Broken。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Defense", meta = (ClampMin = "0.0"))
	float GuardBreakRecoveryDuration = 0.75f;

	/** 驱动拔刀、攻击和收刀输入策略的武器状态机。 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Combat|Weapon")
	EZCWeaponState WeaponState = EZCWeaponState::Sheathed;

	/** 独立于武器状态的战斗可用性；死亡会把它锁定为 Disabled。 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Combat")
	EZCCombatAvailability CombatAvailability = EZCCombatAvailability::Enabled;

	/**
	 * 驱动动画基础姿势的实际装备挂点状态。
	 *
	 * 与 WeaponState 分离：WeaponState 在整段拔刀/收刀 Montage 期间保持过渡态，
	 * 而该值在接触帧切换挂点时更新，使 AnimBP 能在 Montage 结束前准备下一套基础姿势。
	 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Combat|Weapon")
	EZCWeaponAttachmentState AnimationAttachmentState = EZCWeaponAttachmentState::Sheathed;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> CharacterMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SwordMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SheathMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShieldMesh;

	/** 当前用于攻击轨迹的来源网格；玩家是剑网格，敌人可以直接使用骨骼网格。 */
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> TraceSourceMesh;

	/** 剑在手中的骨骼挂点。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName WeaponHandSocket = TEXT("WeaponHand_R");

	/** 剑和剑鞘在背部的骨骼挂点。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName WeaponSheathSocket = TEXT("WeaponSheath");

	/** 盾牌在手中的骨骼挂点。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName ShieldHandSocket = TEXT("ShieldHand_L");

	/** 盾牌在背部的骨骼挂点。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Sockets")
	FName ShieldBackSocket = TEXT("ShieldBack");

	/** 控制装备自动收刀的单次 Timer。 */
	FTimerHandle AutoSheathTimerHandle;
	/** 控制拔刀/收刀动画中途装备切换的单次 Timer。 */
	FTimerHandle AttachmentTimerHandle;
	/** Timer 到期时准备应用的挂点状态。 */
	EZCWeaponAttachmentState PendingAttachmentState = EZCWeaponAttachmentState::Sheathed;
	/** 当前实际播放的拔刀蒙太奇，普通和锁定拔刀各自校验自己的结束回调。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDrawMontage;

	/** 当前防御表现状态，不依赖 WeaponState 或 AttachmentState。 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Combat|Defense")
	EZCDefenseState DefenseState = EZCDefenseState::Normal;
	/** 目标锁定产生的自动守卫意图。 */
	bool bTargetLockActive = false;
	/** 攻击、跳跃、冲刺、技能和持物期间暂时隐藏守卫姿势。 */
	bool bGuardSuppressed = false;
	/** ParryWindowStart/End Timer 的代数校验。 */
	uint32 ParryWindowGeneration = 0;
	/** 当前是否位于招架有效窗口。 */
	bool bParryWindowActive = false;
	/** 当前已成功格挡的连续次数。 */
	int32 GuardBlockCount = 0;
	/** 防御蒙太奇结束回调的代数，防止旧回调清理新状态。 */
	uint32 DefenseMontageGeneration = 0;
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveDefenseMontage;
	FTimerHandle GuardBlockResetTimerHandle;
	FTimerHandle ParryWindowStartTimerHandle;
	FTimerHandle ParryWindowEndTimerHandle;
	FTimerHandle GuardBreakFallbackTimerHandle;
	/** 当前是否存在一条尚未结束的攻击生命周期。 */
	bool bAttackActive = false;
	/** 当前正在播放的攻击蒙太奇；玩家连段和敌人 AI 都通过它校验结束回调。 */
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage;
	/** 当前玩家连段序号：0、1、2、3 分别对应 Attack01/02/03/04。 */
	int32 PlayerAttackComboIndex = 0;
	/** 当前生命周期是否由玩家连段入口启动。 */
	bool bActivePlayerAttackCombo = false;
	/** 玩家在当前攻击窗口结束前是否已经按下下一次攻击。 */
	bool bPlayerAttackQueued = false;
	/** 当前段的 ZC Weapon Trace 窗口是否已经结束，用于收刀尾段中的预输入衔接。 */
	bool bPlayerAttackTraceWindowEnded = false;
	/** 当前是否处于可登记命中的窗口。 */
	bool bTraceActive = false;
	/** 本次攻击已经命中的目标集合，用于去重。 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** 当前攻击窗口的伤害值。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace", meta = (ClampMin = "0.0"))
	float TraceDamage = 25.0f;
	/** 是否将命中目标限制为玩家控制的 Pawn；默认为 false。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace")
	bool bPlayerOnlyDamage = false;
	/** 剑身 Sweep 球体半径，单位为厘米。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace", meta = (ClampMin = "0.0"))
	float TraceRadius = 8.0f;
	/** 武器 Sweep 使用的碰撞通道。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;
	/** 沿剑身划分的采样段数；实际每帧 Sweep 点数为段数加一。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace", meta = (ClampMin = "1", ClampMax = "32"))
	int32 TraceSampleSegments = 4;
	/** 是否在编辑器/开发构建中绘制当前剑身轨迹。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace")
	bool bDebugDrawTrace = false;
	/** 攻击轨迹来源网格的起点和终点；骨骼网格也可填写骨骼名。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace|Sockets")
	FName TraceBaseSocket = TEXT("Trace_Base");
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Trace|Sockets")
	FName TraceTipSocket = TEXT("Trace_Tip");

	/** 上一次 Tick 的 socket 位置；窗口第一次 Tick 只建立基线，不产生跨帧 Sweep。 */
	FVector PreviousTraceBase = FVector::ZeroVector;
	FVector PreviousTraceTip = FVector::ZeroVector;
	bool bHasPreviousTracePositions = false;
	/** 避免缺失 Mesh/socket 时每帧刷屏，同时保留一次可定位的警告。 */
	bool bTraceConfigurationWarningLogged = false;
};
