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

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZCCombatComponent* Combat = Owner ? Owner->FindComponentByClass<UZCCombatComponent>() : nullptr)
	{
		Combat->SetEquipmentAttachmentState(AttachmentState);
	}
}

FString UZCAnimNotify_WeaponAttachment::GetNotifyName_Implementation() const
{
	return AttachmentState == EZCWeaponAttachmentState::Equipped
		? TEXT("ZC Equip Weapon")
		: TEXT("ZC Sheath Weapon");
}
