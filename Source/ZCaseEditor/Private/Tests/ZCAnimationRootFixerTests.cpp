// 版权所有 Epic Games, Inc，保留所有权利

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "ZCAnimationRootFixer.h"

// 验证首帧目标、平移与相对变换保留，以及重复修正的幂等性
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCAnimationRootFixBuildKeysTest,
	"ZCase.Editor.AnimationRootFix.BuildCorrectedKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCAnimationRootFixBuildKeysTest::RunTest(const FString& Parameters)
{
	const FTransform FirstKey(
		FRotator(90.0, 0.0, -90.0),
		FVector(10.0, 20.0, 30.0),
		FVector(1.0));
	const FQuat RelativeRotation = FRotator(0.0, 25.0, 0.0).Quaternion();
	const FTransform SecondKey(
		RelativeRotation * FirstKey.GetRotation(),
		FVector(40.0, 50.0, 60.0),
		FVector(2.0));

	const TArray<FTransform> InputKeys = { FirstKey, SecondKey };
	const FZCAnimationRootFixSettings Settings;
	TArray<FTransform> CorrectedKeys;
	FString Error;
	TestTrue(TEXT("Correction succeeds"), FZCAnimationRootFixer::BuildCorrectedRootKeys(InputKeys, Settings, CorrectedKeys, Error));
	TestEqual(TEXT("Key count is preserved"), CorrectedKeys.Num(), InputKeys.Num());
	TestTrue(TEXT("First key scale reaches the target"), CorrectedKeys[0].GetScale3D().Equals(FVector(100.0), Settings.Tolerance));
	TestTrue(TEXT("First key rotation reaches the target"), CorrectedKeys[0].GetRotation().Equals(Settings.TargetFirstKeyRotation.Quaternion(), Settings.Tolerance));
	TestTrue(TEXT("First translation is preserved"), CorrectedKeys[0].GetTranslation().Equals(InputKeys[0].GetTranslation(), Settings.Tolerance));
	TestTrue(TEXT("Second translation is preserved"), CorrectedKeys[1].GetTranslation().Equals(InputKeys[1].GetTranslation(), Settings.Tolerance));
	TestTrue(TEXT("Relative scale is preserved"), CorrectedKeys[1].GetScale3D().Equals(FVector(200.0), Settings.Tolerance));

	const FQuat InputRelativeRotation = InputKeys[0].GetRotation().Inverse() * InputKeys[1].GetRotation();
	const FQuat OutputRelativeRotation = CorrectedKeys[0].GetRotation().Inverse() * CorrectedKeys[1].GetRotation();
	TestTrue(TEXT("Relative rotation is preserved"), InputRelativeRotation.Equals(OutputRelativeRotation, Settings.Tolerance));

	TArray<FTransform> ReappliedKeys;
	TestTrue(TEXT("Reapplying succeeds"), FZCAnimationRootFixer::BuildCorrectedRootKeys(CorrectedKeys, Settings, ReappliedKeys, Error));
	for (int32 Index = 0; Index < CorrectedKeys.Num(); ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("Reapplying is idempotent for key %d"), Index),
			CorrectedKeys[Index].Equals(ReappliedKeys[Index], Settings.Tolerance));
	}

	return true;
}

// 验证首帧含零缩放分量时拒绝修正并提供错误说明
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCAnimationRootFixRejectsZeroScaleTest,
	"ZCase.Editor.AnimationRootFix.RejectsZeroScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCAnimationRootFixRejectsZeroScaleTest::RunTest(const FString& Parameters)
{
	const TArray<FTransform> InputKeys = {
		FTransform(FQuat::Identity, FVector::ZeroVector, FVector(1.0, 0.0, 1.0))
	};
	TArray<FTransform> CorrectedKeys;
	FString Error;
	TestFalse(
		TEXT("A zero scale component is rejected"),
		FZCAnimationRootFixer::BuildCorrectedRootKeys(InputKeys, FZCAnimationRootFixSettings(), CorrectedKeys, Error));
	TestTrue(TEXT("The failure explains the invalid scale"), Error.Contains(TEXT("zero scale")));
	return true;
}

// 在真实动画的瞬态副本上验证根轨道修正，不改动源资产
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FZCAnimationRootFixRealAssetTest,
	"ZCase.Editor.AnimationRootFix.RealLinkAssetTransientCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FZCAnimationRootFixRealAssetTest::RunTest(const FString& Parameters)
{
	const TCHAR* AssetPath = TEXT("/Game/_Game/Animations/LinkAnim/00_Combat/07_Combat_Pose/Bow/Link_Ani_Anim_Armature_A_Pose_Bow_Build_Move.Link_Ani_Anim_Armature_A_Pose_Bow_Build_Move");
	UAnimSequence* SourceAnimation = LoadObject<UAnimSequence>(nullptr, AssetPath);
	if (!TestNotNull(TEXT("The representative Link animation loads"), SourceAnimation))
	{
		return false;
	}

	UAnimSequence* WorkingCopy = DuplicateObject<UAnimSequence>(SourceAnimation, GetTransientPackage());
	if (!TestNotNull(TEXT("A transient working copy is created"), WorkingCopy))
	{
		return false;
	}

	const FZCAnimationRootFixSettings Settings;
	FString Details;
	const EZCAnimationRootFixResult Result = FZCAnimationRootFixer::ApplyToAnimation(WorkingCopy, Settings, Details);
	TestTrue(
		TEXT("The real asset layout can be corrected"),
		Result == EZCAnimationRootFixResult::Applied || Result == EZCAnimationRootFixResult::AlreadyCorrect);

	const USkeleton* Skeleton = WorkingCopy->GetSkeleton();
	if (!TestNotNull(TEXT("The working copy has a skeleton"), Skeleton))
	{
		return false;
	}

	const FName RootBoneName = Skeleton->GetReferenceSkeleton().GetBoneName(0);
	TArray<FTransform> RootKeys;
	WorkingCopy->GetDataModel()->GetBoneTrackTransforms(RootBoneName, RootKeys);
	if (!TestTrue(TEXT("The real animation has root keys"), !RootKeys.IsEmpty()))
	{
		return false;
	}

	TestTrue(TEXT("The real animation reaches the target scale"), RootKeys[0].GetScale3D().Equals(Settings.TargetFirstKeyScale, Settings.Tolerance));
	TestTrue(TEXT("The real animation reaches the target rotation"), RootKeys[0].GetRotation().Equals(Settings.TargetFirstKeyRotation.Quaternion(), Settings.Tolerance));
	return true;
}

#endif
