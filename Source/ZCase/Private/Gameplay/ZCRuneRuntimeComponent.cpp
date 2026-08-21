// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/ZCRuneRuntimeComponent.h"

namespace
{
bool IsKnownRune(const ERunes Rune)
{
	switch (Rune)
	{
	case ERunes::R_EMAX:
	case ERunes::R_RBS:
	case ERunes::R_RBB:
	case ERunes::R_Mag:
	case ERunes::R_Stasis:
	case ERunes::R_Ice:
		return true;
	default:
		return false;
	}
}
}

UZCRuneRuntimeComponent::UZCRuneRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UZCRuneRuntimeComponent::SelectRune(const ERunes NewRune)
{
	if (!IsKnownRune(NewRune) || SelectedRune == NewRune)
	{
		return false;
	}

	// Cancel first so the ActiveRune invariant also holds while listeners run.
	if (ActiveRune != ERunes::R_EMAX)
	{
		SetActiveRune(ERunes::R_EMAX);
	}

	const ERunes PreviousRune = SelectedRune;
	SelectedRune = NewRune;
	OnSelectedRuneChanged.Broadcast(PreviousRune, SelectedRune);
	return true;
}

bool UZCRuneRuntimeComponent::ToggleSelectedRune()
{
	if (SelectedRune == ERunes::R_EMAX)
	{
		return false;
	}

	SetActiveRune(ActiveRune == SelectedRune ? ERunes::R_EMAX : SelectedRune.GetValue());
	return true;
}

bool UZCRuneRuntimeComponent::CancelAll()
{
	if (ActiveRune == ERunes::R_EMAX)
	{
		return false;
	}

	SetActiveRune(ERunes::R_EMAX);
	return true;
}

void UZCRuneRuntimeComponent::SetActiveRune(const ERunes NewActiveRune)
{
	if (ActiveRune == NewActiveRune)
	{
		return;
	}

	const ERunes PreviousRune = ActiveRune;
	ActiveRune = NewActiveRune;
	OnActiveRuneChanged.Broadcast(PreviousRune, ActiveRune);
}
