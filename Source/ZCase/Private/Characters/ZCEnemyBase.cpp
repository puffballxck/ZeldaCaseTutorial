// 版权所有 Epic Games, Inc，保留所有权利

#include "Characters/ZCEnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogZCEnemy, Log, All);

AZCEnemyBase::AZCEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;
	Attributes = CreateDefaultSubobject<UZCAttributeComponent>(TEXT("Attributes"));
	Combat = CreateDefaultSubobject<UZCCombatComponent>(TEXT("Combat"));
}

void AZCEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	if (Attributes)
	{
		Attributes->OnDeath.AddDynamic(this, &AZCEnemyBase::HandleDeath);
	}

	if (Combat)
	{
		ConfigureCombat();
	}
}

void AZCEnemyBase::ConfigureCombat()
{
	// 保留 Combat 的默认 Trace 配置；具体敌人可在派生类中提供骨骼/Socket 和 Montage
	if (Combat)
	{
		Combat->InitializeAttackSource(GetMesh(), GetMesh(), NAME_None, NAME_None);
	}
}

void AZCEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Attributes)
	{
		Attributes->OnDeath.RemoveDynamic(this, &AZCEnemyBase::HandleDeath);
	}

	Super::EndPlay(EndPlayReason);
}

float AZCEnemyBase::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (bDeathStarted || !CanBeDamaged() || bDamageProcessing
		|| !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	// Attribute 的同步广播可能触发外部回调；同一次伤害调用不允许递归扣血
	TGuardValue<bool> DamageGuard(bDamageProcessing, true);
	const float EngineDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!Attributes || !FMath::IsFinite(EngineDamage) || EngineDamage <= 0.0f)
	{
		return 0.0f;
	}

	const FZCDamageResult Result = Attributes->ApplyDamage(EngineDamage);
	if (Result.AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	// 致命伤害只进入死亡分支，避免先播放 Hit React 再立即被 Death 打断
	if (Result.bBecameDead)
	{
		// OnDeath 已在 ApplyDamage 内同步广播；这里保留幂等兜底，覆盖
		// BeginPlay 前或外部直接调用属性组件的特殊路径
		HandleDeath(this);
	}
	else
	{
		PlayHitReact();
	}

	return Result.AppliedDamage;
}

bool AZCEnemyBase::CanBeTargetLocked() const
{
	return Attributes && !Attributes->IsDead() && !bDeathStarted;
}

FVector AZCEnemyBase::GetTargetLockLocation() const
{
	if (const USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		static const FName HeadSocketName(TEXT("socket_head"));
		static const FName HeadBoneName(TEXT("head"));
		static const FName UppercaseHeadBoneName(TEXT("Head"));
		const FName LockAnchorName = EnemyMesh->DoesSocketExist(HeadSocketName)
			? HeadSocketName
			: EnemyMesh->DoesSocketExist(HeadBoneName)
				? HeadBoneName
				: UppercaseHeadBoneName;
		if (EnemyMesh->DoesSocketExist(LockAnchorName))
		{
			return EnemyMesh->GetSocketLocation(LockAnchorName);
		}
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HeightOffset = Capsule ? Capsule->GetScaledCapsuleHalfHeight() * 0.5f : 0.0f;
	return GetActorLocation() + FVector::UpVector * HeightOffset;
}

FVector AZCEnemyBase::GetTargetLockCameraLocation() const
{
	if (const USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		if (!TargetLockCameraSocketName.IsNone()
			&& EnemyMesh->DoesSocketExist(TargetLockCameraSocketName))
		{
			return EnemyMesh->GetSocketLocation(TargetLockCameraSocketName);
		}
	}

	// 镜头锚点不会复用动画头部或 UI 锚点
	// 缺少胸口 Socket 时使用胶囊体回退位置保持锁定焦点稳定
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float HeightOffset = Capsule
		? Capsule->GetScaledCapsuleHalfHeight() * 0.5f
		: 0.0f;
	return GetActorLocation() + FVector::UpVector * HeightOffset;
}

void AZCEnemyBase::PlayHitReact()
{
	if (bDeathStarted)
	{
		return;
	}

	USkeletalMeshComponent* EnemyMesh = GetMesh();
	UAnimInstance* AnimInstance = EnemyMesh ? EnemyMesh->GetAnimInstance() : nullptr;
	if (bHitReactActive && HitReactMontage && AnimInstance && AnimInstance->Montage_IsPlaying(HitReactMontage))
	{
		// 连续受击重置同一 Montage，不让旧结束回调提前恢复战斗
		AnimInstance->Montage_SetPosition(HitReactMontage, 0.0f);
		return;
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->PauseLogic(TEXT("Enemy hit reaction"));
		}
	}
	if (Combat && !Combat->InterruptForHitReaction())
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("Enemy hit reaction was not accepted"));
			}
		}
		return;
	}

	if (!HitReactMontage || !AnimInstance || AnimInstance->Montage_Play(HitReactMontage) <= 0.0f)
	{
		if (!bHitReactDiagnosticIssued)
		{
			bHitReactDiagnosticIssued = true;
			UE_LOG(
				LogZCEnemy,
				Warning,
				TEXT("%s received non-lethal damage but cannot play Hit React: Montage or AnimInstance is missing."),
				*GetNameSafe(this));
		}
		if (Combat)
		{
			Combat->ResumeAfterHitReaction();
		}
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("Enemy hit reaction unavailable"));
			}
		}
		return;
	}

	bHitReactActive = true;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AZCEnemyBase::HandleHitReactMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, HitReactMontage);
}

void AZCEnemyBase::HandleHitReactMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != HitReactMontage)
	{
		return;
	}

	bHitReactActive = false;
	if (!bDeathStarted && Combat)
	{
		Combat->ResumeAfterHitReaction();
	}
	if (!bDeathStarted)
	{
		if (AAIController* AIController = Cast<AAIController>(GetController()))
		{
			if (UBrainComponent* Brain = AIController->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("Enemy hit reaction ended"));
			}
		}
	}
}

void AZCEnemyBase::HandleDeath(AActor* DeadActor)
{
	if (bDeathStarted || (DeadActor && DeadActor != this))
	{
		return;
	}

	bDeathStarted = true;
	bHitReactActive = false;
	SetCanBeDamaged(false);
	SetActorTickEnabled(false);

	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->ClearFocus(EAIFocusPriority::Gameplay);
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Enemy died"));
		}
		AIController->StopMovement();
	}
	if (Combat)
	{
		// 先终止 AI，再进入 Disabled；这样攻击任务先解绑自身 delegate，
		// Combat 停止旧 Montage 时不会把行为树重新推进一轮
		Combat->DisableCombat();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// 第一版只忽略 Pawn，保留对世界的碰撞，避免倒地后穿地或位置失稳
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	USkeletalMeshComponent* EnemyMesh = GetMesh();
	UAnimInstance* AnimInstance = EnemyMesh ? EnemyMesh->GetAnimInstance() : nullptr;
	if (AnimInstance && !Combat)
	{
		AnimInstance->Montage_Stop(0.05f);
	}

	if (!DeathMontage || !AnimInstance)
	{
		if (!bDeathDiagnosticIssued)
		{
			bDeathDiagnosticIssued = true;
			UE_LOG(
				LogZCEnemy,
				Warning,
				TEXT("%s died but cannot play Death Montage: Montage or AnimInstance is missing."),
				*GetNameSafe(this));
		}
		return;
	}

	AnimInstance->Montage_Play(DeathMontage);
}
