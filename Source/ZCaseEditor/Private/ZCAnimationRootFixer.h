// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "CoreMinimal.h"

class UAnimSequence;

// 根骨骼首帧的目标变换及浮点比较容差
struct FZCAnimationRootFixSettings
{
	// 首帧目标缩放，用于计算所有关键帧共用的缩放比例
	FVector TargetFirstKeyScale = FVector(100.0);
	// 首帧目标朝向，用于计算统一的旋转修正量
	FRotator TargetFirstKeyRotation = FRotator(0.0, 90.0, 0.0);
	// 近零缩放检查与已修正状态比较使用的容差
	float Tolerance = 0.001f;
};

// 区分修正成功、无需修改与各类输入或写入失败
enum class EZCAnimationRootFixResult : uint8
{
	Applied, // 已写入修正后的根关键帧
	AlreadyCorrect, // 首帧已满足目标，无需重复写入
	InvalidAnimation, // 动画对象无效
	MissingSkeleton, // 骨架无效或没有骨骼
	MissingRootTrack, // 动画数据中不存在根骨骼轨道
	MissingRootKeys, // 根轨道没有关键帧
	InvalidRootScale, // 首帧包含无法计算比例的近零缩放
	WriteFailed // 动画数据控制器未能写入轨道
};

// 生成根轨道修正数据，并通过动画数据控制器应用到资产
struct FZCAnimationRootFixer
{
	// 生成修正关键帧并保留平移和相对旋转缩放，失败时输出原因
	static bool BuildCorrectedRootKeys(
		const TArray<FTransform>& InputKeys,
		const FZCAnimationRootFixSettings& Settings,
		TArray<FTransform>& OutKeys,
		FString& OutError);

	// 校验动画根轨道并执行可撤销写入，仅标脏而不保存资产
	static EZCAnimationRootFixResult ApplyToAnimation(
		UAnimSequence* Animation,
		const FZCAnimationRootFixSettings& Settings,
		FString& OutDetails);

	// 将修正结果转换为稳定的日志标识
	static const TCHAR* LexToString(EZCAnimationRootFixResult Result);
};
