// 版权所有 Epic Games, Inc，保留所有权利

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Combat/ZCCombatComponent.h"
#include "ZCAnimNotify_WeaponAttachment.generated.h"

/** 在拔刀或收刀蒙太奇中由动画师指定的接触帧切换剑和盾牌挂点 */
UCLASS(meta = (DisplayName = "ZC Weapon Attachment"))
class ZCASE_API UZCAnimNotify_WeaponAttachment : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 在动画准确接触帧把挂点切换请求交给 Combat 组件 */
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 根据挂点状态返回编辑器中显示的 Notify 名称 */
	virtual FString GetNotifyName_Implementation() const override;

	/** Notify 执行时要应用的装备挂点状态 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZCase|Combat|Weapon")
	EZCWeaponAttachmentState AttachmentState = EZCWeaponAttachmentState::Equipped;
};
