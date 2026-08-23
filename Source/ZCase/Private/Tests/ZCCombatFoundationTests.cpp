// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Characters/ZCCharBase.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

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
	Combat->BeginTrace();
	TestTrue(TEXT("A new attack clears the hit set"), Combat->TryApplyHit(Target, 10.0f));
	return true;
}

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
