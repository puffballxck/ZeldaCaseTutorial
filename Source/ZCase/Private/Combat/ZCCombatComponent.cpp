// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/ZCAttributeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UZCCombatComponent::UZCCombatComponent()
{
	// Trace 只在 NotifyState 打开的攻击窗口内 Tick，避免常态下产生无意义的碰撞查询。
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// 在动画和角色移动更新后读取附着武器的位置，避免 Sweep 使用上一阶段的骨骼变换。
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

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
	// 重新绑定装备时切断旧攻击生命周期，避免旧的 Notify/Tick 继续使用已失效的 socket。
	EndTrace();
	bAttackActive = false;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
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
	if (!CanAcceptCombatInput())
	{
		return false;
	}

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
	if (!CanAcceptCombatInput())
	{
		return false;
	}

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
	if (!CanAcceptCombatInput())
	{
		return false;
	}

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
	if (!CanAcceptCombatInput() || WeaponState != EZCWeaponState::Equipped)
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
	if (Montage != DrawSwordMontage || WeaponState != EZCWeaponState::Drawing)
	{
		return;
	}
	ClearAttachmentTimer();

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
	if (Montage != AttackMontage || WeaponState != EZCWeaponState::Attacking)
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
	if (Montage != SheathSwordMontage || WeaponState != EZCWeaponState::Sheathing)
	{
		return;
	}
	ClearAttachmentTimer();

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
	// 受击/死亡后忽略旧攻击 Montage 迟到的挂点 Notify。
	if (!CanAcceptCombatInput())
	{
		return;
	}

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

int32 UZCCombatComponent::NormalizeTraceSampleSegments(const int32 RequestedSegments)
{
	// 上限是保护性约束：Sweep 数量应由动画配置控制，但不能因误填值拖垮每帧查询。
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
	if (!CanAcceptCombatInput() || WeaponState != EZCWeaponState::Equipped || AutoSheathDelay <= 0.0f || !GetWorld())
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
	if (CanAcceptCombatInput() && WeaponState == EZCWeaponState::Equipped)
	{
		// 只从稳定的 Equipped 状态触发自动收刀，过渡期间的旧 Timer 无效。
		RequestSheath();
	}
}

void UZCCombatComponent::FinishAttack()
{
	// 结束攻击时同时关闭生命周期、命中窗口和 Tick；命中集合在下一次攻击开始时清空。
	EndTrace();
	bAttackActive = false;
}

void UZCCombatComponent::StartAttack()
{
	if (!CanAcceptCombatInput())
	{
		return;
	}

	// 新攻击接管前先关闭旧窗口，保证不会把上一攻击的 Tick/基线带入本次攻击。
	EndTrace();
	bAttackActive = true;
	// 每次攻击独立去重，允许同一目标在下一次攻击再次受击。
	HitActors.Reset();
}

bool UZCCombatComponent::BeginTrace()
{
	if (!CanAcceptCombatInput() || !bAttackActive || !GetTraceSocketLocations(PreviousTraceBase, PreviousTraceTip))
	{
		return false;
	}

	// NotifyState 的 Begin 是唯一打开窗口的入口；先建立上一帧基线，避免武器从挂点瞬移时误扫整段路径。
	bHasPreviousTracePositions = true;
	bTraceConfigurationWarningLogged = false;
	bTraceActive = true;
	SetComponentTickEnabled(true);
	return true;
}

void UZCCombatComponent::EndTrace()
{
	// 关闭窗口后即使攻击仍未结束，也不再接受命中；同时关闭 Tick，形成窗口生命周期不变量。
	bTraceActive = false;
	bHasPreviousTracePositions = false;
	DisableTraceTick();
}

void UZCCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 角色销毁/PIE 停止可能绕过 Montage 回调，必须在组件生命周期边界强制关闭残留 Trace。
	EndTrace();
	bAttackActive = false;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	CombatAvailability = EZCCombatAvailability::Disabled;
	Super::EndPlay(EndPlayReason);
}

bool UZCCombatComponent::InterruptForHitReaction()
{
	if (CombatAvailability == EZCCombatAvailability::Disabled)
	{
		return false;
	}

	// 连续受击由角色重置同一 Montage 的位置；这里保持锁定且不打断该 Montage。
	if (CombatAvailability == EZCCombatAvailability::Reacting)
	{
		return true;
	}

	CombatAvailability = EZCCombatAvailability::Reacting;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	FinishAttack();
	WeaponState = AnimationAttachmentState == EZCWeaponAttachmentState::Equipped
		? EZCWeaponState::Equipped
		: EZCWeaponState::Sheathed;

	if (CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.05f);
		}
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
	if (WeaponState == EZCWeaponState::Equipped)
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

	// 先锁定终止状态，让 Montage_Stop 触发的旧回调无法恢复任何战斗动作。
	CombatAvailability = EZCCombatAvailability::Disabled;
	ClearAutoSheathTimer();
	ClearAttachmentTimer();
	FinishAttack();
	WeaponState = AnimationAttachmentState == EZCWeaponAttachmentState::Equipped
		? EZCWeaponState::Equipped
		: EZCWeaponState::Sheathed;

	if (CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			AnimInstance->Montage_Stop(0.05f);
		}
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
	if (!SwordMesh)
	{
		if (!bTraceConfigurationWarningLogged)
		{
			UE_LOG(LogTemp, Warning, TEXT("ZCCombatComponent: Weapon trace skipped because SwordMesh is missing on %s."), *GetNameSafe(GetOwner()));
			bTraceConfigurationWarningLogged = true;
		}
		return false;
	}

	if (TraceBaseSocket.IsNone() || TraceTipSocket.IsNone()
		|| !SwordMesh->DoesSocketExist(TraceBaseSocket)
		|| !SwordMesh->DoesSocketExist(TraceTipSocket))
	{
		if (!bTraceConfigurationWarningLogged)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("ZCCombatComponent: Weapon trace skipped on %s because sockets '%s'/'%s' are missing from %s."),
				*GetNameSafe(GetOwner()),
				*TraceBaseSocket.ToString(),
				*TraceTipSocket.ToString(),
				*GetNameSafe(SwordMesh));
			bTraceConfigurationWarningLogged = true;
		}
		return false;
	}

	OutBase = SwordMesh->GetSocketLocation(TraceBaseSocket);
	OutTip = SwordMesh->GetSocketLocation(TraceTipSocket);
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
		// 防御性处理：即使外部误留 Tick 开启，也不能在窗口外进行 Sweep。
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
		// 首帧只补齐基线，避免 BeginTrace 之后的第一帧把整把剑当成运动轨迹。
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
	if (SwordMesh)
	{
		QueryParams.AddIgnoredComponent(SwordMesh.Get());
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
			// 所有伤害统一经过 TryApplyHit，集中处理攻击窗口、Owner 排除和同次攻击去重。
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

	const TWeakObjectPtr<AActor> TargetKey(Target);
	if (HitActors.Contains(TargetKey))
	{
		// 同一次攻击对同一 Actor 只允许一次伤害，防止连续帧重复命中。
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
