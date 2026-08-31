// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Characters/ZCCharBase.h"
#include "Characters/ZCEnemyBase.h"
#include "Animation/AnimMontage.h"
#include "Animations/ZCAnimInst.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace
{
	AActor* SpawnTargetLockVisibilityBlocker(UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return nullptr;
		}

		AActor* Blocker = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
		if (!Blocker)
		{
			return nullptr;
		}

		UBoxComponent* Box = NewObject<UBoxComponent>(Blocker, TEXT("TargetLockVisibilityBlocker"));
		if (!Box)
		{
			Blocker->Destroy();
			return nullptr;
		}

		Blocker->AddInstanceComponent(Box);
		Blocker->SetRootComponent(Box);
		Box->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Box->RegisterComponent();
		// Actor 在没有 RootComponent 时 Spawn，随后才挂入 Box；显式设置世界位置，
		// 避免测试遮挡体仍停留在原点而没有进入目标锁定射线。
		Box->SetWorldLocation(Location);
		return Blocker;
	}
}

// 属性测试覆盖最大生命值/当前生命值钳制，以及死亡事件只允许完成一次。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCAttributeClampingTest,
	"ZCase.Attributes.Clamping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCAttributeClampingTest::RunTest(const FString& Parameters)
{
	UZCAttributeComponent* Attributes = NewObject<UZCAttributeComponent>();
	Attributes->InitializeAttributes(0.0f, 250.0f);
	TestEqual(TEXT("Maximum health has a lower bound"), Attributes->GetMaxHealth(), 1.0f);
	TestEqual(TEXT("Health is clamped to maximum"), Attributes->GetHealth(), 1.0f);

	Attributes->InitializeAttributes(100.0f, -20.0f);
	TestEqual(TEXT("Health is clamped to zero"), Attributes->GetHealth(), 0.0f);
	TestTrue(TEXT("Zero health is dead"), Attributes->IsDead());
	return true;
}

// 伤害测试确认负伤害不治疗、致死伤害被截断，并且死亡后的重复伤害不会生效。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCAttributeDamageAndDeathTest,
	"ZCase.Attributes.DamageAndDeathAreIdempotent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCAttributeDamageAndDeathTest::RunTest(const FString& Parameters)
{
	UZCAttributeComponent* Attributes = NewObject<UZCAttributeComponent>();
	Attributes->InitializeAttributes(100.0f, 25.0f);

	const FZCDamageResult NegativeDamage = Attributes->ApplyDamage(-10.0f);
	TestEqual(TEXT("Negative damage is ignored"), NegativeDamage.AppliedDamage, 0.0f);
	TestEqual(TEXT("Negative damage does not heal"), Attributes->GetHealth(), 25.0f);

	const FZCDamageResult LethalDamage = Attributes->ApplyDamage(1000.0f);
	TestEqual(TEXT("Damage is limited to remaining health"), LethalDamage.AppliedDamage, 25.0f);
	TestEqual(TEXT("Lethal damage clamps health to zero"), Attributes->GetHealth(), 0.0f);
	TestTrue(TEXT("The lethal transition is reported once"), LethalDamage.bBecameDead);

	const FZCDamageResult RepeatedDamage = Attributes->ApplyDamage(10.0f);
	TestEqual(TEXT("Damage after death is ignored"), RepeatedDamage.AppliedDamage, 0.0f);
	TestFalse(TEXT("Death is not reported twice"), RepeatedDamage.bBecameDead);
	return true;
}

// 敌人伤害生命周期测试覆盖非致命受击、首次死亡，以及死亡后的不可伤害和碰撞策略。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCEnemyDamageLifecycleTest,
	"ZCase.Enemy.DamageLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCEnemyDamageLifecycleTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("The enemy test world can be created"), TestWorld.CreateTestWorld(EWorldType::Game));
	TestTrue(TEXT("The enemy test world can begin play"), TestWorld.BeginPlayInTestWorld());
	UWorld* World = TestWorld.GetTestWorld();
	TestNotNull(TEXT("The enemy test world is valid"), World);
	if (!World)
	{
		return false;
	}

	AZCEnemyBase* Enemy = World->SpawnActor<AZCEnemyBase>();
	TestTrue(TEXT("The enemy fixture is valid"), IsValid(Enemy));
	if (!Enemy)
	{
		return false;
	}

	TestNotNull(TEXT("The enemy owns attributes"), Enemy->Attributes.Get());
	if (!Enemy->Attributes)
	{
		return false;
	}

	TestEqual(TEXT("The enemy starts with full health"), Enemy->Attributes->GetHealth(), 100.0f);
	TestTrue(TEXT("A living enemy can be target locked"), Enemy->CanBeTargetLocked());
	TestEqual(
		TEXT("A living enemy blocks the Pawn channel"),
		Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn),
		ECR_Block);

	AddExpectedError(TEXT("cannot play Hit React"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("cannot play Death Montage"), EAutomationExpectedErrorFlags::Contains, 1);
	FDamageEvent DamageEvent;
	const float NonLethalDamage = Enemy->TakeDamage(25.0f, DamageEvent, nullptr, nullptr);
	TestEqual(TEXT("Non-lethal damage is applied"), NonLethalDamage, 25.0f);
	TestEqual(TEXT("Non-lethal damage reduces health"), Enemy->Attributes->GetHealth(), 75.0f);
	TestTrue(TEXT("A surviving enemy remains targetable"), Enemy->CanBeTargetLocked());
	TestEqual(
		TEXT("A surviving enemy still blocks the Pawn channel"),
		Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn),
		ECR_Block);

	const float LethalDamage = Enemy->TakeDamage(1000.0f, DamageEvent, nullptr, nullptr);
	TestEqual(TEXT("Lethal damage is limited to remaining health"), LethalDamage, 75.0f);
	TestEqual(TEXT("Lethal damage clamps health to zero"), Enemy->Attributes->GetHealth(), 0.0f);
	TestFalse(TEXT("A dead enemy cannot be target locked"), Enemy->CanBeTargetLocked());
	TestEqual(
		TEXT("A dead enemy has movement disabled"),
		Enemy->GetCharacterMovement()->MovementMode,
		MOVE_None);
	TestEqual(
		TEXT("A dead enemy ignores the Pawn channel"),
		Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn),
		ECR_Ignore);

	const float RepeatedDamage = Enemy->TakeDamage(10.0f, DamageEvent, nullptr, nullptr);
	TestEqual(TEXT("Damage after death is ignored"), RepeatedDamage, 0.0f);
	TestEqual(TEXT("Repeated damage keeps health at zero"), Enemy->Attributes->GetHealth(), 0.0f);
	TestEqual(
		TEXT("Repeated damage does not restore Pawn collision"),
		Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn),
		ECR_Ignore);
	return true;
}

// 玩家非致死受击应恢复战斗；首次死亡则清理锁定并进入不可逆的终止状态。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCPlayerDamageLifecycleTest,
	"ZCase.Player.DamageLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCPlayerDamageLifecycleTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("The player test world can be created"), TestWorld.CreateTestWorld(EWorldType::Game));
	TestTrue(TEXT("The player test world can begin play"), TestWorld.BeginPlayInTestWorld());
	UWorld* World = TestWorld.GetTestWorld();
	TestNotNull(TEXT("The player test world is valid"), World);
	if (!World)
	{
		return false;
	}

	AZCCharBase* Player = World->SpawnActor<AZCCharBase>();
	AZCEnemyBase* LockTarget = World->SpawnActor<AZCEnemyBase>();
	TestNotNull(TEXT("The player fixture is valid"), Player);
	TestNotNull(TEXT("The lock target fixture is valid"), LockTarget);
	if (!Player || !LockTarget || !Player->Attributes || !Player->Combat || !Player->TargetLock)
	{
		return false;
	}

	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	TestTrue(TEXT("A living player can acquire a target"), Player->TargetLock->SetTarget(LockTarget));
	AddExpectedError(TEXT("cannot play Hit React"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("cannot play Death Montage"), EAutomationExpectedErrorFlags::Contains, 1);
	FDamageEvent DamageEvent;
	const float NonLethalDamage = Player->TakeDamage(25.0f, DamageEvent, nullptr, LockTarget);
	TestEqual(TEXT("Non-lethal player damage is applied"), NonLethalDamage, 25.0f);
	TestEqual(TEXT("Non-lethal player damage reduces health"), Player->Attributes->GetHealth(), 75.0f);
	TestEqual(
		TEXT("A missing hit montage cannot leave combat locked"),
		Player->Combat->GetCombatAvailability(),
		EZCCombatAvailability::Enabled);
	TestEqual(TEXT("Non-lethal hit reaction preserves target lock"), Player->TargetLock->GetCurrentTarget(), static_cast<AActor*>(LockTarget));
	TestEqual(
		TEXT("Non-lethal hit reaction preserves movement"),
		Player->GetCharacterMovement()->MovementMode,
		MOVE_Walking);

	const float LethalDamage = Player->TakeDamage(1000.0f, DamageEvent, nullptr, LockTarget);
	TestEqual(TEXT("Lethal player damage is limited to remaining health"), LethalDamage, 75.0f);
	TestTrue(TEXT("Player death starts once"), Player->IsDeathStarted());
	TestFalse(TEXT("A dead player cannot be damaged"), Player->CanBeDamaged());
	TestFalse(TEXT("A dead player cannot be target locked"), Player->CanBeTargetLocked());
	TestNull(TEXT("Player death clears the current target"), Player->TargetLock->GetCurrentTarget());
	TestEqual(
		TEXT("Player death terminally disables combat"),
		Player->Combat->GetCombatAvailability(),
		EZCCombatAvailability::Disabled);
	TestEqual(TEXT("Player death disables movement"), Player->GetCharacterMovement()->MovementMode, MOVE_None);
	TestEqual(
		TEXT("Player death ignores the Pawn channel"),
		Player->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn),
		ECR_Ignore);

	Player->Combat->ResumeAfterHitReaction();
	TestEqual(
		TEXT("A stale reaction callback cannot revive player combat"),
		Player->Combat->GetCombatAvailability(),
		EZCCombatAvailability::Disabled);
	const float RepeatedDamage = Player->TakeDamage(10.0f, DamageEvent, nullptr, LockTarget);
	TestEqual(TEXT("Repeated player damage after death is ignored"), RepeatedDamage, 0.0f);
	return true;
}

// 命中窗口测试确认攻击生命周期、窗口开关和同一攻击内的目标去重边界。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCCombatHitWindowTest,
	"ZCase.Combat.HitWindowAndDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCCombatHitWindowTest::RunTest(const FString& Parameters)
{
	// 为命中窗口测试提供最小的瞬态武器和两个 socket，避免把“缺少配置”的安全失败与正常去重逻辑混在一起。
	AActor* Owner = NewObject<AActor>();
	UStaticMeshComponent* SwordMesh = NewObject<UStaticMeshComponent>(Owner);
	UStaticMesh* RuntimeMesh = NewObject<UStaticMesh>(Owner);
	UStaticMeshSocket* BaseSocket = NewObject<UStaticMeshSocket>(RuntimeMesh);
	BaseSocket->SocketName = TEXT("Trace_Base");
	BaseSocket->RelativeLocation = FVector::ZeroVector;
	RuntimeMesh->AddSocket(BaseSocket);
	UStaticMeshSocket* TipSocket = NewObject<UStaticMeshSocket>(RuntimeMesh);
	TipSocket->SocketName = TEXT("Trace_Tip");
	TipSocket->RelativeLocation = FVector(0.0f, 0.0f, 100.0f);
	RuntimeMesh->AddSocket(TipSocket);
	SwordMesh->SetStaticMesh(RuntimeMesh);
	UZCCombatComponent* Combat = NewObject<UZCCombatComponent>(Owner);
	Combat->InitializeEquipment(nullptr, SwordMesh, nullptr, nullptr);
	AActor* Target = NewObject<AActor>();
	FHitResult Hit(Target, nullptr, FVector(10.0f, 20.0f, 30.0f), FVector::UpVector);
	Hit.TraceStart = FVector::ZeroVector;
	Hit.TraceEnd = FVector(100.0f, 0.0f, 0.0f);
	Hit.BoneName = TEXT("spine_01");
	FHitResult InvalidHit;

	TestFalse(TEXT("An invalid target is rejected"), Combat->TryApplyHit(InvalidHit, 10.0f).bRegistered);
	TestFalse(TEXT("A hit before an attack is rejected"), Combat->TryApplyHit(Hit, 10.0f).bRegistered);
	Combat->StartAttack();
	TestFalse(TEXT("A hit before the trace window is rejected"), Combat->TryApplyHit(Hit, 10.0f).bRegistered);
	TestTrue(TEXT("An attack can open its trace window"), Combat->BeginTrace());
	const FZCCombatHitResult FirstResult = Combat->TryApplyHit(Hit, 10.0f);
	TestTrue(TEXT("The first hit is registered"), FirstResult.bRegistered);
	TestEqual(TEXT("The hit target is preserved"), FirstResult.Target.Get(), Target);
	TestEqual(TEXT("Requested damage is preserved"), FirstResult.RequestedDamage, 10.0f);
	TestEqual(TEXT("Impact point is preserved"), FirstResult.ImpactPoint, Hit.ImpactPoint);
	TestEqual(TEXT("Impact normal is preserved"), FirstResult.ImpactNormal, Hit.ImpactNormal);
	TestEqual(TEXT("Hit bone is preserved"), FirstResult.HitBoneName, Hit.BoneName);
	TestFalse(TEXT("The same attack cannot hit one target twice"), Combat->TryApplyHit(Hit, 10.0f).bRegistered);
	Combat->EndTrace();
	TestFalse(TEXT("A closed trace window rejects hits"), Combat->TryApplyHit(Hit, 10.0f).bRegistered);

	Combat->StartAttack();
	// 新攻击必须重置去重集合，使同一目标可以再次受击。
	Combat->BeginTrace();
	TestTrue(TEXT("A new attack clears the hit set"), Combat->TryApplyHit(Hit, 10.0f).bRegistered);
	return true;
}

// 命中结算测试覆盖实际扣血、致死转移和跨攻击重新登记。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCCombatHitResolutionTest,
	"ZCase.Combat.HitResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCCombatHitResolutionTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("The hit-resolution world can be created"), TestWorld.CreateTestWorld(EWorldType::Game));
	TestTrue(TEXT("The hit-resolution world can begin play"), TestWorld.BeginPlayInTestWorld());
	UWorld* World = TestWorld.GetTestWorld();
	if (!World)
	{
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	UStaticMeshComponent* SwordMesh = NewObject<UStaticMeshComponent>(Owner);
	UStaticMesh* RuntimeMesh = NewObject<UStaticMesh>(Owner);
	UStaticMeshSocket* BaseSocket = NewObject<UStaticMeshSocket>(RuntimeMesh);
	BaseSocket->SocketName = TEXT("Trace_Base");
	RuntimeMesh->AddSocket(BaseSocket);
	UStaticMeshSocket* TipSocket = NewObject<UStaticMeshSocket>(RuntimeMesh);
	TipSocket->SocketName = TEXT("Trace_Tip");
	TipSocket->RelativeLocation = FVector(0.0f, 0.0f, 100.0f);
	RuntimeMesh->AddSocket(TipSocket);
	SwordMesh->SetStaticMesh(RuntimeMesh);
	Owner->AddInstanceComponent(SwordMesh);
	SwordMesh->RegisterComponent();

	UZCCombatComponent* Combat = NewObject<UZCCombatComponent>(Owner);
	Owner->AddInstanceComponent(Combat);
	Combat->RegisterComponent();
	Combat->InitializeEquipment(nullptr, SwordMesh, nullptr, nullptr);
	AZCEnemyBase* Target = World->SpawnActor<AZCEnemyBase>();
	FHitResult Hit(Target, nullptr, FVector(5.0f, 10.0f, 15.0f), FVector::BackwardVector);
	Hit.TraceStart = FVector::ZeroVector;
	Hit.TraceEnd = FVector(100.0f, 0.0f, 0.0f);

	AddExpectedError(TEXT("cannot play Hit React"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("cannot play Death Montage"), EAutomationExpectedErrorFlags::Contains, 1);
	Combat->StartAttack();
	TestTrue(TEXT("The first resolution opens a trace window"), Combat->BeginTrace());
	const FZCCombatHitResult NonLethal = Combat->TryApplyHit(Hit, 25.0f);
	TestTrue(TEXT("A non-lethal contact is registered"), NonLethal.bRegistered);
	TestEqual(TEXT("A non-lethal hit reports actual damage"), NonLethal.AppliedDamage, 25.0f);
	TestFalse(TEXT("A non-lethal hit does not report death"), NonLethal.bBecameDead);
	TestEqual(TEXT("A non-lethal hit changes target health"), Target->Attributes->GetHealth(), 75.0f);

	Combat->EndTrace();
	Combat->StartAttack();
	TestTrue(TEXT("The lethal resolution opens a new trace window"), Combat->BeginTrace());
	const FZCCombatHitResult Lethal = Combat->TryApplyHit(Hit, 1000.0f);
	TestTrue(TEXT("The next attack can register the target again"), Lethal.bRegistered);
	TestEqual(TEXT("Lethal damage is limited to remaining health"), Lethal.AppliedDamage, 75.0f);
	TestTrue(TEXT("The lethal transition is exposed by combat"), Lethal.bBecameDead);
	TestEqual(TEXT("The target reaches zero health"), Target->Attributes->GetHealth(), 0.0f);
	return true;
}

// 受击锁定保留可恢复性，而死亡禁用必须拒绝所有陈旧恢复调用。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCCombatAvailabilityTest,
	"ZCase.Combat.ReactionAvailability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCCombatAvailabilityTest::RunTest(const FString& Parameters)
{
	UZCCombatComponent* Combat = NewObject<UZCCombatComponent>();
	TestEqual(TEXT("Combat begins enabled"), Combat->GetCombatAvailability(), EZCCombatAvailability::Enabled);
	TestTrue(TEXT("Hit reaction can interrupt enabled combat"), Combat->InterruptForHitReaction());
	TestEqual(TEXT("Hit reaction enters reacting state"), Combat->GetCombatAvailability(), EZCCombatAvailability::Reacting);
	Combat->StartAttack();
	TestFalse(TEXT("Reacting combat rejects attack lifecycle"), Combat->IsAttackActive());
	TestFalse(TEXT("Reacting combat rejects trace windows"), Combat->BeginTrace());

	Combat->ResumeAfterHitReaction();
	TestEqual(TEXT("Reaction completion restores combat"), Combat->GetCombatAvailability(), EZCCombatAvailability::Enabled);
	Combat->StartAttack();
	TestTrue(TEXT("Restored combat accepts an attack lifecycle"), Combat->IsAttackActive());

	Combat->DisableCombat();
	TestEqual(TEXT("Death disables combat"), Combat->GetCombatAvailability(), EZCCombatAvailability::Disabled);
	TestFalse(TEXT("Disabled combat closes the attack lifecycle"), Combat->IsAttackActive());
	Combat->InitializeEquipment(nullptr, nullptr, nullptr, nullptr);
	TestEqual(
		TEXT("Equipment reinitialization cannot revive disabled combat"),
		Combat->GetCombatAvailability(),
		EZCCombatAvailability::Disabled);
	Combat->ResumeAfterHitReaction();
	Combat->StartAttack();
	TestEqual(TEXT("A stale resume cannot re-enable combat"), Combat->GetCombatAvailability(), EZCCombatAvailability::Disabled);
	TestFalse(TEXT("Disabled combat continues to reject attacks"), Combat->IsAttackActive());
	return true;
}

// 武器轨迹采样测试保护配置钳制、采样数量，以及上一帧/当前帧的剑身端点插值。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCCombatTraceSamplingTest,
	"ZCase.Combat.TraceSampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCCombatTraceSamplingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Trace segments have a safe lower bound"), UZCCombatComponent::NormalizeTraceSampleSegments(0), 1);
	TestEqual(TEXT("Trace segments have a safe upper bound"), UZCCombatComponent::NormalizeTraceSampleSegments(100), 32);
	TestEqual(TEXT("Trace segments preserve valid configuration"), UZCCombatComponent::NormalizeTraceSampleSegments(4), 4);

	TArray<FVector> PreviousSamples;
	TArray<FVector> CurrentSamples;
	UZCCombatComponent::BuildTraceSamplePositions(
		FVector(0.0f, 0.0f, 0.0f),
		FVector(0.0f, 0.0f, 100.0f),
		FVector(10.0f, 0.0f, 0.0f),
		FVector(10.0f, 0.0f, 100.0f),
		4,
		PreviousSamples,
		CurrentSamples);

	TestEqual(TEXT("Sample count is segments plus endpoints"), PreviousSamples.Num(), 5);
	TestEqual(TEXT("Previous sample starts at the blade base"), PreviousSamples[0], FVector(0.0f, 0.0f, 0.0f));
	TestEqual(TEXT("Previous sample ends at the blade tip"), PreviousSamples.Last(), FVector(0.0f, 0.0f, 100.0f));
	TestEqual(TEXT("Current sample interpolates along the blade"), CurrentSamples[2], FVector(10.0f, 0.0f, 50.0f));
	return true;
}

// 输入策略测试固定 Sheathed -> Draw、Equipped -> Attack，其余过渡态忽略输入。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCWeaponInputPolicyTest,
	"ZCase.Combat.WeaponInputPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCWeaponInputPolicyTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Attack input draws a sheathed weapon"),
		UZCCombatComponent::ResolveAttackCommand(EZCWeaponState::Sheathed),
		EZCWeaponCommand::Draw);
	TestEqual(
		TEXT("Attack input attacks with an equipped weapon"),
		UZCCombatComponent::ResolveAttackCommand(EZCWeaponState::Equipped),
		EZCWeaponCommand::Attack);
	TestEqual(
		TEXT("Attack input is ignored while drawing"),
		UZCCombatComponent::ResolveAttackCommand(EZCWeaponState::Drawing),
		EZCWeaponCommand::None);
	TestEqual(
		TEXT("Attack input is ignored during an attack"),
		UZCCombatComponent::ResolveAttackCommand(EZCWeaponState::Attacking),
		EZCWeaponCommand::None);
	TestEqual(
		TEXT("Attack input is ignored while sheathing"),
		UZCCombatComponent::ResolveAttackCommand(EZCWeaponState::Sheathing),
		EZCWeaponCommand::None);
	return true;
}

// 动画基础姿势跟随实际挂点切换，不应等待 Drawing/Sheathing Montage 的结束回调。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCWeaponAnimationPoseStateTest,
	"ZCase.Combat.WeaponAnimationPoseState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCWeaponAnimationPoseStateTest::RunTest(const FString& Parameters)
{
	UZCCombatComponent* Combat = NewObject<UZCCombatComponent>();

	Combat->SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
	TestFalse(
		TEXT("Sheathed attachment selects the unarmed animation pose"),
		Combat->IsWeaponEquippedForAnimation());

	Combat->SetEquipmentAttachmentState(EZCWeaponAttachmentState::Equipped);
	TestTrue(
		TEXT("Equipped attachment selects the armed animation pose before montage end"),
		Combat->IsWeaponEquippedForAnimation());

	Combat->SetEquipmentAttachmentState(EZCWeaponAttachmentState::Sheathed);
	TestFalse(
		TEXT("Returning to the sheath selects the unarmed animation pose"),
		Combat->IsWeaponEquippedForAnimation());

	return true;
}

// 蒙太奇配置测试保护 ABP 使用的 FullBody 插槽，以及装备切换发生在动画结束前。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCWeaponMontageConfigurationTest,
	"ZCase.Combat.WeaponMontageConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCWeaponMontageConfigurationTest::RunTest(const FString& Parameters)
{
	const UAnimMontage* DrawMontage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_DrawSword.AM_DrawSword"));
	const UAnimMontage* SheathMontage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_SheathSword.AM_SheathSword"));
	const UAnimMontage* PlayerHitReactMontage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Player_HitReact.AM_Player_HitReact"));
	const UAnimMontage* PlayerDeathMontage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Player_Death.AM_Player_Death"));

	TestNotNull(TEXT("Draw montage exists"), DrawMontage);
	TestNotNull(TEXT("Sheath montage exists"), SheathMontage);
	TestNotNull(TEXT("Player hit react montage exists"), PlayerHitReactMontage);
	TestNotNull(TEXT("Player death montage exists"), PlayerDeathMontage);
	if (DrawMontage)
	{
		TestTrue(TEXT("Draw montage has a slot track"), !DrawMontage->SlotAnimTracks.IsEmpty());
		if (!DrawMontage->SlotAnimTracks.IsEmpty())
		{
			TestEqual(TEXT("Draw montage uses the ABP FullBody slot"), DrawMontage->SlotAnimTracks[0].SlotName, FName(TEXT("FullBody")));
		}
	}
	if (SheathMontage)
	{
		TestTrue(TEXT("Sheath montage has a slot track"), !SheathMontage->SlotAnimTracks.IsEmpty());
		if (!SheathMontage->SlotAnimTracks.IsEmpty())
		{
			TestEqual(TEXT("Sheath montage uses the ABP FullBody slot"), SheathMontage->SlotAnimTracks[0].SlotName, FName(TEXT("FullBody")));
		}
	}
	if (PlayerHitReactMontage)
	{
		TestTrue(TEXT("Player hit react montage has a slot track"), !PlayerHitReactMontage->SlotAnimTracks.IsEmpty());
		if (!PlayerHitReactMontage->SlotAnimTracks.IsEmpty())
		{
			TestEqual(
				TEXT("Player hit react montage uses the ABP FullBody slot"),
				PlayerHitReactMontage->SlotAnimTracks[0].SlotName,
				FName(TEXT("FullBody")));
		}
		TestTrue(TEXT("Player hit react montage automatically blends out"), PlayerHitReactMontage->bEnableAutoBlendOut);
	}
	if (PlayerDeathMontage)
	{
		TestTrue(TEXT("Player death montage has a slot track"), !PlayerDeathMontage->SlotAnimTracks.IsEmpty());
		if (!PlayerDeathMontage->SlotAnimTracks.IsEmpty())
		{
			TestEqual(
				TEXT("Player death montage uses the ABP FullBody slot"),
				PlayerDeathMontage->SlotAnimTracks[0].SlotName,
				FName(TEXT("FullBody")));
		}
		TestFalse(TEXT("Player death montage holds its final pose"), PlayerDeathMontage->bEnableAutoBlendOut);
	}

	const float DrawDelay = UZCCombatComponent::CalculateAttachmentDelay(1.24f, 0.35f);
	const float SheathDelay = UZCCombatComponent::CalculateAttachmentDelay(0.92f, 0.70f);
	// 默认 35% 拔刀、70% 收刀时刻对应可复现的 Timer 延迟。
	TestTrue(TEXT("Draw attachment switches before the montage ends"), DrawDelay > 0.0f && DrawDelay < 1.24f);
	TestTrue(TEXT("Sheath attachment switches before the montage ends"), SheathDelay > 0.0f && SheathDelay < 0.92f);
	TestEqual(TEXT("Draw attachment default time is stable"), DrawDelay, 0.434f, 0.001f);
	TestEqual(TEXT("Sheath attachment default time is stable"), SheathDelay, 0.644f, 0.001f);
	return true;
}

// 目标锁定生命周期测试覆盖接口校验、替换/清除和目标销毁后的自动清理。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCTargetLockLifecycleTest,
	"ZCase.TargetLock.ValidationAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCTargetLockLifecycleTest::RunTest(const FString& Parameters)
{
	UZCTargetLockComponent* TargetLock = NewObject<UZCTargetLockComponent>();
	AActor* NonTargetable = NewObject<AActor>();
	TestFalse(TEXT("An actor without the targetable interface is rejected"), TargetLock->SetTarget(NonTargetable));

	AZCCharBase* Targetable = NewObject<AZCCharBase>();
	TestTrue(TEXT("The targetable fixture is a valid UObject"), IsValid(Targetable));
	TestTrue(
		TEXT("The targetable fixture implements the targetable interface"),
		Targetable->GetClass()->ImplementsInterface(UZCTargetable::StaticClass()));
	TestNotNull(TEXT("The targetable fixture owns attributes"), Targetable->Attributes.Get());
	if (Targetable->Attributes)
	{
		TestEqual(TEXT("The targetable fixture starts with health"), Targetable->Attributes->GetHealth(), 100.0f);
	}
	TestTrue(
		TEXT("The native targetable implementation permits target lock"),
		Targetable->CanBeTargetLocked());
	TestTrue(TEXT("A targetable actor can be locked"), TargetLock->SetTarget(Targetable));
	TestEqual(TEXT("The current target is exposed"), TargetLock->GetCurrentTarget(), static_cast<AActor*>(Targetable));
	TargetLock->ClearTarget();
	TestNull(TEXT("A target can be cleared"), TargetLock->GetCurrentTarget());

	TestTrue(TEXT("The target can be locked again"), TargetLock->SetTarget(Targetable));
	Targetable->MarkAsGarbage();
	TestNull(TEXT("An invalidated target is cleared automatically"), TargetLock->GetCurrentTarget());
	return true;
}

// 目标获取测试覆盖默认范围/角度、屏幕中心角度优先、可见性和失败时的幂等性。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCTargetLockAcquisitionTest,
	"ZCase.TargetLock.AcquireBestTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCTargetLockAcquisitionTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("The target-lock acquisition world can be created"), TestWorld.CreateTestWorld(EWorldType::Game));
	TestTrue(TEXT("The target-lock acquisition world can begin play"), TestWorld.BeginPlayInTestWorld());
	UWorld* World = TestWorld.GetTestWorld();
	if (!World)
	{
		return false;
	}

	AZCCharBase* Player = World->SpawnActor<AZCCharBase>(FVector::ZeroVector, FRotator::ZeroRotator);
	AZCEnemyBase* CenterTarget = World->SpawnActor<AZCEnemyBase>(FVector(1200.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	AZCEnemyBase* OffCenterTarget = World->SpawnActor<AZCEnemyBase>(FVector(700.0f, 650.0f, 0.0f), FRotator::ZeroRotator);
	AZCEnemyBase* OutsideAngleTarget = World->SpawnActor<AZCEnemyBase>(FVector(500.0f, 1200.0f, 0.0f), FRotator::ZeroRotator);
	AZCEnemyBase* OutsideRadiusTarget = World->SpawnActor<AZCEnemyBase>(FVector(3000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("The acquisition player fixture is valid"), Player);
	TestNotNull(TEXT("The center target fixture is valid"), CenterTarget);
	TestNotNull(TEXT("The off-center target fixture is valid"), OffCenterTarget);
	TestNotNull(TEXT("The outside-angle target fixture is valid"), OutsideAngleTarget);
	TestNotNull(TEXT("The outside-radius target fixture is valid"), OutsideRadiusTarget);
	if (!Player || !CenterTarget || !OffCenterTarget || !OutsideAngleTarget || !OutsideRadiusTarget || !Player->TargetLock)
	{
		return false;
	}

	UZCTargetLockComponent* TargetLock = Player->TargetLock;
	TestEqual(TEXT("Default acquisition radius is 2500 cm"), TargetLock->AcquisitionRadius, 2500.0f);
	TestEqual(TEXT("Default acquisition half angle is 50 degrees"), TargetLock->AcquisitionHalfAngle, 50.0f);
	TestEqual(TEXT("Default lock-lost distance is 3000 cm"), TargetLock->LockLostDistance, 3000.0f);
	TestEqual(TEXT("Default occlusion grace period is 0.75 seconds"), TargetLock->OcclusionGracePeriod, 0.75f);
	TestEqual(TEXT("Default angle weight is 0.8"), TargetLock->AngleWeight, 0.8f);
	TestEqual(TEXT("Default distance weight is 0.2"), TargetLock->DistanceWeight, 0.2f);
	TestFalse(TEXT("The component does not tick before acquiring a target"), TargetLock->IsComponentTickEnabled());

	const FVector ViewLocation = FVector::ZeroVector;
	const FVector ViewForward = FVector::ForwardVector;
	TestTrue(TEXT("The acquisition finds a visible target"), TargetLock->AcquireBestTarget(ViewLocation, ViewForward));
	TestEqual(
		TEXT("The screen-center target wins over a nearer off-center target"),
		TargetLock->GetCurrentTarget(),
		static_cast<AActor*>(CenterTarget));
	TestTrue(TEXT("Acquiring a target enables component ticking"), TargetLock->IsComponentTickEnabled());

	// 获取失败不应破坏当前锁定；输入层只会在无目标时调用 AcquireBestTarget，但
	// 组件 API 也应保持这个失败路径的幂等性。
	TestFalse(TEXT("A zero view direction rejects acquisition"), TargetLock->AcquireBestTarget(ViewLocation, FVector::ZeroVector));
	TestEqual(
		TEXT("A failed acquisition preserves the current target"),
		TargetLock->GetCurrentTarget(),
		static_cast<AActor*>(CenterTarget));

	TargetLock->ClearTarget();
	AActor* Blocker = SpawnTargetLockVisibilityBlocker(World, FVector(600.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("The visibility blocker fixture is valid"), Blocker);
	TestTrue(TEXT("The acquisition can fall back to an unoccluded target"), TargetLock->AcquireBestTarget(ViewLocation, ViewForward));
	TestEqual(
		TEXT("An occluded center target is excluded from acquisition"),
		TargetLock->GetCurrentTarget(),
		static_cast<AActor*>(OffCenterTarget));

	return true;
}

// 目标锁定生命周期测试覆盖相机视点遮挡宽限、超距清除和无目标停 Tick。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCTargetLockLossConditionsTest,
	"ZCase.TargetLock.LossConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCTargetLockLossConditionsTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("The target-lock loss world can be created"), TestWorld.CreateTestWorld(EWorldType::Game));
	TestTrue(TEXT("The target-lock loss world can begin play"), TestWorld.BeginPlayInTestWorld());
	UWorld* World = TestWorld.GetTestWorld();
	if (!World)
	{
		return false;
	}

	AZCCharBase* Player = World->SpawnActor<AZCCharBase>(FVector::ZeroVector, FRotator::ZeroRotator);
	AZCEnemyBase* Target = World->SpawnActor<AZCEnemyBase>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	if (!Player || !Target || !Player->TargetLock)
	{
		AddError(TEXT("The target-lock loss fixtures are invalid"));
		return false;
	}

	UZCTargetLockComponent* TargetLock = Player->TargetLock;
	TestTrue(TEXT("The lifecycle fixture can be locked"), TargetLock->SetTarget(Target));
	TestTrue(TEXT("A locked component ticks"), TargetLock->IsComponentTickEnabled());

	// 失锁距离优先于遮挡判定。
	Target->SetActorLocation(FVector(3500.0f, 0.0f, 0.0f));
	TargetLock->TickComponent(0.01f, LEVELTICK_All, nullptr);
	TestNull(TEXT("A target beyond 3000 cm is cleared"), TargetLock->GetCurrentTarget());
	TestFalse(TEXT("Distance loss disables component ticking"), TargetLock->IsComponentTickEnabled());

	Target->SetActorLocation(FVector(1000.0f, 0.0f, 0.0f));
	TestTrue(TEXT("The target can be locked again after returning to range"), TargetLock->SetTarget(Target));
	AActor* Blocker = SpawnTargetLockVisibilityBlocker(World, FVector(500.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("The occlusion blocker fixture is valid"), Blocker);
	FHitResult VisibilityHit;
	TestTrue(
		TEXT("The first blocker intersects the target-lock visibility trace"),
		World->LineTraceSingleByChannel(
			VisibilityHit,
			Player->GetActorLocation(),
			Target->GetTargetLockLocation(),
			ECC_Visibility));
	TestEqual(TEXT("The first visibility hit is the blocker"), VisibilityHit.GetActor(), Blocker);
	TargetLock->TickComponent(0.50f, LEVELTICK_All, nullptr);
	TestEqual(
		TEXT("Short occlusion remains within the 0.75 second grace period"),
		TargetLock->GetCurrentTarget(),
		static_cast<AActor*>(Target));

	// 恢复可见必须清零遮挡累计；随后再次遮挡 0.5 秒仍应保持锁定。
	Blocker->Destroy();
	TargetLock->TickComponent(0.01f, LEVELTICK_All, nullptr);
	VisibilityHit = FHitResult();
	const bool bHitAfterRecovery = World->LineTraceSingleByChannel(
		VisibilityHit,
		Player->GetActorLocation(),
		Target->GetTargetLockLocation(),
		ECC_Visibility);
	TestTrue(
		TEXT("After recovery the trace is clear or reaches the target"),
		!bHitAfterRecovery || VisibilityHit.GetActor() == Target);
	Blocker = SpawnTargetLockVisibilityBlocker(World, FVector(500.0f, 0.0f, 0.0f));
	TestNotNull(TEXT("The second occlusion blocker fixture is valid"), Blocker);
	VisibilityHit = FHitResult();
	TestTrue(
		TEXT("The second blocker intersects the target-lock visibility trace"),
		World->LineTraceSingleByChannel(
			VisibilityHit,
			Player->GetActorLocation(),
			Target->GetTargetLockLocation(),
			ECC_Visibility));
	TestEqual(TEXT("The second visibility hit is the blocker"), VisibilityHit.GetActor(), Blocker);
	TargetLock->TickComponent(0.50f, LEVELTICK_All, nullptr);
	TestEqual(
		TEXT("Visibility recovery resets the occlusion grace timer"),
		TargetLock->GetCurrentTarget(),
		static_cast<AActor*>(Target));
	TargetLock->TickComponent(0.26f, LEVELTICK_All, nullptr);
	TestNull(TEXT("Continuous occlusion beyond the grace period clears the target"), TargetLock->GetCurrentTarget());
	TestFalse(TEXT("Occlusion loss disables component ticking"), TargetLock->IsComponentTickEnabled());

	return true;
}

// 锁定移动基础测试覆盖角色朝向模式，以及 AnimBP 需要的锁定布尔和局部方向角。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCTargetLockAnimationDataTest,
	"ZCase.TargetLock.AnimationData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCTargetLockAnimationDataTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("The target-lock animation world can be created"), TestWorld.CreateTestWorld(EWorldType::Game));
	TestTrue(TEXT("The target-lock animation world can begin play"), TestWorld.BeginPlayInTestWorld());
	UWorld* World = TestWorld.GetTestWorld();
	if (!World)
	{
		return false;
	}

	AZCCharBase* Player = World->SpawnActor<AZCCharBase>();
	AZCEnemyBase* Target = World->SpawnActor<AZCEnemyBase>();
	if (!Player || !Target || !Player->TargetLock || !Player->GetCharacterMovement())
	{
		AddError(TEXT("The target-lock animation fixtures are invalid"));
		return false;
	}

	Player->SetActorLocation(FVector::ZeroVector);
	Player->SetActorRotation(FRotator::ZeroRotator);
	Target->SetActorLocation(FVector(0.0f, 1000.0f, 0.0f));
	TestTrue(
		TEXT("Unlocked movement initially rotates toward movement"),
		Player->GetCharacterMovement()->bOrientRotationToMovement);
	TestTrue(TEXT("A valid target can be locked"), Player->TargetLock->SetTarget(Target));
	TestFalse(
		TEXT("Locking disables orient-to-movement"),
		Player->GetCharacterMovement()->bOrientRotationToMovement);

	// 默认 720 度/秒在一秒内应完成 90 度转向，并且只修改水平 Yaw。
	Player->Tick(1.0f);
	TestEqual(TEXT("Locked facing keeps pitch at zero"), Player->GetActorRotation().Pitch, 0.0, 0.01);
	TestEqual(TEXT("Locked facing keeps roll at zero"), Player->GetActorRotation().Roll, 0.0, 0.01);
	TestEqual(TEXT("Locked facing turns toward the target"), Player->GetActorRotation().Yaw, 90.0, 0.1);

	TestEqual(
		TEXT("Forward direction is zero"),
		UZCAnimInst::CalculateLockOnDirection(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)),
		0.0f,
		0.01f);
	TestEqual(
		TEXT("Right direction is positive ninety"),
		UZCAnimInst::CalculateLockOnDirection(FRotator::ZeroRotator, FVector(0.0f, 100.0f, 0.0f)),
		90.0f,
		0.01f);
	TestEqual(
		TEXT("Left direction is negative ninety"),
		UZCAnimInst::CalculateLockOnDirection(FRotator::ZeroRotator, FVector(0.0f, -100.0f, 0.0f)),
		-90.0f,
		0.01f);
	TestEqual(
		TEXT("Stationary direction preserves its stable value"),
		UZCAnimInst::CalculateLockOnDirection(FRotator(0.0f, 135.0f, 0.0f), FVector::ZeroVector, 42.0f),
		42.0f,
		0.01f);

	// UAnimInstance 声明了 Within=SkeletalMeshComponent，测试实例必须使用角色网格作为 Outer。
	UZCAnimInst* AnimInstance = NewObject<UZCAnimInst>(Player->GetMesh());
	AnimInstance->PlayerRef = Player;
	AnimInstance->MoveComp = Player->GetCharacterMovement();
	Player->SetActorRotation(FRotator::ZeroRotator);
	Player->GetCharacterMovement()->Velocity = FVector(0.0f, 200.0f, 0.0f);
	AnimInstance->NativeUpdateAnimation(1.0f / 60.0f);
	TestTrue(TEXT("AnimInstance exposes the active target lock"), AnimInstance->bIsTargetLocked);
	TestEqual(TEXT("AnimInstance exposes rightward local movement"), AnimInstance->LockOnDirection, 90.0f, 0.01f);

	Player->TargetLock->ClearTarget();
	TestTrue(
		TEXT("Unlocking restores orient-to-movement"),
		Player->GetCharacterMovement()->bOrientRotationToMovement);
	AnimInstance->NativeUpdateAnimation(1.0f / 60.0f);
	TestFalse(TEXT("AnimInstance clears the target-lock flag"), AnimInstance->bIsTargetLocked);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
