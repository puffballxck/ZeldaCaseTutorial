// 版权所有 Epic Games, Inc，保留所有权利

#include "Animations/ZCAnimNotifyState_WeaponTrace.h"

#include "Combat/ZCCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UZCAnimNotifyState_WeaponTrace::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 动画网格是稳定入口；组件自行校验攻击生命周期、SwordMesh 和 Trace socket
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZCCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UZCCombatComponent>() : nullptr)
	{
		Combat->BeginTrace();
	}
}

void UZCAnimNotifyState_WeaponTrace::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// Notify 被正常结束或动画被打断时都走同一个关闭入口，保证 Tick 不会泄漏到窗口之外
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZCCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UZCCombatComponent>() : nullptr)
	{
		Combat->HandleAttackTraceWindowEnded(Animation);
	}
}

FString UZCAnimNotifyState_WeaponTrace::GetNotifyName_Implementation() const
{
	return TEXT("ZC Weapon Trace");
}
