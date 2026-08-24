// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/ZCCombatComponent.h"
#include "ZCAnimNotify_WeaponAttachment.generated.h"

/** Moves the sword and shield at the authored contact frame of a draw/sheath montage. */
UCLASS(meta = (DisplayName = "ZC Weapon Attachment"))
class ZCASE_API UZCAnimNotify_WeaponAttachment : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|Combat|Weapon")
	EZCWeaponAttachmentState AttachmentState = EZCWeaponAttachmentState::Equipped;
};
