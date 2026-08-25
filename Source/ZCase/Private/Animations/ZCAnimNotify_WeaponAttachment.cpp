// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animations/ZCAnimNotify_WeaponAttachment.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UZCAnimNotify_WeaponAttachment::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	// Notify 以动画网格所属 Actor 为入口，把精确接触帧交给战斗组件处理。
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZCCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UZCCombatComponent>() : nullptr)
	{
		// 组件会取消同方向的回退 Timer，让动画师指定的精确帧优先生效。
		Combat->SetEquipmentAttachmentState(AttachmentState);
	}
}

FString UZCAnimNotify_WeaponAttachment::GetNotifyName_Implementation() const
{
	return AttachmentState == EZCWeaponAttachmentState::Equipped
		? TEXT("ZC Equip Weapon")
		: TEXT("ZC Sheath Weapon");
}
