// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZCAnimationRootFixer.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

#define LOCTEXT_NAMESPACE "ZCAnimationRootFixer"

namespace
{
bool IsNearlyZeroScaleComponent(const double Value, const float Tolerance)
{
	return FMath::Abs(Value) <= Tolerance;
}
}

bool FZCAnimationRootFixer::BuildCorrectedRootKeys(
	const TArray<FTransform>& InputKeys,
	const FZCAnimationRootFixSettings& Settings,
	TArray<FTransform>& OutKeys,
	FString& OutError)
{
	OutKeys.Reset();
	OutError.Reset();

	if (InputKeys.IsEmpty())
	{
		OutError = TEXT("The root track has no keys.");
		return false;
	}

	const FTransform& FirstKey = InputKeys[0];
	const FVector FirstScale = FirstKey.GetScale3D();
	if (IsNearlyZeroScaleComponent(FirstScale.X, Settings.Tolerance)
		|| IsNearlyZeroScaleComponent(FirstScale.Y, Settings.Tolerance)
		|| IsNearlyZeroScaleComponent(FirstScale.Z, Settings.Tolerance))
	{
		OutError = TEXT("The first root key contains a zero scale component.");
		return false;
	}

	const FVector ScaleRatio(
		Settings.TargetFirstKeyScale.X / FirstScale.X,
		Settings.TargetFirstKeyScale.Y / FirstScale.Y,
		Settings.TargetFirstKeyScale.Z / FirstScale.Z);

	const FQuat FirstRotation = FirstKey.GetRotation().GetNormalized();
	const FQuat TargetRotation = Settings.TargetFirstKeyRotation.Quaternion().GetNormalized();
	const FQuat RotationCorrection = (TargetRotation * FirstRotation.Inverse()).GetNormalized();

	OutKeys.Reserve(InputKeys.Num());
	for (const FTransform& InputKey : InputKeys)
	{
		FTransform CorrectedKey = InputKey;
		CorrectedKey.SetScale3D(InputKey.GetScale3D() * ScaleRatio);

		FQuat CorrectedRotation = RotationCorrection * InputKey.GetRotation().GetNormalized();
		CorrectedRotation.Normalize();
		CorrectedKey.SetRotation(CorrectedRotation);
		OutKeys.Add(CorrectedKey);
	}

	return true;
}

EZCAnimationRootFixResult FZCAnimationRootFixer::ApplyToAnimation(
	UAnimSequence* Animation,
	const FZCAnimationRootFixSettings& Settings,
	FString& OutDetails)
{
	OutDetails.Reset();
	if (!Animation)
	{
		OutDetails = TEXT("Invalid Animation Sequence.");
		return EZCAnimationRootFixResult::InvalidAnimation;
	}

	const USkeleton* Skeleton = Animation->GetSkeleton();
	if (!Skeleton || Skeleton->GetReferenceSkeleton().GetNum() == 0)
	{
		OutDetails = TEXT("Animation has no valid skeleton.");
		return EZCAnimationRootFixResult::MissingSkeleton;
	}

	const FName RootBoneName = Skeleton->GetReferenceSkeleton().GetBoneName(0);
	const IAnimationDataModel* DataModel = Animation->GetDataModel();
	if (!DataModel || !DataModel->IsValidBoneTrackName(RootBoneName))
	{
		OutDetails = FString::Printf(TEXT("Root track '%s' was not found."), *RootBoneName.ToString());
		return EZCAnimationRootFixResult::MissingRootTrack;
	}

	TArray<FTransform> ExistingKeys;
	DataModel->GetBoneTrackTransforms(RootBoneName, ExistingKeys);
	if (ExistingKeys.IsEmpty())
	{
		OutDetails = FString::Printf(TEXT("Root track '%s' contains no keys."), *RootBoneName.ToString());
		return EZCAnimationRootFixResult::MissingRootKeys;
	}

	TArray<FTransform> CorrectedKeys;
	FString BuildError;
	if (!BuildCorrectedRootKeys(ExistingKeys, Settings, CorrectedKeys, BuildError))
	{
		OutDetails = MoveTemp(BuildError);
		return EZCAnimationRootFixResult::InvalidRootScale;
	}

	const FTransform& ExistingFirstKey = ExistingKeys[0];
	const FTransform& CorrectedFirstKey = CorrectedKeys[0];
	const bool bScaleAlreadyCorrect = ExistingFirstKey.GetScale3D().Equals(
		CorrectedFirstKey.GetScale3D(), Settings.Tolerance);
	const bool bRotationAlreadyCorrect = ExistingFirstKey.GetRotation().Equals(
		CorrectedFirstKey.GetRotation(), Settings.Tolerance);
	if (bScaleAlreadyCorrect && bRotationAlreadyCorrect)
	{
		OutDetails = FString::Printf(TEXT("Root track '%s' already matches the target first key."), *RootBoneName.ToString());
		return EZCAnimationRootFixResult::AlreadyCorrect;
	}

	TArray<FVector3f> PositionKeys;
	TArray<FQuat4f> RotationKeys;
	TArray<FVector3f> ScaleKeys;
	PositionKeys.Reserve(CorrectedKeys.Num());
	RotationKeys.Reserve(CorrectedKeys.Num());
	ScaleKeys.Reserve(CorrectedKeys.Num());

	for (const FTransform& CorrectedKey : CorrectedKeys)
	{
		PositionKeys.Add(FVector3f(CorrectedKey.GetTranslation()));
		RotationKeys.Add(FQuat4f(CorrectedKey.GetRotation()));
		ScaleKeys.Add(FVector3f(CorrectedKey.GetScale3D()));
	}

	IAnimationDataController& Controller = Animation->GetController();
	if (!Controller.SetBoneTrackKeys(RootBoneName, PositionKeys, RotationKeys, ScaleKeys, true))
	{
		OutDetails = FString::Printf(TEXT("Failed to write root track '%s'."), *RootBoneName.ToString());
		return EZCAnimationRootFixResult::WriteFailed;
	}

	Animation->MarkPackageDirty();
	OutDetails = FString::Printf(
		TEXT("Updated %d keys on root track '%s'. Changes are unsaved and can be undone."),
		CorrectedKeys.Num(),
		*RootBoneName.ToString());
	return EZCAnimationRootFixResult::Applied;
}

const TCHAR* FZCAnimationRootFixer::LexToString(const EZCAnimationRootFixResult Result)
{
	switch (Result)
	{
	case EZCAnimationRootFixResult::Applied: return TEXT("Applied");
	case EZCAnimationRootFixResult::AlreadyCorrect: return TEXT("AlreadyCorrect");
	case EZCAnimationRootFixResult::InvalidAnimation: return TEXT("InvalidAnimation");
	case EZCAnimationRootFixResult::MissingSkeleton: return TEXT("MissingSkeleton");
	case EZCAnimationRootFixResult::MissingRootTrack: return TEXT("MissingRootTrack");
	case EZCAnimationRootFixResult::MissingRootKeys: return TEXT("MissingRootKeys");
	case EZCAnimationRootFixResult::InvalidRootScale: return TEXT("InvalidRootScale");
	case EZCAnimationRootFixResult::WriteFailed: return TEXT("WriteFailed");
	default: return TEXT("Unknown");
	}
}

#undef LOCTEXT_NAMESPACE
