// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Characters/ZCCharBase.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

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

// 命中窗口测试确认攻击生命周期、窗口开关和同一攻击内的目标去重边界。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCCombatHitWindowTest,
	"ZCase.Combat.HitWindowAndDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCCombatHitWindowTest::RunTest(const FString& Parameters)
{
	UZCCombatComponent* Combat = NewObject<UZCCombatComponent>();
	AActor* Target = NewObject<AActor>();

	TestFalse(TEXT("An invalid target is rejected"), Combat->TryApplyHit(nullptr, 10.0f));
	TestFalse(TEXT("A hit before an attack is rejected"), Combat->TryApplyHit(Target, 10.0f));
	Combat->StartAttack();
	TestFalse(TEXT("A hit before the trace window is rejected"), Combat->TryApplyHit(Target, 10.0f));
	TestTrue(TEXT("An attack can open its trace window"), Combat->BeginTrace());
	TestTrue(TEXT("The first hit is applied"), Combat->TryApplyHit(Target, 10.0f));
	TestFalse(TEXT("The same attack cannot hit one target twice"), Combat->TryApplyHit(Target, 10.0f));
	Combat->EndTrace();
	TestFalse(TEXT("A closed trace window rejects hits"), Combat->TryApplyHit(Target, 10.0f));

	Combat->StartAttack();
	// 新攻击必须重置去重集合，使同一目标可以再次受击。
	Combat->BeginTrace();
	TestTrue(TEXT("A new attack clears the hit set"), Combat->TryApplyHit(Target, 10.0f));
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

	TestNotNull(TEXT("Draw montage exists"), DrawMontage);
	TestNotNull(TEXT("Sheath montage exists"), SheathMontage);
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

#endif // WITH_DEV_AUTOMATION_TESTS
