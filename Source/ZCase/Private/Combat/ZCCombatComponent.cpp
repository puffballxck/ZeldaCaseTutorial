// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UZCCombatComponent::UZCCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DrawMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_DrawSword.AM_DrawSword"));
	DrawSwordMontage = DrawMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> SheathMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_SheathSword.AM_SheathSword"));
	SheathSwordMontage = SheathMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Attack_01.AM_Attack_01"));
	AttackMontage = AttackMontageFinder.Object;
}

void UZCCombatComponent::InitializeEquipment(
	USkeletalMeshComponent* InCharacterMesh,
	UStaticMeshComponent* InSwordMesh,
	UStaticMeshComponent* InSheathMesh,
	UStaticMeshComponent* InShieldMesh)
{
	CharacterMesh = InCharacterMesh;
	SwordMesh = InSwordMesh;
	SheathMesh = InSheathMesh;
	ShieldMesh = InShieldMesh;
	WeaponState = EZCWeaponState::Sheathed;
	AnimationAttachmentState = EZCWeaponAttachmentState::Sheathed;
	// 初始时所有装备都放在背部挂点，后续由状态机和动画时序切换。
	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
}

EZCWeaponCommand UZCCombatComponent::ResolveAttackCommand(const EZCWeaponState State)
{
	switch (State)
	{
	case EZCWeaponState::Sheathed:
		// 收刀状态下第一次按攻击键先拔刀。
		return EZCWeaponCommand::Draw;
	case EZCWeaponState::Equipped:
		// 武器在手时同一个输入才进入攻击。
		return EZCWeaponCommand::Attack;
	default:
		// 过渡状态忽略重复输入，避免打断状态机。
		return EZCWeaponCommand::None;
	}
}

bool UZCCombatComponent::HandleAttackInput()
{
	switch (ResolveAttackCommand(WeaponState))
	{
	case EZCWeaponCommand::Draw:
		return StartDraw();
	case EZCWeaponCommand::Attack:
		return StartWeaponAttack();
	default:
		return false;
	}
}

bool UZCCombatComponent::StartDraw()
{
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	// 只有拔刀蒙太奇完成或被打断，才能离开 Drawing 状态。
	WeaponState = EZCWeaponState::Drawing;
	if (PlayMontage(DrawSwordMontage, &UZCCombatComponent::HandleDrawMontageEnded))
	{
		ScheduleAttachmentSwitch(
			EZCWeaponAttachmentState::Equipped,
			DrawSwordMontage,
			DrawAttachmentNormalizedTime);
		return true;
	}

	WeaponState = EZCWeaponState::Sheathed;
	return false;
}

bool UZCCombatComponent::StartWeaponAttack()
{
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	WeaponState = EZCWeaponState::Attacking;
	// 攻击窗口由动画或调用方显式打开，开始攻击本身不会立即造成命中。
	StartAttack();
	if (PlayMontage(AttackMontage, &UZCCombatComponent::HandleAttackMontageEnded))
	{
		return true;
	}

	FinishAttack();
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
	return false;
}

bool UZCCombatComponent::RequestSheath()
{
	if (WeaponState != EZCWeaponState::Equipped)
	{
		return false;
	}

	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	// 只允许从稳定的 Equipped 状态进入收刀，避免与其他过渡竞争挂点。
	WeaponState = EZCWeaponState::Sheathing;
	if (PlayMontage(SheathSwordMontage, &UZCCombatComponent::HandleSheathMontageEnded))
	{
		ScheduleAttachmentSwitch(
			EZCWeaponAttachmentState::Sheathed,
			SheathSwordMontage,
			SheathAttachmentNormalizedTime);
		return true;
	}

	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
	return false;
}

bool UZCCombatComponent::PlayMontage(
	UAnimMontage* Montage,
	void (UZCCombatComponent::*EndCallback)(UAnimMontage*, bool))
{
	if (!CharacterMesh || !Montage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (!AnimInstance || AnimInstance->Montage_Play(Montage) <= 0.0f)
	{
		return false;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, EndCallback);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void UZCCombatComponent::HandleDrawMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	ClearAttachmentTimer();
	if (WeaponState != EZCWeaponState::Drawing)
	{
		return;
	}

	if (bInterrupted)
	{
		// 被打断的拔刀必须回到一致的收刀挂点和状态。
		SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
		WeaponState = EZCWeaponState::Sheathed;
		return;
	}

	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
}

void UZCCombatComponent::HandleAttackMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (WeaponState != EZCWeaponState::Attacking)
	{
		return;
	}

	// 无论攻击蒙太奇正常结束还是被打断，都必须关闭残留命中窗口。
	FinishAttack();
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
}

void UZCCombatComponent::HandleSheathMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	ClearAttachmentTimer();
	if (WeaponState != EZCWeaponState::Sheathing)
	{
		return;
	}

	if (bInterrupted)
	{
		// 被打断的收刀恢复到手持挂点，并重新启动自动收刀计时。
		SetEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
		WeaponState = EZCWeaponState::Equipped;
		ScheduleAutoSheath();
		return;
	}

	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
	WeaponState = EZCWeaponState::Sheathed;
}

void UZCCombatComponent::SetEquipmentAttachmentState(const EZCWeaponAttachmentState AttachmentState)
{
	if (GetWorld()
		&& GetWorld()->GetTimerManager().IsTimerActive(AttachmentTimerHandle)
		&& PendingAttachmentState == AttachmentState)
	{
		ClearAttachmentTimer();
	}

	// 动画基础姿势必须在实际挂点切换的同一调用中更新，不能等 Montage 结束回调。
	AnimationAttachmentState = AttachmentState;

	if (!CharacterMesh)
	{
		return;
	}

	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	// 剑鞘本身始终固定在背部；剑和盾牌根据状态分别切换手部或背部挂点。
	if (SheathMesh)
	{
		SheathMesh->AttachToComponent(CharacterMesh, AttachmentRules, WeaponSheathSocket);
	}

	if (AttachmentState == EZCWeaponAttachmentState::Equipped)
	{
		if (SwordMesh)
		{
			SwordMesh->AttachToComponent(CharacterMesh, AttachmentRules, WeaponHandSocket);
		}
		if (ShieldMesh)
		{
			ShieldMesh->AttachToComponent(CharacterMesh, AttachmentRules, ShieldHandSocket);
		}
		return;
	}

	if (SwordMesh)
	{
		SwordMesh->AttachToComponent(CharacterMesh, AttachmentRules, WeaponSheathSocket);
	}
	if (ShieldMesh)
	{
		ShieldMesh->AttachToComponent(CharacterMesh, AttachmentRules, ShieldBackSocket);
	}
}

float UZCCombatComponent::CalculateAttachmentDelay(const float MontageLength, const float NormalizedTime)
{
	if (MontageLength <= 0.0f)
	{
		return 0.0f;
	}

	const float ClampedTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
	// 即使配置为 1.0，也保留极短余量，确保切换发生在蒙太奇结束回调之前。
	return FMath::Min(MontageLength * ClampedTime, FMath::Max(0.0f, MontageLength - 0.001f));
}

void UZCCombatComponent::ScheduleAttachmentSwitch(
	const EZCWeaponAttachmentState AttachmentState,
	const UAnimMontage* Montage,
	const float NormalizedTime)
{
	if (!Montage || !GetWorld())
	{
		return;
	}

	PendingAttachmentState = AttachmentState;
	const float Delay = CalculateAttachmentDelay(Montage->GetPlayLength(), NormalizedTime);
	// 归一化时刻转换为单次 Timer；无有效延迟时立即走同一套状态校验。
	if (Delay <= 0.0f)
	{
		HandleAttachmentTimerElapsed();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AttachmentTimerHandle,
		this,
		&UZCCombatComponent::HandleAttachmentTimerElapsed,
		Delay,
		false);
}

void UZCCombatComponent::ClearAttachmentTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AttachmentTimerHandle);
	}
}

void UZCCombatComponent::HandleAttachmentTimerElapsed()
{
	const bool bExpectedState =
		(PendingAttachmentState == EZCWeaponAttachmentState::Equipped
			&& WeaponState == EZCWeaponState::Drawing)
		|| (PendingAttachmentState == EZCWeaponAttachmentState::Sheathed
			&& WeaponState == EZCWeaponState::Sheathing);
	if (bExpectedState)
	{
		// Timer 可能在蒙太奇被打断后才回调，必须核对状态才能避免陈旧切换。
		SetEquipmentAttachmentState(PendingAttachmentState);
	}
}

bool UZCCombatComponent::IsWeaponEquippedForAnimation() const
{
	// 该接口描述的是基础动画姿势，而非战斗输入状态；挂点接管后即可提前准备下一套 Pose。
	return AnimationAttachmentState == EZCWeaponAttachmentState::Equipped;
}

void UZCCombatComponent::ScheduleAutoSheath()
{
	if (WeaponState != EZCWeaponState::Equipped || AutoSheathDelay <= 0.0f || !GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AutoSheathTimerHandle,
		this,
		&UZCCombatComponent::HandleAutoSheathElapsed,
		AutoSheathDelay,
		false);
}

void UZCCombatComponent::ClearAutoSheathTimer()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoSheathTimerHandle);
	}
}

void UZCCombatComponent::HandleAutoSheathElapsed()
{
	if (WeaponState == EZCWeaponState::Equipped)
	{
		// 只从稳定的 Equipped 状态触发自动收刀，过渡期间的旧 Timer 无效。
		RequestSheath();
	}
}

void UZCCombatComponent::FinishAttack()
{
	// 结束攻击时同时关闭生命周期和命中窗口；命中集合在下一次攻击开始时清空。
	bAttackActive = false;
	bTraceActive = false;
}

void UZCCombatComponent::StartAttack()
{
	bAttackActive = true;
	bTraceActive = false;
	// 每次攻击独立去重，允许同一目标在下一次攻击再次受击。
	HitActors.Reset();
}

bool UZCCombatComponent::BeginTrace()
{
	if (!bAttackActive)
	{
		return false;
	}

	bTraceActive = true;
	return true;
}

void UZCCombatComponent::EndTrace()
{
	// 关闭窗口后即使攻击仍未结束，也不再接受命中。
	bTraceActive = false;
}

bool UZCCombatComponent::TryApplyHit(AActor* Target, const float DamageAmount)
{
	if (!bAttackActive || !bTraceActive || !IsValid(Target) || DamageAmount <= 0.0f)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (Owner && Target == Owner)
	{
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (HitActors.Contains(TargetKey))
	{
		// 同一次攻击对同一 Actor 只允许一次伤害，防止连续帧重复命中。
		return false;
	}

	HitActors.Add(TargetKey);
	AController* InstigatorController = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	UGameplayStatics::ApplyDamage(Target, DamageAmount, InstigatorController, Owner, UDamageType::StaticClass());
	return true;
}
