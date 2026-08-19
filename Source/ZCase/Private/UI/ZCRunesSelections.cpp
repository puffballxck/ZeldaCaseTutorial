// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/ZCRunesSelections.h"

#include "Characters/ZCCharBase.h"

void UZCRunesSelections::SelectRuneType(TEnumAsByte<ERunes> RuneType)
{
	if (PlayerRef)
	{
		PlayerRef->SelectRune(static_cast<ERunes>(RuneType.GetValue()));
	}
}
