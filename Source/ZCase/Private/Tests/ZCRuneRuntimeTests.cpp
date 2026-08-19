// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/ZCRuneRuntimeComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCRuneRuntimeSelectionTest,
	"ZCase.Runes.Runtime.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCRuneRuntimeSelectionTest::RunTest(const FString& Parameters)
{
	UZCRuneRuntimeComponent* Runtime = NewObject<UZCRuneRuntimeComponent>();

	TestEqual(TEXT("Selection starts empty"), Runtime->GetSelectedRune(), ERunes::R_EMAX);
	TestEqual(TEXT("Activation starts empty"), Runtime->GetActiveRune(), ERunes::R_EMAX);
	TestTrue(TEXT("A rune can be selected"), Runtime->SelectRune(ERunes::R_Mag));
	TestEqual(TEXT("The selected rune is exposed"), Runtime->GetSelectedRune(), ERunes::R_Mag);
	TestEqual(TEXT("Selecting does not activate"), Runtime->GetActiveRune(), ERunes::R_EMAX);
	TestFalse(TEXT("Selecting the same rune is a no-op"), Runtime->SelectRune(ERunes::R_Mag));
	TestFalse(TEXT("An unknown enum value is rejected"), Runtime->SelectRune(static_cast<ERunes>(255)));
	TestEqual(TEXT("Rejecting an unknown value preserves selection"), Runtime->GetSelectedRune(), ERunes::R_Mag);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCRuneRuntimeToggleTest,
	"ZCase.Runes.Runtime.Toggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCRuneRuntimeToggleTest::RunTest(const FString& Parameters)
{
	UZCRuneRuntimeComponent* Runtime = NewObject<UZCRuneRuntimeComponent>();

	TestFalse(TEXT("An empty selection cannot be activated"), Runtime->ToggleSelectedRune());
	Runtime->SelectRune(ERunes::R_RBS);
	TestTrue(TEXT("The selected rune can be activated"), Runtime->ToggleSelectedRune());
	TestEqual(TEXT("Activation follows selection"), Runtime->GetActiveRune(), ERunes::R_RBS);
	TestTrue(TEXT("Toggling an active rune cancels it"), Runtime->ToggleSelectedRune());
	TestEqual(TEXT("Toggling leaves the rune selected"), Runtime->GetSelectedRune(), ERunes::R_RBS);
	TestEqual(TEXT("The active rune is cleared"), Runtime->GetActiveRune(), ERunes::R_EMAX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCRuneRuntimeSwitchingTest,
	"ZCase.Runes.Runtime.SwitchingIsExclusive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCRuneRuntimeSwitchingTest::RunTest(const FString& Parameters)
{
	UZCRuneRuntimeComponent* Runtime = NewObject<UZCRuneRuntimeComponent>();

	Runtime->SelectRune(ERunes::R_Mag);
	Runtime->ToggleSelectedRune();
	TestEqual(TEXT("The first rune is active"), Runtime->GetActiveRune(), ERunes::R_Mag);

	TestTrue(TEXT("A different rune can be selected"), Runtime->SelectRune(ERunes::R_Ice));
	TestEqual(TEXT("The new rune replaces the selection"), Runtime->GetSelectedRune(), ERunes::R_Ice);
	TestEqual(TEXT("Changing selection cancels the old activation"), Runtime->GetActiveRune(), ERunes::R_EMAX);

	Runtime->ToggleSelectedRune();
	TestEqual(TEXT("Only the newly selected rune can become active"), Runtime->GetActiveRune(), ERunes::R_Ice);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCRuneRuntimeCancelAllTest,
	"ZCase.Runes.Runtime.CancelAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCRuneRuntimeCancelAllTest::RunTest(const FString& Parameters)
{
	UZCRuneRuntimeComponent* Runtime = NewObject<UZCRuneRuntimeComponent>();

	Runtime->SelectRune(ERunes::R_Stasis);
	Runtime->ToggleSelectedRune();
	TestTrue(TEXT("An active rune can be cancelled"), Runtime->CancelAll());
	TestEqual(TEXT("Cancelling clears activation"), Runtime->GetActiveRune(), ERunes::R_EMAX);
	TestEqual(TEXT("Cancelling preserves selection"), Runtime->GetSelectedRune(), ERunes::R_Stasis);
	TestFalse(TEXT("Cancelling again is a no-op"), Runtime->CancelAll());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
