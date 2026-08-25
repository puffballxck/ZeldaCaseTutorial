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

/** 管理武器状态、装备挂点、攻击生命周期，以及一次攻击内的命中去重规则。 */
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

	/** 将攻击输入按当前武器状态解释为拔刀或攻击。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat|Weapon")
	bool HandleAttackInput();

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

	/** 将攻击输入状态映射为动作；过渡状态统一返回 None。 */
	static EZCWeaponCommand ResolveAttackCommand(EZCWeaponState State);
	/** 按蒙太奇长度和归一化时刻计算装备切换 Timer 延迟。 */
	static float CalculateAttachmentDelay(float MontageLength, float NormalizedTime);

	/** 开启一次攻击生命周期，并清空该次攻击的已命中集合。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void StartAttack();

	/** 打开本次攻击的命中检测窗口；未开始攻击时返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool BeginTrace();

	/** 关闭本次攻击的命中检测窗口。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	void EndTrace();

	/** 在有效命中窗口内对目标造成一次伤害；同一攻击不会重复命中同一目标。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Combat")
	bool TryApplyHit(AActor* Target, float DamageAmount);

	/** 返回当前是否处于可登记命中的窗口。 */
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
	void ScheduleAttachmentSwitch(
		EZCWeaponAttachmentState AttachmentState,
		const UAnimMontage* Montage,
		float NormalizedTime);
	void ClearAttachmentTimer();
	void HandleAttachmentTimerElapsed();

	/** 拔刀时播放的武器蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> DrawSwordMontage;

	/** 收刀时播放的武器蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> SheathSwordMontage;

	/** 武器已装备时播放的攻击蒙太奇。 */
	UPROPERTY(EditDefaultsOnly, Category = "ZCase|Combat|Weapon|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 武器保持 Equipped 后自动请求收刀的等待秒数。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon", meta = (ClampMin = "0.1"))
	float AutoSheathDelay = 5.0f;

	/** 拔刀蒙太奇中由手部接管装备的归一化时刻。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DrawAttachmentNormalizedTime = 0.35f;

	/** 收刀蒙太奇中装备回到背部挂点的归一化时刻。 */
	UPROPERTY(EditAnywhere, Category = "ZCase|Combat|Weapon|Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SheathAttachmentNormalizedTime = 0.70f;

	/** 驱动拔刀、攻击和收刀输入策略的武器状态机。 */
	UPROPERTY(VisibleInstanceOnly, Category = "ZCase|Combat|Weapon")
	EZCWeaponState WeaponState = EZCWeaponState::Sheathed;

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
	/** 当前是否存在一条尚未结束的攻击生命周期。 */
	bool bAttackActive = false;
	/** 当前是否处于可登记命中的窗口。 */
	bool bTraceActive = false;
	/** 本次攻击已经命中的目标集合，用于去重。 */
	TSet<TWeakObjectPtr<AActor>> HitActors;
};
