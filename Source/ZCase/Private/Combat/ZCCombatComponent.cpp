// 版权所有 Epic Games, Inc，保留所有权利

#include "Combat/ZCCombatComponent.h"

#include "AlphaBlend.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/ZCAttributeComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Characters/ZCCharBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UZCCombatComponent::UZCCombatComponent()
{
	// Trace 只在 NotifyState 打开的攻击窗口内 Tick，避免常态下产生无意义的碰撞查询
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// 在动画和角色移动更新后读取附着武器的位置，避免 Sweep 使用上一阶段的骨骼变换
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DrawMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_DrawSword.AM_DrawSword"));
	DrawSwordMontage = DrawMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> SheathMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_SheathSword.AM_SheathSword"));
	SheathSwordMontage = SheathMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> LockedDrawMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/00_Combat/06_Equip_Build/Sword/AM_Equip_Sword_On_Lockon_Additive.AM_Equip_Sword_On_Lockon_Additive"));
	DrawSwordOnLockonAdditiveMontage = LockedDrawMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Attack_01.AM_Attack_01"));
	AttackMontage = AttackMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontage02Finder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Attack_02.AM_Attack_02"));
	AttackMontage02 = AttackMontage02Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontage03Finder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Attack_03.AM_Attack_03"));
	AttackMontage03 = AttackMontage03Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontage04Finder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Attack_04.AM_Attack_04"));
	AttackMontage04 = AttackMontage04Finder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> GuardHitMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Sword_Guard_Hit.AM_Sword_Guard_Hit"));
	GuardHitMontage = GuardHitMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> GuardParryMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Sword_Guard_Just.AM_Sword_Guard_Just"));
	GuardParryMontage = GuardParryMontageFinder.Object;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> GuardBreakMontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Guard_BreaK.AM_Guard_BreaK"));
	GuardBreakMontage = GuardBreakMontageFinder.Object;
}

void UZCCombatComponent::InitializeEquipment(
	USkeletalMeshComponent* InCharacterMesh,
	UStaticMeshComponent* InSwordMesh,
	UStaticMeshComponent* InSheathMesh,
	UStaticMeshComponent* InShieldMesh)
{
	// 重新绑定装备时切断旧攻击生命周期，避免旧的 Notify/Tick 继续使用已失效的 socket
	EndTrace();
	bAttackActive = false;
	ActiveAttackMontage = nullptr;
	bActivePlayerAttackCombo = false;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ResetPlayerAttackCombo();
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	ClearDefenseTimers();
	ResetGuardBlockCount();
	ClearDefenseMontageEndDelegate();
	DefenseState = EZCDefenseState::Normal;
	bTargetLockActive = false;
	bGuardSuppressed = false;
	bParryWindowActive = false;
	GuardBlockCount = 0;
	ActiveDrawMontage = nullptr;
	CharacterMesh = InCharacterMesh;
	SwordMesh = InSwordMesh;
	SheathMesh = InSheathMesh;
	ShieldMesh = InShieldMesh;
	TraceSourceMesh = InSwordMesh;
	WeaponState = EZCWeaponState::Sheathed;
	AnimationAttachmentState = EZCWeaponAttachmentState::Sheathed;
	// 初始时所有装备都放在背部挂点，后续由状态机和动画时序切换
	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
}

void UZCCombatComponent::InitializeAttackSource(
	USkeletalMeshComponent* InCharacterMesh,
	UMeshComponent* InTraceMesh,
	const FName InTraceBasePoint,
	const FName InTraceTipPoint)
{
	// 重新绑定攻击来源时关闭旧窗口，但不改变死亡后的 Disabled 状态
	EndTrace();
	bAttackActive = false;
	bActivePlayerAttackCombo = false;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ActiveAttackMontage = nullptr;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	ClearDefenseTimers();
	ResetGuardBlockCount();
	ClearDefenseMontageEndDelegate();
	DefenseState = EZCDefenseState::Normal;
	bTargetLockActive = false;
	bGuardSuppressed = false;
	bParryWindowActive = false;
	GuardBlockCount = 0;
	ActiveDrawMontage = nullptr;
	CharacterMesh = InCharacterMesh;
	TraceSourceMesh = InTraceMesh;
	// NAME_None 表示保留组件可编辑默认值，这样
	// 通用敌人基类只需提供网格，具体敌人可以选择
	// 显式使用骨骼端点
	if (!InTraceBasePoint.IsNone())
	{
		TraceBaseSocket = InTraceBasePoint;
	}
	if (!InTraceTipPoint.IsNone())
	{
		TraceTipSocket = InTraceTipPoint;
	}
}

void UZCCombatComponent::SetAttackMontage(UAnimMontage* InAttackMontage)
{
	AttackMontage = InAttackMontage;
}

UAnimMontage* UZCCombatComponent::ResolvePlayerAttackMontage() const
{
	switch (PlayerAttackComboIndex)
	{
	case 1:
		return AttackMontage02.Get();
	case 2:
		return AttackMontage03.Get();
	case 3:
		return AttackMontage04.Get();
	default:
		return AttackMontage.Get();
	}
}

void UZCCombatComponent::ResetPlayerAttackCombo()
{
	PlayerAttackComboIndex = 0;
}

void UZCCombatComponent::AdvancePlayerAttackCombo()
{
	PlayerAttackComboIndex = FMath::Min(PlayerAttackComboIndex + 1, 3);
}

void UZCCombatComponent::SetTraceDamage(const float InTraceDamage)
{
	TraceDamage = FMath::IsFinite(InTraceDamage) ? FMath::Max(InTraceDamage, 0.0f) : 0.0f;
}

void UZCCombatComponent::SetTraceRadius(const float InTraceRadius)
{
	TraceRadius = FMath::IsFinite(InTraceRadius) ? FMath::Max(InTraceRadius, 0.0f) : 0.0f;
}

void UZCCombatComponent::SetPlayerOnlyDamage(const bool bInPlayerOnlyDamage)
{
	bPlayerOnlyDamage = bInPlayerOnlyDamage;
}

EZCWeaponCommand UZCCombatComponent::ResolveAttackCommand(const EZCWeaponState State)
{
	switch (State)
	{
	case EZCWeaponState::Sheathed:
		// 收刀状态下第一次按攻击键先拔刀
		return EZCWeaponCommand::Draw;
	case EZCWeaponState::Equipped:
		// 武器在手时同一个输入才进入攻击
		return EZCWeaponCommand::Attack;
	default:
		// 过渡状态忽略重复输入，避免打断状态机
		return EZCWeaponCommand::None;
	}
}

bool UZCCombatComponent::HandleAttackInput()
{
	if (!CanAcceptCombatInput() || IsGuardBroken())
	{
		return false;
	}

	// 玩家连段允许在当前命中窗口结束前预输入一次；NotifyEnd 会在不播放收刀尾段的情况下接上下一段
	if (bActivePlayerAttackCombo && bAttackActive && WeaponState == EZCWeaponState::Attacking)
	{
		// 终结段必须完成收招，连点不能跳回第一段取消它
		if (PlayerAttackComboIndex >= 3)
		{
			return false;
		}
		bPlayerAttackQueued = true;
		// Trace 窗口已经结束时不再等待 Montage 末尾，直接跳过收刀尾段
		return bPlayerAttackTraceWindowEnded ? ContinuePlayerAttackCombo() : true;
	}

	switch (ResolveAttackCommand(WeaponState))
	{
	case EZCWeaponCommand::Draw:
		return StartDraw();
	case EZCWeaponCommand::Attack:
		return StartWeaponAttack(ResolvePlayerAttackMontage(), true);
	default:
		return false;
	}
}

bool UZCCombatComponent::HandleGuardInput()
{
	if (!CanAcceptCombatInput() || IsGuardBroken() || bGuardSuppressed || !bTargetLockActive)
	{
		return false;
	}

	// Started 事件刻意采用边沿触发，在目标锁定
	// 守卫激活期间只开启一次有限招架尝试，而不是每帧重复播放
	// 持续按住的 Montage，守卫命中也可以从 BlockHit 进入同一路径
	if (DefenseState == EZCDefenseState::Guarding || DefenseState == EZCDefenseState::BlockHit)
	{
		return StartParry();
	}

	if (DefenseState == EZCDefenseState::Parrying)
	{
		return false;
	}

	EnsureGuardState();
	return DefenseState == EZCDefenseState::Guarding && StartParry();
}

void UZCCombatComponent::SetTargetLockActive(const bool bInTargetLockActive)
{
	if (bTargetLockActive == bInTargetLockActive)
	{
		if (bInTargetLockActive)
		{
			EnsureGuardState();
		}
		return;
	}

	bTargetLockActive = bInTargetLockActive;
	if (!bTargetLockActive)
	{
		// 解除锁定是自动守卫路径的权威退出条件
		ExitGuard(true);
		if (WeaponState == EZCWeaponState::Equipped)
		{
			ScheduleAutoSheath();
		}
		return;
	}

	EnsureGuardState();
}

void UZCCombatComponent::SetGuardSuppressed(const bool bInGuardSuppressed)
{
	if (bGuardSuppressed == bInGuardSuppressed)
	{
		if (!bInGuardSuppressed)
		{
			EnsureGuardState();
		}
		return;
	}

	bGuardSuppressed = bInGuardSuppressed;
	if (bGuardSuppressed)
	{
		if (DefenseState != EZCDefenseState::Broken)
		{
			ClearDefenseTimers();
			StopDefenseMontage();
			DefenseState = EZCDefenseState::Normal;
		}
		return;
	}

	EnsureGuardState();
}

bool UZCCombatComponent::IsGuardPoseActive() const
{
	return IsGuardDesired() && CanEnterGuard() && (DefenseState == EZCDefenseState::Guarding
		|| DefenseState == EZCDefenseState::BlockHit
		|| DefenseState == EZCDefenseState::Parrying);
}

bool UZCCombatComponent::IsGuardDesired() const
{
	return bTargetLockActive;
}

bool UZCCombatComponent::CanEnterGuard() const
{
	const AZCCharBase* Player = Cast<AZCCharBase>(GetOwner());
	return CanAcceptCombatInput()
		&& Player && Player->CanMaintainGuard()
		&& !bGuardSuppressed
		&& !IsGuardBroken()
		&& !bAttackActive
		&& WeaponState == EZCWeaponState::Equipped
		&& CharacterMesh != nullptr;
}

bool UZCCombatComponent::StartGuard()
{
	if (!IsGuardDesired() || !CanEnterGuard())
	{
		return false;
	}

	ClearAutoSheathTimer();
	if (DefenseState == EZCDefenseState::Guarding
		|| DefenseState == EZCDefenseState::BlockHit
		|| DefenseState == EZCDefenseState::Parrying)
	{
		return true;
	}

	DefenseState = EZCDefenseState::Guarding;
	return true;
}

void UZCCombatComponent::EnsureGuardState()
{
	if (!IsGuardDesired() || !CanAcceptCombatInput() || bGuardSuppressed || IsGuardBroken())
	{
		return;
	}

	if (WeaponState == EZCWeaponState::Equipped)
	{
		StartGuard();
	}
}

void UZCCombatComponent::ExitGuard(const bool bClearRequests)
{
	if (bClearRequests)
	{
		bTargetLockActive = false;
	}

	const bool bWasBroken = DefenseState == EZCDefenseState::Broken;
	if (!bWasBroken)
	{
		ClearDefenseTimers();
		StopDefenseMontage();
		DefenseState = EZCDefenseState::Normal;
	}
	ResetGuardBlockCount();

	if (!IsGuardDesired() && WeaponState == EZCWeaponState::Equipped)
	{
		ScheduleAutoSheath();
	}
}

bool UZCCombatComponent::StartParry()
{
	if (!IsGuardDesired() || !CanEnterGuard() || DefenseState == EZCDefenseState::Parrying)
	{
		return false;
	}

	ClearDefenseTimers();
	StopDefenseMontage();
	DefenseState = EZCDefenseState::Parrying;
	bParryWindowActive = false;

	if (!GuardParryMontage || !StartDefenseMontage(GuardParryMontage, EZCDefenseState::Parrying))
	{
		// 缺少可选资源时也不能让角色永久停在 Parrying
		DefenseState = EZCDefenseState::Guarding;
		return false;
	}

	const float MontageLength = GuardParryMontage->GetPlayLength();
	const float SafeStart = FMath::Clamp(
		FMath::IsFinite(ParryWindowStartTime) ? ParryWindowStartTime : 0.0f,
		0.0f,
		MontageLength);
	const float SafeEnd = FMath::Clamp(
		FMath::IsFinite(ParryWindowEndTime) ? ParryWindowEndTime : 0.0f,
		SafeStart,
		MontageLength);
	const uint32 ExpectedGeneration = ++ParryWindowGeneration;

	if (SafeEnd > SafeStart && GetWorld())
	{
		if (SafeStart <= 0.0f)
		{
			HandleParryWindowStart(ExpectedGeneration);
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimer(
				ParryWindowStartTimerHandle,
				FTimerDelegate::CreateWeakLambda(this,
					[this, ExpectedGeneration]()
					{
						HandleParryWindowStart(ExpectedGeneration);
					}),
				SafeStart,
				false);
		}

		GetWorld()->GetTimerManager().SetTimer(
			ParryWindowEndTimerHandle,
			FTimerDelegate::CreateWeakLambda(this,
				[this, ExpectedGeneration]()
				{
					HandleParryWindowEnd(ExpectedGeneration);
				}),
			SafeEnd,
			false);
	}

	return true;
}

bool UZCCombatComponent::StartDefenseMontage(
	UAnimMontage* Montage,
	const EZCDefenseState MontageState)
{
	if (CombatAvailability != EZCCombatAvailability::Enabled || !CharacterMesh || !Montage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	UAnimMontage* PreviousMontage = ActiveDefenseMontage.Get();
	if (PreviousMontage && AnimInstance->Montage_IsPlaying(PreviousMontage))
	{
		ClearDefenseMontageEndDelegate();
		AnimInstance->Montage_Stop(0.05f, PreviousMontage);
	}

	const float PlayLength = AnimInstance->Montage_Play(Montage);
	if (PlayLength <= 0.0f)
	{
		return false;
	}

	ActiveDefenseMontage = Montage;
	const uint32 ExpectedGeneration = ++DefenseMontageGeneration;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindWeakLambda(this,
		[this, ExpectedGeneration, MontageState](UAnimMontage* EndedMontage, const bool bInterrupted)
		{
			if (DefenseState == MontageState)
			{
				HandleDefenseMontageEnded(EndedMontage, bInterrupted, ExpectedGeneration);
			}
		});
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void UZCCombatComponent::HandleDefenseMontageEnded(
	UAnimMontage* Montage,
	const bool bInterrupted,
	const uint32 Generation)
{
	if (Generation != DefenseMontageGeneration || Montage != ActiveDefenseMontage.Get())
	{
		return;
	}

	ActiveDefenseMontage = nullptr;
	ClearDefenseTimers();
	bParryWindowActive = false;

	if (DefenseState == EZCDefenseState::Broken)
	{
		GuardBlockCount = 0;
		DefenseState = EZCDefenseState::Normal;
		EnsureGuardState();
		ScheduleAutoSheath();
		return;
	}

	if (DefenseState == EZCDefenseState::BlockHit || DefenseState == EZCDefenseState::Parrying)
	{
		DefenseState = EZCDefenseState::Normal;
		EnsureGuardState();
		if (DefenseState == EZCDefenseState::Normal && WeaponState == EZCWeaponState::Equipped)
		{
			ScheduleAutoSheath();
		}
	}
}

void UZCCombatComponent::ClearDefenseMontageEndDelegate()
{
	if (CharacterMesh && ActiveDefenseMontage)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			FOnMontageEnded EmptyEndDelegate;
			AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, ActiveDefenseMontage.Get());
		}
	}

	ActiveDefenseMontage = nullptr;
	++DefenseMontageGeneration;
}

void UZCCombatComponent::StopDefenseMontage()
{
	UAnimMontage* MontageToStop = ActiveDefenseMontage.Get();
	UAnimInstance* AnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	const bool bMontagePlaying = MontageToStop && AnimInstance && AnimInstance->Montage_IsPlaying(MontageToStop);
	ClearDefenseMontageEndDelegate();
	if (bMontagePlaying)
	{
		AnimInstance->Montage_Stop(0.05f, MontageToStop);
	}
}

void UZCCombatComponent::HandleBlockHit()
{
	if (!CanEnterGuard())
	{
		return;
	}

	if (GuardBlockResetTime <= 0.0f)
	{
		GuardBlockCount = 0;
	}
	GuardBlockCount = FMath::Max(0, GuardBlockCount) + 1;
	if (GetWorld())
	{
		if (GuardBlockResetTime > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				GuardBlockResetTimerHandle,
				this,
				&UZCCombatComponent::ResetGuardBlockCount,
				GuardBlockResetTime,
				false);
		}
		else
		{
			GetWorld()->GetTimerManager().ClearTimer(GuardBlockResetTimerHandle);
		}
	}

	if (GuardBlockCount >= FMath::Max(1, GuardBlocksToBreak))
	{
		EnterGuardBroken();
		return;
	}

	ClearDefenseTimers();
	bParryWindowActive = false;
	if (DefenseState == EZCDefenseState::BlockHit
		&& ActiveDefenseMontage == GuardHitMontage)
	{
		return;
	}

	StopDefenseMontage();
	DefenseState = EZCDefenseState::BlockHit;
	if (!GuardHitMontage || !StartDefenseMontage(GuardHitMontage, EZCDefenseState::BlockHit))
	{
		DefenseState = EZCDefenseState::Guarding;
	}
}

void UZCCombatComponent::EnterGuardBroken()
{
	if (CombatAvailability == EZCCombatAvailability::Disabled)
	{
		return;
	}

	ClearDefenseTimers();
	StopDefenseMontage();
	bParryWindowActive = false;
	ResetGuardBlockCount();
	DefenseState = EZCDefenseState::Broken;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->StopJumping();
		Character->ConsumeMovementInputVector();
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	if (GuardBreakMontage && StartDefenseMontage(GuardBreakMontage, EZCDefenseState::Broken))
	{
		return;
	}

	const uint32 ExpectedGeneration = ++DefenseMontageGeneration;
	if (GetWorld() && GuardBreakRecoveryDuration > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			GuardBreakFallbackTimerHandle,
			FTimerDelegate::CreateWeakLambda(this,
				[this, ExpectedGeneration]()
				{
					HandleGuardBreakFallbackElapsed(ExpectedGeneration);
				}),
			GuardBreakRecoveryDuration,
			false);
	}
	else
	{
		HandleGuardBreakFallbackElapsed(ExpectedGeneration);
	}
}

void UZCCombatComponent::ResetGuardBlockCount()
{
	GuardBlockCount = 0;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GuardBlockResetTimerHandle);
	}
}

void UZCCombatComponent::HandleParryWindowStart(const uint32 Generation)
{
	if (Generation == ParryWindowGeneration
		&& DefenseState == EZCDefenseState::Parrying
		&& ActiveDefenseMontage == GuardParryMontage)
	{
		bParryWindowActive = true;
	}
}

void UZCCombatComponent::HandleParryWindowEnd(const uint32 Generation)
{
	if (Generation == ParryWindowGeneration && DefenseState == EZCDefenseState::Parrying)
	{
		bParryWindowActive = false;
	}
}

void UZCCombatComponent::HandleGuardBreakFallbackElapsed(const uint32 Generation)
{
	if (Generation != DefenseMontageGeneration || DefenseState != EZCDefenseState::Broken)
	{
		return;
	}

	DefenseState = EZCDefenseState::Normal;
	EnsureGuardState();
	if (DefenseState == EZCDefenseState::Normal && WeaponState == EZCWeaponState::Equipped)
	{
		ScheduleAutoSheath();
	}
}

void UZCCombatComponent::ClearDefenseTimers()
{
	if (GetWorld())
	{
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		TimerManager.ClearTimer(ParryWindowStartTimerHandle);
		TimerManager.ClearTimer(ParryWindowEndTimerHandle);
		TimerManager.ClearTimer(GuardBreakFallbackTimerHandle);
	}
	++ParryWindowGeneration;
	bParryWindowActive = false;
}

bool UZCCombatComponent::IsDamageFromFront(
	const FDamageEvent& DamageEvent,
	AActor* DamageCauser) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	FVector ToAttacker = DamageCauser
		? DamageCauser->GetActorLocation() - OwnerLocation
		: FVector::ZeroVector;

	if (ToAttacker.SizeSquared2D() <= FMath::Square(KINDA_SMALL_NUMBER)
		&& DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		ToAttacker = -PointDamage.ShotDirection;
	}

	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const FVector HorizontalAttacker = ToAttacker.GetSafeNormal2D();
	if (Forward.IsNearlyZero() || HorizontalAttacker.IsNearlyZero())
	{
		return false;
	}

	const float SafeHalfAngle = FMath::Clamp(
		FMath::IsFinite(GuardFrontHalfAngleDegrees) ? GuardFrontHalfAngleDegrees : 90.0f,
		0.0f,
		180.0f);
	const float RequiredDot = FMath::Cos(FMath::DegreesToRadians(SafeHalfAngle));
	return FVector::DotProduct(Forward, HorizontalAttacker) >= RequiredDot;
}

EZCDefenseHitResult UZCCombatComponent::ResolveIncomingDamage(
	const FDamageEvent& DamageEvent,
	AActor* DamageCauser)
{
	if (DefenseState == EZCDefenseState::Broken)
	{
		// Broken 是可受伤状态，调用方执行普通
		// 生命伤害，但会抑制覆盖破防姿态的第二次受击反应
		return EZCDefenseHitResult::DamageThroughBroken;
	}

	if (!IsGuardDesired() || !CanEnterGuard()
		|| !IsDamageFromFront(DamageEvent, DamageCauser))
	{
		return EZCDefenseHitResult::None;
	}

	if (DefenseState == EZCDefenseState::Parrying && bParryWindowActive)
	{
		bParryWindowActive = false;
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(ParryWindowStartTimerHandle);
			GetWorld()->GetTimerManager().ClearTimer(ParryWindowEndTimerHandle);
		}
		return EZCDefenseHitResult::Parried;
	}

	if (DefenseState == EZCDefenseState::Guarding
		|| DefenseState == EZCDefenseState::BlockHit
		|| DefenseState == EZCDefenseState::Parrying)
	{
		HandleBlockHit();
		return DefenseState == EZCDefenseState::Broken
			? EZCDefenseHitResult::GuardBroken
			: EZCDefenseHitResult::Blocked;
	}

	return EZCDefenseHitResult::None;
}

bool UZCCombatComponent::TryAttack()
{
	// AI 入口不要求武器状态为 Equipped，但仍共享同一战斗可用性和攻击生命周期
	if (!CanAcceptCombatInput() || IsGuardBroken() || bAttackActive || WeaponState == EZCWeaponState::Attacking
		|| !CharacterMesh || !AttackMontage)
	{
		return false;
	}

	return StartWeaponAttack(AttackMontage.Get(), false);
}

void UZCCombatComponent::CancelAttack()
{
	// 先关闭状态和 Trace，再停止 Montage；Montage 的旧结束回调会因状态已改变而失效
	const bool bHadActiveAttack = FinishAttack();
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	if (WeaponState == EZCWeaponState::Attacking)
	{
		WeaponState = AnimationAttachmentState == EZCWeaponAttachmentState::Equipped
			? EZCWeaponState::Equipped
			: EZCWeaponState::Sheathed;
	}

	if (bHadActiveAttack && CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			if (ActiveAttackMontage && AnimInstance->Montage_IsPlaying(ActiveAttackMontage.Get()))
			{
				// 开始 BlendOut 前先移除旧 Delegate，否则
				// 后续复用同一 Montage 的攻击可能收到旧实例的
				// 结束回调并错误结束新的攻击生命周期
				ClearAttackMontageEndDelegate(AnimInstance);
				AnimInstance->Montage_Stop(0.05f, ActiveAttackMontage.Get());
			}
		}
	}

	if (bActivePlayerAttackCombo)
	{
		ResetPlayerAttackCombo();
	}
	bActivePlayerAttackCombo = false;
	ActiveAttackMontage = nullptr;
	SetGuardSuppressed(false);

	if (bHadActiveAttack)
	{
		OnAttackEnded.Broadcast(true);
	}
}

bool UZCCombatComponent::StartDraw()
{
	if (!CanAcceptCombatInput())
	{
		return false;
	}

	// 目标锁定状态下若配置了可选资源，拔刀会在 WeaponAdditive 上播放 Additive Montage
	// 普通未锁定拔刀仍使用旧 Montage
	// 两条路径共享同一装备切换时序
	UAnimMontage* RequestedDrawMontage = bTargetLockActive && DrawSwordOnLockonAdditiveMontage
		? DrawSwordOnLockonAdditiveMontage.Get()
		: DrawSwordMontage.Get();
	if (!RequestedDrawMontage)
	{
		return false;
	}

	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	ActiveDrawMontage = RequestedDrawMontage;
	// 只有拔刀蒙太奇完成或被打断，才能离开 Drawing 状态
	WeaponState = EZCWeaponState::Drawing;
	if (PlayMontage(ActiveDrawMontage.Get(), &UZCCombatComponent::HandleDrawMontageEnded))
	{
		ScheduleAttachmentSwitch(
			EZCWeaponAttachmentState::Equipped,
			ActiveDrawMontage.Get(),
			DrawAttachmentNormalizedTime);
		return true;
	}

	ActiveDrawMontage = nullptr;
	WeaponState = EZCWeaponState::Sheathed;
	return false;
}

bool UZCCombatComponent::StartWeaponAttack(UAnimMontage* Montage, const bool bUsePlayerCombo)
{
	if (!CanAcceptCombatInput() || bAttackActive || WeaponState == EZCWeaponState::Attacking || !Montage)
	{
		return false;
	}

	SetGuardSuppressed(true);
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	WeaponState = EZCWeaponState::Attacking;
	ActiveAttackMontage = Montage;
	bActivePlayerAttackCombo = bUsePlayerCombo;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	// 攻击窗口由动画或调用方显式打开，开始攻击本身不会立即造成命中
	StartAttack();
	if (PlayMontage(ActiveAttackMontage.Get(), &UZCCombatComponent::HandleAttackMontageEnded))
	{
		return true;
	}

	const bool bHadActiveAttack = FinishAttack();
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
	if (bActivePlayerAttackCombo)
	{
		ResetPlayerAttackCombo();
	}
	bActivePlayerAttackCombo = false;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ActiveAttackMontage = nullptr;
	SetGuardSuppressed(false);
	if (bHadActiveAttack)
	{
		OnAttackEnded.Broadcast(true);
	}
	return false;
}

bool UZCCombatComponent::RequestSheath()
{
	if (!CanAcceptCombatInput() || WeaponState != EZCWeaponState::Equipped || !SwordMesh)
	{
		return false;
	}

	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ResetPlayerAttackCombo();
	// 只允许从稳定的 Equipped 状态进入收刀，避免与其他过渡竞争挂点
	WeaponState = EZCWeaponState::Sheathing;
	ExitGuard(false);
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
	void (UZCCombatComponent::*EndCallback)(UAnimMontage*, bool),
	const float BlendInOverride)
{
	if (!CharacterMesh || !Montage)
	{
		return false;
	}

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	const float PlayLength = BlendInOverride >= 0.0f
		? AnimInstance->Montage_PlayWithBlendIn(Montage, FAlphaBlendArgs(BlendInOverride))
		: AnimInstance->Montage_Play(Montage);
	if (PlayLength <= 0.0f)
	{
		return false;
	}

	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, EndCallback);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

bool UZCCombatComponent::ContinuePlayerAttackCombo()
{
	if (!CanAcceptCombatInput()
		|| !bActivePlayerAttackCombo
		|| !bAttackActive
		|| !bPlayerAttackQueued
		|| !bPlayerAttackTraceWindowEnded
		|| PlayerAttackComboIndex >= 3
		|| WeaponState != EZCWeaponState::Attacking
		|| !CharacterMesh)
	{
		return false;
	}

	UAnimMontage* PreviousMontage = ActiveAttackMontage.Get();
	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	const int32 PreviousComboIndex = PlayerAttackComboIndex;
	AdvancePlayerAttackCombo();
	UAnimMontage* NextMontage = ResolvePlayerAttackMontage();
	if (!NextMontage)
	{
		PlayerAttackComboIndex = PreviousComboIndex;
		bPlayerAttackQueued = false;
		return false;
	}

	// 先摘掉上一段的结束回调，再让新段接管；旧段淡出不能结束新攻击
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	if (PreviousMontage && AnimInstance->Montage_IsPlaying(PreviousMontage))
	{
		ClearAttackMontageEndDelegate(AnimInstance);
	}

	ActiveAttackMontage = NextMontage;
	// 每一段独立清空已命中集合，但不结束整个玩家连段生命周期
	StartAttack();
	// 新 Montage 的 BlendIn 会淡出旧段；保留旧姿势用于过渡，并从第 0 秒保留新段前摇
	if (PlayMontage(ActiveAttackMontage.Get(), &UZCCombatComponent::HandleAttackMontageEnded, 0.05f))
	{
		return true;
	}

	const bool bHadActiveAttack = FinishAttack();
	WeaponState = EZCWeaponState::Equipped;
	ScheduleAutoSheath();
	bActivePlayerAttackCombo = false;
	ActiveAttackMontage = nullptr;
	SetGuardSuppressed(false);
	ResetPlayerAttackCombo();
	if (bHadActiveAttack)
	{
		OnAttackEnded.Broadcast(true);
	}
	return false;
}

void UZCCombatComponent::HandleDrawMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActiveDrawMontage.Get() || WeaponState != EZCWeaponState::Drawing)
	{
		return;
	}

	ClearAttachmentTimer();
	ActiveDrawMontage = nullptr;

	if (bInterrupted)
	{
		// 被打断的拔刀必须回到一致的收刀挂点和状态
		SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
		WeaponState = EZCWeaponState::Sheathed;
		return;
	}

	SetEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
	WeaponState = EZCWeaponState::Equipped;
	if (IsGuardDesired())
	{
		StartGuard();
	}
	else
	{
		ScheduleAutoSheath();
	}
}

void UZCCombatComponent::ClearDrawMontageEndDelegate()
{
	if (CharacterMesh && ActiveDrawMontage)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			FOnMontageEnded EmptyEndDelegate;
			AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, ActiveDrawMontage.Get());
		}
	}
	ActiveDrawMontage = nullptr;
}

void UZCCombatComponent::HandleAttackMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActiveAttackMontage.Get() || WeaponState != EZCWeaponState::Attacking)
	{
		return;
	}

	// 无论攻击蒙太奇正常结束还是被打断，都必须关闭残留命中窗口
	const bool bHadActiveAttack = FinishAttack();
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	WeaponState = EZCWeaponState::Equipped;
	if (bActivePlayerAttackCombo)
	{
		// 完整收招表示连段已经结束；下一次独立攻击从第一段开始
		ResetPlayerAttackCombo();
	}
	bActivePlayerAttackCombo = false;
	ActiveAttackMontage = nullptr;
	SetGuardSuppressed(false);
	ScheduleAutoSheath();
	if (bHadActiveAttack)
	{
		OnAttackEnded.Broadcast(bInterrupted);
	}
}

void UZCCombatComponent::HandleAttackTraceWindowEnded(UAnimSequenceBase* Animation)
{
	// 旧 Montage 淡出时也会收到 NotifyEnd，不能关闭或推进新一段的窗口
	if (const UAnimMontage* SourceMontage = Cast<UAnimMontage>(Animation))
	{
		if (SourceMontage != ActiveAttackMontage.Get())
		{
			return;
		}
	}
	EndTrace();
	if (!bActivePlayerAttackCombo || !bAttackActive || WeaponState != EZCWeaponState::Attacking)
	{
		return;
	}
	bPlayerAttackTraceWindowEnded = true;
	if (bPlayerAttackQueued && GetWorld())
	{
		// 离开动画通知派发后再换段，避免 Montage_Play 重入正在处理的 NotifyState
		const TWeakObjectPtr<UAnimMontage> ExpectedMontage = ActiveAttackMontage.Get();
		const int32 ExpectedComboIndex = PlayerAttackComboIndex;
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
			[this, ExpectedMontage, ExpectedComboIndex]()
			{
				if (ActiveAttackMontage.Get() == ExpectedMontage.Get() && PlayerAttackComboIndex == ExpectedComboIndex)
				{
					ContinuePlayerAttackCombo();
				}
			}));
	}
}

void UZCCombatComponent::HandleSheathMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != SheathSwordMontage || WeaponState != EZCWeaponState::Sheathing)
	{
		return;
	}
	ClearAttachmentTimer();

	if (bInterrupted)
	{
		// 被打断的收刀恢复到手持挂点，并重新启动自动收刀计时
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
	// 受击/死亡后忽略旧攻击 Montage 迟到的挂点 Notify
	if (!CanAcceptCombatInput())
	{
		return;
	}
	ApplyEquipmentAttachmentState(AttachmentState);
}

void UZCCombatComponent::ApplyEquipmentAttachmentState(const EZCWeaponAttachmentState AttachmentState)
{
	if (GetWorld()
		&& GetWorld()->GetTimerManager().IsTimerActive(AttachmentTimerHandle)
		&& PendingAttachmentState == AttachmentState)
	{
		ClearAttachmentTimer();
	}

	// 动画基础姿势必须在实际挂点切换的同一调用中更新，不能等 Montage 结束回调
	AnimationAttachmentState = AttachmentState;

	if (!CharacterMesh)
	{
		return;
	}

	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	// 剑鞘本身始终固定在背部；剑和盾牌根据状态分别切换手部或背部挂点
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
	// 即使配置为 1.0，也保留极短余量，确保切换发生在蒙太奇结束回调之前
	return FMath::Min(MontageLength * ClampedTime, FMath::Max(0.0f, MontageLength - 0.001f));
}

int32 UZCCombatComponent::NormalizeTraceSampleSegments(const int32 RequestedSegments)
{
	// 上限是保护性约束：Sweep 数量应由动画配置控制，但不能因误填值拖垮每帧查询
	return FMath::Clamp(RequestedSegments, 1, 32);
}

void UZCCombatComponent::BuildTraceSamplePositions(
	const FVector& PreviousBase,
	const FVector& PreviousTip,
	const FVector& CurrentBase,
	const FVector& CurrentTip,
	const int32 SampleSegments,
	TArray<FVector>& OutPreviousSamples,
	TArray<FVector>& OutCurrentSamples)
{
	const int32 SafeSegments = NormalizeTraceSampleSegments(SampleSegments);
	OutPreviousSamples.Reset(SafeSegments + 1);
	OutCurrentSamples.Reset(SafeSegments + 1);

	for (int32 SampleIndex = 0; SampleIndex <= SafeSegments; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SafeSegments);
		OutPreviousSamples.Add(FMath::Lerp(PreviousBase, PreviousTip, Alpha));
		OutCurrentSamples.Add(FMath::Lerp(CurrentBase, CurrentTip, Alpha));
	}
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
	// 归一化时刻转换为单次 Timer；无有效延迟时立即走同一套状态校验
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
		// Timer 可能在蒙太奇被打断后才回调，必须核对状态才能避免陈旧切换
		SetEquipmentAttachmentState(PendingAttachmentState);
	}
}

bool UZCCombatComponent::IsWeaponEquippedForAnimation() const
{
	// 该接口描述的是基础动画姿势，而非战斗输入状态；挂点接管后即可提前准备下一套 Pose
	return AnimationAttachmentState == EZCWeaponAttachmentState::Equipped;
}

void UZCCombatComponent::ScheduleAutoSheath()
{
	if (!CanAcceptCombatInput() || IsGuardDesired() || IsGuardPoseActive()
		|| WeaponState != EZCWeaponState::Equipped || !SwordMesh
		|| AutoSheathDelay <= 0.0f || !GetWorld())
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
	if (CanAcceptCombatInput() && !IsGuardDesired() && !IsGuardPoseActive()
		&& WeaponState == EZCWeaponState::Equipped)
	{
		// 只从稳定的 Equipped 状态触发自动收刀，过渡期间的旧 Timer 无效
		RequestSheath();
	}
}

bool UZCCombatComponent::FinishAttack()
{
	// 结束攻击时同时关闭生命周期、命中窗口和 Tick；命中集合在下一次攻击开始时清空
	const bool bHadActiveAttack = bAttackActive;
	EndTrace();
	bAttackActive = false;
	return bHadActiveAttack;
}

void UZCCombatComponent::StartAttack()
{
	if (!CanAcceptCombatInput())
	{
		return;
	}

	// 新攻击接管前先关闭旧窗口，保证不会把上一攻击的 Tick/基线带入本次攻击
	EndTrace();
	bAttackActive = true;
	// 每次攻击独立去重，允许同一目标在下一次攻击再次受击
	HitActors.Reset();
}

bool UZCCombatComponent::BeginTrace()
{
	if (!CanAcceptCombatInput() || !bAttackActive || !GetTraceSocketLocations(PreviousTraceBase, PreviousTraceTip))
	{
		return false;
	}

	// NotifyState 的 Begin 是唯一打开窗口的入口；先建立上一帧基线，避免武器从挂点瞬移时误扫整段路径
	bHasPreviousTracePositions = true;
	bTraceConfigurationWarningLogged = false;
	bTraceActive = true;
	SetComponentTickEnabled(true);
	return true;
}

void UZCCombatComponent::EndTrace()
{
	// 关闭窗口后即使攻击仍未结束，也不再接受命中；同时关闭 Tick，形成窗口生命周期不变量
	bTraceActive = false;
	bHasPreviousTracePositions = false;
	DisableTraceTick();
}

void UZCCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CombatAvailability = EZCCombatAvailability::Disabled;
	// 角色销毁/PIE 停止可能绕过 Montage 回调，必须在组件生命周期边界强制关闭残留 Trace
	EndTrace();
	bAttackActive = false;
	ClearDrawMontageEndDelegate();
	ActiveAttackMontage = nullptr;
	StopDefenseMontage();
	ClearDefenseTimers();
	ResetGuardBlockCount();
	DefenseState = EZCDefenseState::Normal;
	bTargetLockActive = false;
	bGuardSuppressed = false;
	bActivePlayerAttackCombo = false;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ResetPlayerAttackCombo();
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	Super::EndPlay(EndPlayReason);
}

bool UZCCombatComponent::InterruptForHitReaction()
{
	if (CombatAvailability == EZCCombatAvailability::Disabled)
	{
		return false;
	}

	// 连续受击由角色重置同一 Montage 的位置；这里保持锁定且不打断该 Montage
	if (CombatAvailability == EZCCombatAvailability::Reacting)
	{
		return true;
	}

	CombatAvailability = EZCCombatAvailability::Reacting;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	const bool bWasDrawing = WeaponState == EZCWeaponState::Drawing;
	const EZCWeaponAttachmentState PreviousAttachmentState = AnimationAttachmentState;
	const bool bHadActiveAttack = FinishAttack();
	ClearDrawMontageEndDelegate();
	StopDefenseMontage();
	DefenseState = EZCDefenseState::Normal;
	WeaponState = AnimationAttachmentState == EZCWeaponAttachmentState::Equipped
		? EZCWeaponState::Equipped
		: EZCWeaponState::Sheathed;
	if (CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			ClearAttackMontageEndDelegate(AnimInstance);
			AnimInstance->Montage_Stop(0.05f);
		}
	}
	if (bWasDrawing)
	{
		ApplyEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
		WeaponState = EZCWeaponState::Sheathed;
	}
	else if (PreviousAttachmentState == EZCWeaponAttachmentState::Equipped)
	{
		ApplyEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
	}
	if (bActivePlayerAttackCombo)
	{
		ResetPlayerAttackCombo();
	}
	bActivePlayerAttackCombo = false;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ActiveAttackMontage = nullptr;
	bGuardSuppressed = false;
	if (bHadActiveAttack)
	{
		OnAttackEnded.Broadcast(true);
	}
	return true;
}

void UZCCombatComponent::ResumeAfterHitReaction()
{
	if (CombatAvailability != EZCCombatAvailability::Reacting)
	{
		return;
	}

	CombatAvailability = EZCCombatAvailability::Enabled;
	if (IsGuardDesired() && WeaponState == EZCWeaponState::Equipped)
	{
		StartGuard();
	}
	else if (WeaponState == EZCWeaponState::Equipped)
	{
		ScheduleAutoSheath();
	}
}

void UZCCombatComponent::DisableCombat()
{
	if (CombatAvailability == EZCCombatAvailability::Disabled)
	{
		return;
	}

	// 先锁定终止状态，让 Montage_Stop 触发的旧回调无法恢复任何战斗动作
	CombatAvailability = EZCCombatAvailability::Disabled;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	const bool bHadActiveAttack = FinishAttack();
	ClearDrawMontageEndDelegate();
	StopDefenseMontage();
	ClearDefenseTimers();
	ResetGuardBlockCount();
	DefenseState = EZCDefenseState::Normal;
	bTargetLockActive = false;
	bGuardSuppressed = false;
	WeaponState = AnimationAttachmentState == EZCWeaponAttachmentState::Equipped
		? EZCWeaponState::Equipped
		: EZCWeaponState::Sheathed;
	if (CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			ClearAttackMontageEndDelegate(AnimInstance);
			AnimInstance->Montage_Stop(0.05f);
		}
	}
	if (bActivePlayerAttackCombo)
	{
		ResetPlayerAttackCombo();
	}
	bActivePlayerAttackCombo = false;
	bPlayerAttackQueued = false;
	bPlayerAttackTraceWindowEnded = false;
	ActiveAttackMontage = nullptr;
	if (bHadActiveAttack)
	{
		OnAttackEnded.Broadcast(true);
	}
}

void UZCCombatComponent::ClearAttackMontageEndDelegate(UAnimInstance* AnimInstance)
{
	if (AnimInstance && ActiveAttackMontage && AnimInstance->Montage_IsPlaying(ActiveAttackMontage.Get()))
	{
		FOnMontageEnded EmptyEndDelegate;
		AnimInstance->Montage_SetEndDelegate(EmptyEndDelegate, ActiveAttackMontage.Get());
	}
}

void UZCCombatComponent::DisableTraceTick()
{
	if (IsComponentTickEnabled())
	{
		SetComponentTickEnabled(false);
	}
}

bool UZCCombatComponent::GetTraceSocketLocations(FVector& OutBase, FVector& OutTip)
{
	if (!TraceSourceMesh)
	{
		if (!bTraceConfigurationWarningLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("ZCCombatComponent: Attack trace skipped because the trace source is missing on %s."), *GetNameSafe(GetOwner()));
			bTraceConfigurationWarningLogged = true;
		}
		return false;
	}

	const USkeletalMeshComponent* SkeletalTraceSource = Cast<USkeletalMeshComponent>(TraceSourceMesh.Get());
	const bool bBaseExists = !TraceBaseSocket.IsNone()
		&& (TraceSourceMesh->DoesSocketExist(TraceBaseSocket)
			|| (SkeletalTraceSource && SkeletalTraceSource->GetBoneIndex(TraceBaseSocket) != INDEX_NONE));
	const bool bTipExists = !TraceTipSocket.IsNone()
		&& (TraceSourceMesh->DoesSocketExist(TraceTipSocket)
			|| (SkeletalTraceSource && SkeletalTraceSource->GetBoneIndex(TraceTipSocket) != INDEX_NONE));
	if (!bBaseExists || !bTipExists)
	{
		if (!bTraceConfigurationWarningLogged)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("ZCCombatComponent: Attack trace skipped on %s because points '%s'/'%s' are missing from %s."),
				*GetNameSafe(GetOwner()),
				*TraceBaseSocket.ToString(),
				*TraceTipSocket.ToString(),
				*GetNameSafe(TraceSourceMesh));
			bTraceConfigurationWarningLogged = true;
		}
		return false;
	}

	OutBase = SkeletalTraceSource && !TraceSourceMesh->DoesSocketExist(TraceBaseSocket)
		? SkeletalTraceSource->GetBoneLocation(TraceBaseSocket, EBoneSpaces::WorldSpace)
		: TraceSourceMesh->GetSocketLocation(TraceBaseSocket);
	OutTip = SkeletalTraceSource && !TraceSourceMesh->DoesSocketExist(TraceTipSocket)
		? SkeletalTraceSource->GetBoneLocation(TraceTipSocket, EBoneSpaces::WorldSpace)
		: TraceSourceMesh->GetSocketLocation(TraceTipSocket);
	return true;
}

void UZCCombatComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bTraceActive)
	{
		// 防御性处理：即使外部误留 Tick 开启，也不能在窗口外进行 Sweep
		DisableTraceTick();
		return;
	}

	if (TraceDamage <= 0.0f || TraceRadius <= 0.0f)
	{
		if (!bTraceConfigurationWarningLogged)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("ZCCombatComponent: Weapon trace disabled on %s because Damage (%.2f) and Radius (%.2f) must be positive."),
				*GetNameSafe(GetOwner()),
				TraceDamage,
				TraceRadius);
			bTraceConfigurationWarningLogged = true;
		}
		EndTrace();
		return;
	}

	FVector CurrentBase;
	FVector CurrentTip;
	if (!GetTraceSocketLocations(CurrentBase, CurrentTip))
	{
		EndTrace();
		return;
	}

	if (!bHasPreviousTracePositions)
	{
		// 首帧只补齐基线，避免 BeginTrace 之后的第一帧把整把剑当成运动轨迹
		PreviousTraceBase = CurrentBase;
		PreviousTraceTip = CurrentTip;
		bHasPreviousTracePositions = true;
		return;
	}

	TArray<FVector> PreviousSamples;
	TArray<FVector> CurrentSamples;
	BuildTraceSamplePositions(
		PreviousTraceBase,
		PreviousTraceTip,
		CurrentBase,
		CurrentTip,
		TraceSampleSegments,
		PreviousSamples,
		CurrentSamples);

	UWorld* World = GetWorld();
	if (!World)
	{
		EndTrace();
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZCWeaponTrace), false, GetOwner());
	if (TraceSourceMesh)
	{
		QueryParams.AddIgnoredComponent(TraceSourceMesh.Get());
	}
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(TraceRadius);

	for (int32 SampleIndex = 0; SampleIndex < CurrentSamples.Num(); ++SampleIndex)
	{
		const FVector& Start = PreviousSamples[SampleIndex];
		const FVector& End = CurrentSamples[SampleIndex];
		TArray<FHitResult> Hits;
		const bool bHit = World->SweepMultiByChannel(
			Hits,
			Start,
			End,
			FQuat::Identity,
			TraceChannel,
			SweepShape,
			QueryParams);

		if (bDebugDrawTrace)
		{
			DrawDebugLine(World, Start, End, bHit ? FColor::Red : FColor::Green, false, 0.1f, 0, 1.5f);
			DrawDebugSphere(World, End, TraceRadius, 8, bHit ? FColor::Red : FColor::Yellow, false, 0.1f, 0, 1.0f);
		}

		for (const FHitResult& Hit : Hits)
		{
			// 所有伤害统一经过 TryApplyHit，集中处理攻击窗口、Owner 排除和同次攻击去重
			TryApplyHit(Hit, TraceDamage);
		}
	}

	PreviousTraceBase = CurrentBase;
	PreviousTraceTip = CurrentTip;
}

FZCCombatHitResult UZCCombatComponent::TryApplyHit(const FHitResult& Hit, const float DamageAmount)
{
	FZCCombatHitResult Result;
	AActor* Target = Hit.GetActor();
	if (!CanAcceptCombatInput() || !bAttackActive || !bTraceActive || !IsValid(Target)
		|| !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return Result;
	}

	AActor* Owner = GetOwner();
	if (Owner && Target == Owner)
	{
		return Result;
	}

	if (bPlayerOnlyDamage)
	{
		const APawn* TargetPawn = Cast<APawn>(Target);
		if (!TargetPawn || !TargetPawn->IsPlayerControlled())
		{
			return Result;
		}
	}

	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (HitActors.Contains(TargetKey))
	{
		// 同一次攻击对同一 Actor 只允许一次伤害，防止连续帧重复命中
		return Result;
	}

	HitActors.Add(TargetKey);
	Result.bRegistered = true;
	Result.Target = Target;
	Result.RequestedDamage = DamageAmount;
	Result.ImpactPoint = Hit.ImpactPoint;
	Result.ImpactNormal = Hit.ImpactNormal;
	Result.HitBoneName = Hit.BoneName;

	UZCAttributeComponent* TargetAttributes = Target->FindComponentByClass<UZCAttributeComponent>();
	const bool bTargetWasDead = TargetAttributes && TargetAttributes->IsDead();
	AController* InstigatorController = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	FVector ShotDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
	if (ShotDirection.IsNearlyZero())
	{
		ShotDirection = Owner ? Owner->GetActorForwardVector() : FVector::ForwardVector;
	}

	Result.AppliedDamage = UGameplayStatics::ApplyPointDamage(
		Target,
		DamageAmount,
		ShotDirection,
		Hit,
		InstigatorController,
		Owner,
		UDamageType::StaticClass());
	Result.bBecameDead = TargetAttributes && !bTargetWasDead && TargetAttributes->IsDead();
	OnHitResolved.Broadcast(Result);
	return Result;
}
