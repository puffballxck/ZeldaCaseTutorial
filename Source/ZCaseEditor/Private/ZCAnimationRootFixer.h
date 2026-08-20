// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

struct FZCAnimationRootFixSettings
{
	FVector TargetFirstKeyScale = FVector(100.0);
	FRotator TargetFirstKeyRotation = FRotator(0.0, 90.0, 0.0);
	float Tolerance = 0.001f;
};

enum class EZCAnimationRootFixResult : uint8
{
	Applied,
	AlreadyCorrect,
	InvalidAnimation,
	MissingSkeleton,
	MissingRootTrack,
	MissingRootKeys,
	InvalidRootScale,
	WriteFailed
};

struct FZCAnimationRootFixer
{
	static bool BuildCorrectedRootKeys(
		const TArray<FTransform>& InputKeys,
		const FZCAnimationRootFixSettings& Settings,
		TArray<FTransform>& OutKeys,
		FString& OutError);

	static EZCAnimationRootFixResult ApplyToAnimation(
		UAnimSequence* Animation,
		const FZCAnimationRootFixSettings& Settings,
		FString& OutDetails);

	static const TCHAR* LexToString(EZCAnimationRootFixResult Result);
};
