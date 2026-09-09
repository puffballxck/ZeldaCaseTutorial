#include "Actors/ZCInventoryPickupActor.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/ZCCharBase.h"
#include "Combat/ZCCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Inventory/ZCInventorySubsystem.h"
#include "UObject/ConstructorHelpers.h"

AZCInventoryPickupActor::AZCInventoryPickupActor()
{
	BaseMesh->SetCollisionProfileName(TEXT("NoCollision"));
	InteractSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractSphere->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageFinder(
		TEXT("/Game/_Game/Animations/LinkAnim/Montage/AM_Take_LR.AM_Take_LR"));
	PickupMontage = MontageFinder.Object;
}

void AZCInventoryPickupActor::ToggleInteraction(AZCCharBase* Character)
{
	if (bPickupInProgress || !IsValid(Character) || !InteractSphere->IsOverlappingActor(Character)
		|| (Character->Combat && !Character->Combat->CanAcceptCombatInput()))
	{
		return;
	}

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	// 不覆盖攻击、受击等正在播放的动作，也防止同时拾取多个物品。
	if (!AnimInstance || !PickupMontage || AnimInstance->IsAnyMontagePlaying())
	{
		return;
	}

	PlayerRef = Character;
	bPickupInProgress = true;
	if (AnimInstance->Montage_Play(PickupMontage) <= 0.0f)
	{
		bPickupInProgress = false;
		PlayerRef = nullptr;
		return;
	}
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AZCInventoryPickupActor::HandlePickupEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, PickupMontage);
}

void AZCInventoryPickupActor::HandlePickupEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bPickupInProgress || Montage != PickupMontage)
	{
		return;
	}
	AZCCharBase* Character = PlayerRef;
	PlayerRef = nullptr;
	// 入包会同步广播 UI 事件，在整个提交期间保持防重入标记。
	if (bInterrupted || !IsValid(Character) || !InteractSphere->IsOverlappingActor(Character)
		|| (Character->Combat && !Character->Combat->CanAcceptCombatInput()))
	{
		bPickupInProgress = false;
		return;
	}

	UGameInstance* GameInstance = Character->GetGameInstance();
	UZCInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UZCInventorySubsystem>() : nullptr;
	const EZCInventoryResult Result = Inventory
		? Inventory->TryAddItem(ItemId, Amount) : EZCInventoryResult::NotInitialized;
	if (Result == EZCInventoryResult::Success)
	{
		PlayerLeft();
		PlayerLeftCpp();
		Destroy();
	}
	else
	{
		bPickupInProgress = false;
		PickupFailed(Result);
	}
}
