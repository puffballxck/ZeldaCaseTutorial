// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ZCGameplayTypes.generated.h"

UENUM(BlueprintType)
enum class EMovementTypes : uint8
{
	MT_EMAX UMETA(DisplayName = "EMAX"),           // Default / unset.
	MT_Walking UMETA(DisplayName = "Walking"),     // Ground movement.
	MT_Exhausted UMETA(DisplayName = "Exhausted"), // Stamina depleted.
	MT_Sprinting UMETA(DisplayName = "Sprinting"), // Sprinting.
	MT_Gliding UMETA(DisplayName = "Gliding"),     // Gliding.
	MT_Falling UMETA(DisplayName = "Falling")      // Falling.
};

UENUM(BlueprintType)
enum ERunes : uint8
{
	R_EMAX UMETA(DisplayName = "EMAX"),       // Default / no rune.
	R_RBS UMETA(DisplayName = "RBS"),         // Remote bomb sphere.
	R_RBB UMETA(DisplayName = "RBB"),         // Remote bomb cube.
	R_Mag UMETA(DisplayName = "Magnesis"),    // Magnesis.
	R_Stasis UMETA(DisplayName = "Stasis"),   // Stasis.
	R_Ice UMETA(DisplayName = "Ice"),         // Cryonis.
};
