// 请在项目设置的说明页面填写版权声明

#include "Characters/ZCCharBase.h"
#include "Characters/ZCBokoblinEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Data/ZCPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Gameplay/ZCRuneRuntimeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Debug/DebugHelper.h"
#include "UI/ZCLayout.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Actors/BombBase.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Actors/IceActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Actors/StaticActor.h"
#include "Components/ArrowComponent.h"
#include "Actors/InteractBase.h"
#include "Actors/ZCInventoryPickupActor.h"
#include "Actors/PickupActor.h"
#include "Interface/MyInterface.h"
#include "Combat/ZCAttributeComponent.h"
#include "Combat/ZCCombatComponent.h"
#include "Combat/ZCTargetLockComponent.h"
#include "Combat/ZCTargetLockSpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogZCPlayerCombat, Log, All);

AZCCharBase::AZCCharBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<UZCTargetLockSpringArmComponent>(TEXT("相机臂"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("跟随相机"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	Parachute = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Parachute"));
	Parachute->SetupAttachment(GetMesh());
	Parachute->SetVisibility(false);

	Hair = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hair"));
	Hair->SetupAttachment(GetMesh());
	//Hair->SetCollisionProfileName(TEXT("Cloth"));

	//Hair->SetSimulatePhysics(false); // 布料模拟不直接受物理控制
	//Hair->SetEnableGravity(false);

	Upper = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Upper"));
	Upper->SetupAttachment(GetMesh());

	Lower = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Lower"));
	Lower->SetupAttachment(GetMesh());

	HeadPos = CreateDefaultSubobject<USceneComponent>(TEXT("ReadyThrowPosition"));
	HeadPos->SetupAttachment(GetMesh(),TEXT("socket_head"));

	DropPos = CreateDefaultSubobject<USceneComponent>(TEXT("DropPosition"));
	DropPos->SetupAttachment(GetMesh());

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;

	MagHovered = CreateDefaultSubobject<UMaterialInterface>(TEXT("MagHovered"));
	MagNormal = CreateDefaultSubobject<UMaterialInterface>(TEXT("MagNormal"));
	MagDeactivated = CreateDefaultSubobject<UMaterialInterface>(TEXT("MagDeactivated"));

	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));
	RuneRuntime = CreateDefaultSubobject<UZCRuneRuntimeComponent>(TEXT("RuneRuntime"));
	// 三个组件分别承载属性、战斗状态机和目标锁定生命周期，保持职责分离
	Attributes = CreateDefaultSubobject<UZCAttributeComponent>(TEXT("Attributes"));
	Combat = CreateDefaultSubobject<UZCCombatComponent>(TEXT("Combat"));
	TargetLock = CreateDefaultSubobject<UZCTargetLockComponent>(TEXT("TargetLock"));

	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	// 默认先挂在背部；Combat 初始化后会按状态机和动画时序切换到手部
	SwordMesh->SetupAttachment(GetMesh(), TEXT("WeaponSheath"));
	SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SheathMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SheathMesh"));
	// 剑鞘固定在背部挂点，剑本体负责在手部和剑鞘之间切换
	SheathMesh->SetupAttachment(GetMesh(), TEXT("WeaponSheath"));
	SheathMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	// 盾牌初始位于背部，拔刀完成后由 Combat 切换到左手挂点
	ShieldMesh->SetupAttachment(GetMesh(), TEXT("ShieldBack"));
	ShieldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SwordFinder(
		TEXT("/Game/Assets/Equipment/Sword/MasterSword.MasterSword"));
	SwordMesh->SetStaticMesh(SwordFinder.Object);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SheathFinder(
		TEXT("/Game/Assets/Equipment/Sheath/Sheath.Sheath"));
	SheathMesh->SetStaticMesh(SheathFinder.Object);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShieldFinder(
		TEXT("/Game/Assets/Equipment/Sheild/HylianSheild.HylianSheild"));
	ShieldMesh->SetStaticMesh(ShieldFinder.Object);

	static ConstructorHelpers::FObjectFinder<UInputAction> AttackActionFinder(
		TEXT("/Game/_Game/Data/Inputs/IA_Attack.IA_Attack"));
	AttackAction = AttackActionFinder.Object;

	// 即使旧版本
	// BP_Player 尚未序列化新增属性，也要保持独立解除锁定接线可用
	static ConstructorHelpers::FObjectFinder<UInputAction> TargetUnlockActionFinder(
		TEXT("/Game/_Game/Data/Inputs/IA_TargetUnlock.IA_TargetUnlock"));
	TargetUnlockAction = TargetUnlockActionFinder.Object;
	PhysicsHandle->LinearDamping = 100.0f;//线性阻尼
	PhysicsHandle->LinearStiffness = 325.0f;//硬度
	PhysicsHandle->AngularDamping = 250.0f;//环形阻尼
	PhysicsHandle->AngularStiffness = 750.0f;//环形硬度
	PhysicsHandle->InterpolationSpeed = 5.0f;//差值速度

	PhysicsObjectHolder = CreateDefaultSubobject<USceneComponent>(TEXT("PhysicsObjectHolder"));
	PhysicsObjectHolder->SetupAttachment(FollowCamera);

	IceDisabled = CreateDefaultSubobject<UMaterialInterface>(TEXT("IceDisabled"));
	IceEnabled = CreateDefaultSubobject<UMaterialInterface>(TEXT("IceEnabled"));
}

void AZCCharBase::BeginPlay()
{
	Super::BeginPlay();

	// 独立动作在资源创建并接入前是可选的
	// 在 IMC_ZC 中，BP_Player 上的配置仍是权威来源
	if (!InventoryAction)
	{
		InventoryAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/Game/_Game/Data/Inputs/IA_Inventory.IA_Inventory"));
	}
	if (!OffWeaponAction)
	{
		OffWeaponAction = LoadObject<UInputAction>(
			nullptr, TEXT("/Game/_Game/Data/Inputs/IA_OffWeapon.IA_OffWeapon"));
	}

	if (!GuardAction)
	{
		// 在用户创建并接入 IA_Guard 前保持可选，缺失资源仍会保留
		// 现有输入配置有效，只是不添加该绑定
		GuardAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/Game/_Game/Data/Inputs/IA_Guard.IA_Guard"));
	}

	// 输入接线只对本地玩家生效，但战斗状态仍必须为 AI、服务器和编辑器实例初始化
	if (AZCPlayerController* PC = Cast<AZCPlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (IMC_ZC)
			{
				Subsystem->AddMappingContext(IMC_ZC, 0);
			}
		}
	}

	//初始化精力值
	CurStamina = MaxStamina;
	BroadcastStaminaChanged();

	if (RuneRuntime)
	{
		RuneRuntime->OnActiveRuneChanged.AddDynamic(this, &AZCCharBase::HandleActiveRuneChanged);
	}

	if (Attributes)
	{
		Attributes->OnDeath.AddDynamic(this, &AZCCharBase::HandleDeath);
	}

	if (TargetLock)
	{
		TargetLock->OnTargetChanged.AddUniqueDynamic(this, &AZCCharBase::HandleTargetChanged);
		// 兼容 BeginPlay 前已经设置好的目标，确保移动组件模式与组件状态同步
		HandleTargetChanged(nullptr, TargetLock->GetCurrentTarget());
	}

	if (Combat)
	{
		// BeginPlay 时把角色网格和三件装备交给 Combat，建立统一的挂点控制入口
		Combat->InitializeEquipment(GetMesh(), SwordMesh, SheathMesh, ShieldMesh);
	}
	// InitializeEquipment 会重置瞬态战斗状态，因此重新同步已经
	// 选中的目标，保证进入时已有的锁定仍能启用自动守卫
	if (TargetLock)
	{
		HandleTargetChanged(nullptr, TargetLock->GetCurrentTarget());
	}

	//为磁铁吸附技能事先筛选场景中的Actor，存放在AllMagSMs数组中
	FilterOutAllMagSM();

	//设置tag
	Tags.Add(FName("tag_player"));

	//if (HairPhysicsAsset && Hair)
	//{
	//	// 彻底重置物理状态
	//	Hair->DestroyPhysicsState();
      //  
	//	// 强制加载物理资产
	//	Hair->SetPhysicsAsset(HairPhysicsAsset, true);
        
	//	// 重建物理系统
	//	Hair->CreatePhysicsState();
        
		// 特殊配置：必须开启布料物理混合
	//	Hair->bBlendPhysics = true; // 关键! 允许布料与骨骼混合
	//}
}

void AZCCharBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Attributes)
	{
		Attributes->OnDeath.RemoveDynamic(this, &AZCCharBase::HandleDeath);
	}

	if (TargetLock)
	{
		TargetLock->OnTargetChanged.RemoveDynamic(this, &AZCCharBase::HandleTargetChanged);
		SetTargetLockRotationMode(false);
	}

	if (RuneRuntime)
	{
		// 让正常状态切换路径清理已生成的预览物、手持炸弹
		// 高亮网格和表现状态，再让 Pawn 消失
		RuneRuntime->CancelAll();
		RuneRuntime->OnActiveRuneChanged.RemoveDynamic(this, &AZCCharBase::HandleActiveRuneChanged);
	}
	RestoreStasisState(false);

	//if (Hair)
	//{
	//	Hair->DestroyPhysicsState();
	//}

	Super::EndPlay(EndPlayReason);
}

float AZCCharBase::TakeDamage(
	const float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (bDeathStarted || bDamageProcessing || !CanBeDamaged() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	TGuardValue<bool> DamageGuard(bDamageProcessing, true);
	UpdateGuardSuppression();
	const EZCDefenseHitResult DefenseResult = Combat
		? Combat->ResolveIncomingDamage(DamageEvent, DamageCauser)
		: EZCDefenseHitResult::None;
	if (DefenseResult == EZCDefenseHitResult::Parried)
	{
		if (AZCBokoblinEnemy* Attacker = Cast<AZCBokoblinEnemy>(DamageCauser))
		{
			Attacker->HandleAttackParried();
		}
	}
	if (DefenseResult == EZCDefenseHitResult::Blocked
		|| DefenseResult == EZCDefenseHitResult::Parried
		|| DefenseResult == EZCDefenseHitResult::GuardBroken)
	{
		// 格挡、成功招架和破防命中都会先消费这次攻击接触
		// 再进入 Super 与 Attributes 的生命值结算
		return 0.0f;
	}
	const bool bDamageDuringGuardBreak = DefenseResult == EZCDefenseHitResult::DamageThroughBroken;
	// 先让引擎完成伤害事件处理，再由属性组件执行生命值钳制和死亡闸门
	const float EngineDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!Attributes || !FMath::IsFinite(EngineDamage) || EngineDamage <= 0.0f)
	{
		return 0.0f;
	}

	// 三颗心共六个半心：每次有效命中固定扣半心，不改变敌人的伤害规则
	const float HalfHeartHealth = Attributes->GetMaxHealth() / 6.0f;
	// 消除连续六次减法的浮点尾差，保证最后半心扣完时立即进入死亡
	const float HealthDamage = Attributes->GetHealth() <= HalfHeartHealth + KINDA_SMALL_NUMBER
		? Attributes->GetHealth() : HalfHeartHealth;
	const FZCDamageResult Result = Attributes->ApplyDamage(HealthDamage);
	if (Result.AppliedDamage <= 0.0f)
	{
		return 0.0f;
	}

	// OnDeath 在 ApplyDamage 内同步触发；致死伤害不能先闪出一帧普通受击
	if (Result.bBecameDead)
	{
		// 保留 OnDeath 作为统一入口，同时为 BeginPlay 前等特殊路径提供幂等兜底
		HandleDeath(this);
	}
	else
	{
		// Broken 期间仍按普通生命伤害处理，但不能覆盖当前
		// 正在播放的破防姿态来播放普通受击反应
		if (!bDamageDuringGuardBreak)
		{
			PlayHitReact();
		}
	}
	return Result.AppliedDamage;
}

void AZCCharBase::PlayHitReact()
{
	if (bDeathStarted)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	if (bHitReactActive && HitReactMontage && AnimInstance && AnimInstance->Montage_IsPlaying(HitReactMontage))
	{
		// 重置同一个 Montage，不触发旧结束回调，避免连续受击时提前解除战斗锁定
		AnimInstance->Montage_SetPosition(HitReactMontage, 0.0f);
		return;
	}

	if (RuneRuntime)
	{
		RuneRuntime->CancelAll();
	}
	if (Combat && !Combat->InterruptForHitReaction())
	{
		return;
	}

	if (!HitReactMontage || !AnimInstance || AnimInstance->Montage_Play(HitReactMontage) <= 0.0f)
	{
		if (!bHitReactDiagnosticIssued)
		{
			bHitReactDiagnosticIssued = true;
			UE_LOG(
				LogZCPlayerCombat,
				Warning,
				TEXT("%s received non-lethal damage but cannot play Hit React: Montage or AnimInstance is missing."),
				*GetNameSafe(this));
		}
		if (Combat)
		{
			Combat->ResumeAfterHitReaction();
		}
		return;
	}

	bHitReactActive = true;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AZCCharBase::HandleHitReactMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, HitReactMontage);
}

void AZCCharBase::HandleHitReactMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
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
}

void AZCCharBase::HandleDeath(AActor* DeadActor)
{
	if (bDeathStarted || (DeadActor && DeadActor != this))
	{
		return;
	}

	bDeathStarted = true;
	bHitReactActive = false;
	SetCanBeDamaged(false);
	SetActorTickEnabled(false);

	if (RuneRuntime)
	{
		RuneRuntime->CancelAll();
	}
	if (TargetLock)
	{
		TargetLock->ClearTarget();
	}
	if (Combat)
	{
		Combat->DisableCombat();
	}

	StopJumping();
	ClearDrainRecoverStamina();
	GetWorldTimerManager().ClearTimer(AddGravityForFlyingTimerHandle);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		DisableInput(PlayerController);
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	UAnimInstance* AnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	if (!DeathMontage || !AnimInstance || AnimInstance->Montage_Play(DeathMontage) <= 0.0f)
	{
		if (!bDeathDiagnosticIssued)
		{
			bDeathDiagnosticIssued = true;
			UE_LOG(
				LogZCPlayerCombat,
				Warning,
				TEXT("%s died but cannot play Death Montage: Montage or AnimInstance is missing."),
				*GetNameSafe(this));
		}
	}
}

bool AZCCharBase::CanBeTargetLocked() const
{
	// 没有属性组件时保持兼容；有属性组件时死亡角色不能成为锁定目标
	return (!Attributes || !Attributes->IsDead()) && !bDeathStarted;
}

FVector AZCCharBase::GetTargetLockLocation() const
{
	return GetActorLocation();
}

void AZCCharBase::Landed(const FHitResult& Hit) //着陆逻辑
{
	Super::Landed(Hit);
	if (Combat && !Combat->IsGuardBroken() && CurrentMT != EMovementTypes::MT_Sprinting)
	{
		Combat->SetGuardSuppressed(false);
	}
	if (CurrentMT == EMovementTypes::MT_Exhausted)
	{
		//立刻回复精力
		StartRecoverStamina();
		return;
	}

	if (CurrentMT == EMovementTypes::MT_Gliding)
	{
		//如果在空中，会立刻切换为Falling
		LocomotionManager(EMovementTypes::MT_Walking);
		return;
	}

	if (CurrentMT == EMovementTypes::MT_Sprinting)
	{
		//继续持续冲刺状态
		LocomotionManager(EMovementTypes::MT_Sprinting);
	}
	else
	{
		LocomotionManager(EMovementTypes::MT_Walking);
	}
}

#pragma region Move&Camera Node

void AZCCharBase::Move_Triggered(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		Vel_X = 0.0f;
		Vel_Y = 0.0f;
		return;
	}

	const FVector2d InputVector = val.Get<FVector2d>();
	Vel_X = InputVector.X;
	Vel_Y = InputVector.Y;
	if (Combat
		&& (!FMath::IsNearlyZero(InputVector.X) || !FMath::IsNearlyZero(InputVector.Y))
		&& (Combat->IsAttackActive() || Combat->GetWeaponState() == EZCWeaponState::Attacking))
	{
		// 移动输入立即取消当前攻击，避免攻击蒙太奇与移动同时生效
		Combat->CancelAttack();
	}

	if (TargetLock && TargetLock->HasTarget() && !CanUseTargetLock())
	{
		// 在移动选择方向基准前清除锁定，避免符文或投掷状态
		// 开始后残留一帧相对目标移动
		TargetLock->ClearTarget();
	}

	if (Controller == nullptr) return;

	if (TargetLock && TargetLock->HasTarget())
	{
		AActor* CurrentTarget = TargetLock->GetCurrentTarget();
		const IZCTargetable* Targetable = IsValid(CurrentTarget)
			&& CurrentTarget->GetClass()->ImplementsInterface(UZCTargetable::StaticClass())
			? Cast<IZCTargetable>(CurrentTarget)
			: nullptr;
		const FVector TargetLocation = Targetable
			? Targetable->GetTargetLockLocation()
			: FVector::ZeroVector;
		FVector ForwardToTarget = TargetLocation - GetActorLocation();
		ForwardToTarget.Z = 0.0f;
		if (!Targetable || !Targetable->CanBeTargetLocked() || TargetLocation.ContainsNaN())
		{
			TargetLock->ClearTarget();
		}
		else if (!ForwardToTarget.ContainsNaN() && !ForwardToTarget.IsNearlyZero())
		{
			ForwardToTarget.Normalize();
			const FVector RightOfTarget = FVector::CrossProduct(FVector::UpVector, ForwardToTarget);
			AddMovementInput(RightOfTarget, Vel_X);
			AddMovementInput(ForwardToTarget, Vel_Y);
			return;
		}

	}

	// 未锁定时保持原有的相机相对移动
	const FRotator GroundRotation(0, Controller->GetControlRotation().Yaw, 0);
	const FVector RightDir = FRotationMatrix(GroundRotation).GetUnitAxis(EAxis::Y);
	AddMovementInput(RightDir, Vel_X);
	const FVector FwDir = FRotationMatrix(GroundRotation).GetUnitAxis(EAxis::X);
	AddMovementInput(FwDir, Vel_Y);
}

void AZCCharBase::Move_Completed(const FInputActionValue& val)
{
	Vel_X = 0;
	Vel_Y = 0;
}

void AZCCharBase::Look_Triggered(const FInputActionValue& val)
{
	if (!Controller)
	{
		return;
	}
	const FVector2D LookVal = val.Get<FVector2D>();
	if (LookVal.ContainsNaN() || LookVal.IsNearlyZero())
	{
		return;
	}

	if (TargetLock && TargetLock->HasTarget())
	{
		if (!CanUseTargetLock())
		{
			TargetLock->ClearTarget();
		}
	}

	// Enhanced Input 的缩放、反转和其它修饰器仍由现有 Controller 管线处理
	AddControllerYawInput(LookVal.X);
	AddControllerPitchInput(LookVal.Y);
}

void AZCCharBase::TargetLock_Started(const FInputActionValue& val)
{
	if (!TargetLock || !CanUseTargetLock())
	{
		if (TargetLock && TargetLock->HasTarget())
		{
			TargetLock->ClearTarget();
		}
		return;
	}

	// Enhanced Input 本身只会在本地玩家上触发；这里再显式保护一次，避免
	// 服务器或远程代理角色意外驱动本地目标循环
	if (!IsLocallyControlled())
	{
		return;
	}

	if (TargetLock->HasTarget())
	{
		TargetLock->CycleTarget();
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	if (!ViewLocation.ContainsNaN() && !ViewRotation.ContainsNaN()
		&& !ViewRotation.Vector().IsNearlyZero())
	{
		TargetLock->AcquireBestTarget(ViewLocation, ViewRotation.Vector());
	}
}

void AZCCharBase::TargetUnlock_Started(const FInputActionValue& val)
{
	if (TargetLock)
	{
		TargetLock->ClearTarget();
	}
	else if (Combat)
	{
		Combat->SetTargetLockActive(false);
	}
}

#pragma endregion 

#pragma region Sprint Node
void AZCCharBase::Sprint_Triggered(const FInputActionValue& val)
{
	//用于监听，当无输入且在冲刺状态时，取消冲刺状态进入Walking状态
	if (Vel_X == 0 && Vel_Y == 0 && CurrentMT == EMovementTypes::MT_Sprinting)
	{
		LocomotionManager(EMovementTypes::MT_Walking);
	}
}

void AZCCharBase::Sprint_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	if (CurrentMT ==EMovementTypes::MT_Falling || GetCharacterMovement()->IsFalling())
	{
		return;
	}
	else if (CurrentMT == EMovementTypes::MT_Walking|| CurrentMT == EMovementTypes::MT_EMAX)
	{
		if (Combat)
		{
			Combat->SetGuardSuppressed(true);
		}
		LocomotionManager(EMovementTypes::MT_Sprinting);
	}
}

void AZCCharBase::Sprint_Completed(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	if (CurrentMT == EMovementTypes::MT_Sprinting)
	{
		LocomotionManager(EMovementTypes::MT_Walking);
		if (Combat)
		{
			Combat->SetGuardSuppressed(false);
		}
	}
}

#pragma endregion

#pragma region Jump & Glide Node

void AZCCharBase::JumpGlide_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	if (CurrentMT == EMovementTypes::MT_Exhausted)return;
	

	if (GetCharacterMovement()->MovementMode != MOVE_Falling)
	{
		//可以跳跃
		if (Combat)
		{
			Combat->SetGuardSuppressed(true);
		}
		Jump();
		LocomotionManager(EMovementTypes::MT_Falling);
		return;
	}

	if (CurrentMT == EMovementTypes::MT_Gliding)
	{
		//已经在滑翔状态，则取消滑翔进入下落状态
		LocomotionManager(EMovementTypes::MT_Falling);
		//跳出该函数
		return;
	}

	bool bSkipGroundCheck = bIsInWindTunnel; //在风场中跳过地面检测

	if (!bSkipGroundCheck)
	{
		//检查是否距离地面过近，如果过近，则不能进入滑翔状态
		FHitResult HitResult;
		const FVector Start = GetActorLocation();
		const FVector End = Start - EnableGlideDistance;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);

		bool HitAnything = GetWorld()->LineTraceSingleByChannel(HitResult,Start,End,
			ECollisionChannel::ECC_Visibility,Params);
		//显示射线
		//DrawDebugLine(GetWorld(),Start,End,FColor::Green,false,5.0f,0.0f,3.0f);

		if (HitAnything)
		{
			return;//距离地面过近，不可滑翔
		}
	}
	//如果跳过检测或检测通过，则进入滑翔
	//取消激活释放技能状态
	AutoDeactivateAllRunes();
	if (Combat)
	{
		Combat->SetGuardSuppressed(true);
	}
	//切换至Gliding滑翔状态
	LocomotionManager(EMovementTypes::MT_Gliding);
	
}

void AZCCharBase::JumpGlide_Completed(const FInputActionValue& val)
{
	StopJumping();
}

#pragma endregion

void AZCCharBase::ToggleUI_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken()) return;
	AutoDeactivateAllRunes();

	if (AZCPlayerController* PC = Cast<AZCPlayerController>(Controller))
	{
		PC->ToggleRuneMenu();
	}
}

void AZCCharBase::ActiveRune_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	if (Combat)
	{
		Combat->SetGuardSuppressed(true);
	}
	if (!Combat || Combat->CanAcceptCombatInput())
	{
		ToggleRuneActivity();
		if (Combat && GetActivatedRune() == ERunes::R_EMAX && !InteractingActor)
		{
			Combat->SetGuardSuppressed(false);
		}
	}
}

void AZCCharBase::ReleaseRune_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	if (Combat && !Combat->CanAcceptCombatInput())
	{
		return;
	}
	if (Combat)
	{
		Combat->SetGuardSuppressed(true);
	}

	//检查是否有可投掷的物品
	if (InteractingActor != nullptr)
	{
		// 投掷逻辑，调用 InteractingActor 的投掷方法
		if (APickupActor* Pickup = Cast<APickupActor>(InteractingActor))
		{
			Pickup->ThrowObject(GetThrowDirection()); // 新增方法处理投掷
			InteractingActor = nullptr; // 清空引用
		}
	}
	else
	{
		switch (GetActivatedRune())
		{
		case ERunes::R_EMAX:
			break;
		case ERunes::R_RBS:
			ReleaseRemoteBombSphere();
			break;
		case ERunes::R_RBB:
			ReleaseRemoteBombCube();
			break;
		case ERunes::R_Mag:
			SelectOrReleaseObject();
			break;
		case ERunes::R_Stasis:
			AddStasisForce();
			break;
		case ERunes::R_Ice:
			CreateIce();
			break;
		}
	}

	if (Combat && GetActivatedRune() == ERunes::R_EMAX && !InteractingActor)
	{
		Combat->SetGuardSuppressed(false);
	}
	
}

void AZCCharBase::Interact_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	if (Combat)
	{
		Combat->SetGuardSuppressed(true);
	}

	//取消已经激活的技能
	AutoDeactivateAllRunes();
	if (Combat)
	{
		Combat->SetGuardSuppressed(true);
	}
	
	// 尝试交互
	if (InteractingActor)
	{
		//可能有后续操作比如放下物品
		InteractingActor->NextActionInInteractionActor();
		InteractingActor = nullptr;
	}
	else
	{
		TSet<AActor*> tempActors;
		GetOverlappingActors(tempActors);
		// 忽略普通碰撞体，在有效交互对象中选择距离最近的一个
		float ClosestDistanceSquared = TNumericLimits<float>::Max();
		for (AActor* Actor : tempActors)
		{
			AInteractBase* Candidate = Cast<AInteractBase>(Actor);
			if (!IsValid(Candidate)) continue;
			const float DistanceSquared = FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				InteractingActor = Candidate;
			}
		}
		if (!InteractingActor)
		{
			if (Combat)
			{
				Combat->SetGuardSuppressed(false);
			}
			return;
		}
		// 入包拾取没有举起/放下的后续交互，由拾取物自身等待 Montage 结束
		if (AZCInventoryPickupActor* Pickup = Cast<AZCInventoryPickupActor>(InteractingActor))
		{
			InteractingActor = nullptr;
			Pickup->ToggleInteraction(this);
			if (Combat) Combat->SetGuardSuppressed(false);
			return;
		}
		//开始交互
		InteractingActor->ToggleInteraction(this);
		InteractingActor->ToggleInteractionBP(this);

	}

	if (Combat && GetActivatedRune() == ERunes::R_EMAX && !InteractingActor)
	{
		Combat->SetGuardSuppressed(false);
	}
	
}

void AZCCharBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateGuardSuppression();

	if (TargetLock && TargetLock->HasTarget() && !CanUseTargetLock())
	{
		// 滑翔或符文激活等状态变化会先清除锁定
		// 再让相机或角色朝向读取过期目标
		TargetLock->ClearTarget();
	}

	if (TargetLock && TargetLock->HasTarget())
	{
		UpdateTargetLockOrientation(DeltaTime);
	}

	//检测当前聚焦目标是否是潜在的可磁铁吸附目标，或更新拖拽位置
	MagDragObjTick();

	//更新冰柱所在位置
	UpdateIcePositionTick(DeltaTime);
}

void AZCCharBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIComp= Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EIComp == nullptr) return;

	EIComp->BindAction(MoveAction,ETriggerEvent::Triggered,this,&AZCCharBase::Move_Triggered);
	EIComp->BindAction(MoveAction,ETriggerEvent::Completed,this,&AZCCharBase::Move_Completed);

	EIComp->BindAction(LookAction,ETriggerEvent::Triggered,this,&AZCCharBase::Look_Triggered);

	if (TargetLockAction)
	{
		EIComp->BindAction(TargetLockAction, ETriggerEvent::Started, this, &AZCCharBase::TargetLock_Started);
	}

	// 在旧 BP_Player 资产保存新增暴露属性前，保持其继续可用
	if (!TargetUnlockAction)
	{
		TargetUnlockAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/Game/_Game/Data/Inputs/IA_TargetUnlock.IA_TargetUnlock"));
	}

	if (TargetUnlockAction)
	{
		EIComp->BindAction(TargetUnlockAction, ETriggerEvent::Started, this, &AZCCharBase::TargetUnlock_Started);
	}

	if (!GuardAction)
	{
		GuardAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/Game/_Game/Data/Inputs/IA_Guard.IA_Guard"));
	}
	if (GuardAction)
	{
		EIComp->BindAction(GuardAction, ETriggerEvent::Started, this, &AZCCharBase::Guard_Started);
	}
	
	EIComp->BindAction(SprintAction,ETriggerEvent::Triggered,this,&AZCCharBase::Sprint_Triggered);
	EIComp->BindAction(SprintAction,ETriggerEvent::Completed,this,&AZCCharBase::Sprint_Completed);
	EIComp->BindAction(SprintAction,ETriggerEvent::Started,this,&AZCCharBase::Sprint_Started);

	EIComp->BindAction(JumpGlideAction,ETriggerEvent::Completed,this,&AZCCharBase::JumpGlide_Completed);
	EIComp->BindAction(JumpGlideAction,ETriggerEvent::Started,this,&AZCCharBase::JumpGlide_Started);

	EIComp->BindAction(ToggleUIAction,ETriggerEvent::Started,this,&AZCCharBase::ToggleUI_Started);

	EIComp->BindAction(ActiveRuneAction,ETriggerEvent::Started,this,&AZCCharBase::ActiveRune_Started);

	EIComp->BindAction(InteractAction,ETriggerEvent::Started,this,&AZCCharBase::Interact_Started);

	if (AttackAction)
	{
		// 使用 Started 事件把 IA_Attack 接到角色回调，再由 Combat 决定拔刀或攻击
		EIComp->BindAction(AttackAction, ETriggerEvent::Started, this, &AZCCharBase::Attack_Started);
	}
	if (!OffWeaponAction)
	{
		OffWeaponAction = LoadObject<UInputAction>(
			nullptr, TEXT("/Game/_Game/Data/Inputs/IA_OffWeapon.IA_OffWeapon"));
	}
	if (OffWeaponAction)
	{
		EIComp->BindAction(OffWeaponAction, ETriggerEvent::Started, this, &AZCCharBase::OffWeapon_Started);
	}
}

void AZCCharBase::OffWeapon_Started(const FInputActionValue& val)
{
	if (!bDeathStarted && Combat)
	{
		Combat->RequestSheath();
	}
}

void AZCCharBase::Attack_Started(const FInputActionValue& val)
{
	if (Combat && Combat->IsGuardBroken())
	{
		return;
	}

	// 鼠标左键是统一玩法动作，存在激活符文时优先释放
	// 未激活符文时保留原有 Combat 攻击行为
	if (InteractingActor != nullptr || GetActivatedRune() != ERunes::R_EMAX)
	{
		// 保留现有手持物投掷路径和符文专属释放逻辑
		ReleaseRune_Started(val);
		return;
	}

	if (Combat)
	{
		// 角色不直接修改武器状态，避免输入层绕过 Combat 的状态机策略
		Combat->HandleAttackInput();
	}
}

void AZCCharBase::Guard_Started(const FInputActionValue& val)
{
	UpdateGuardSuppression();
	if (Combat && !Combat->IsGuardBroken())
	{
		Combat->HandleGuardInput();
	}
}
void AZCCharBase::LocomotionManager(EMovementTypes NewMovement)
{
	//控制各个运动
	if (NewMovement == CurrentMT) return;

	const EMovementTypes PreviousMovement = CurrentMT;
	CurrentMT = NewMovement;
	OnMovementTypeChanged.Broadcast(PreviousMovement, CurrentMT);
	BroadcastStaminaChanged();

	//如果在滑翔状态，显示滑翔伞模型
	if (Parachute)
	{
		//显示滑翔伞模型
		Parachute->SetVisibility(CurrentMT == EMovementTypes::MT_Gliding);
	}

	//根据枚举值执行不同运动逻辑
	switch (CurrentMT)
	{
	case EMovementTypes::MT_EMAX:
		break;
	case EMovementTypes::MT_Walking:
		SetWalking();
		break;
	case EMovementTypes::MT_Sprinting:
		SetSprinting();
		break;
	case EMovementTypes::MT_Exhausted:
		SetExhausted();
		break;
	case EMovementTypes::MT_Gliding:
		SetGliding();
		break;
	case EMovementTypes::MT_Falling:
		SetFalling();
		break;
	}
	
}

#pragma region Locomotion
void AZCCharBase::ResetToWalk()
{
	//如正在添加重力，此处取消
	GetWorldTimerManager().ClearTimer(AddGravityForFlyingTimerHandle);
	//重置回地面状态 （从滑翔、下落状态）
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
}

void AZCCharBase::SetSprinting()
{
	//Debug::Print(TEXT("Sprinting"));
	GetCharacterMovement()->MaxWalkSpeed = 1000.0f;
	GetCharacterMovement()->AirControl = 0.35f;

	ResetToWalk();
	//消耗精力值
	StartDrainStamina();
}

void AZCCharBase::SetWalking()
{
	//Debug::Print(TEXT("Walking"));
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->AirControl = 0.35f;

	ResetToWalk();
	//回复精力值
	StartRecoverStamina();
}

void AZCCharBase::SetExhausted()
{
	GetCharacterMovement()->MaxWalkSpeed = 300.0f; //慢速行走
	GetCharacterMovement()->AirControl = 0.35f;

	ClearDrainRecoverStamina(); //清除计时器，停止消耗

	//如果在下落状态，不立刻回复精力，接触地面后再回复精力
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Walking)
	{
		StartRecoverStamina();
	}
	else if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
	{
		ResetToWalk();
	}
	else
	{
		return;
	}
}

void AZCCharBase::SetGliding()
{
	GetCharacterMovement()->AirControl = 0.6f;
	//设为飞行模式
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	StartDrainStamina();
	//设置模拟重力 每帧执行
	GetWorldTimerManager().SetTimer(AddGravityForFlyingTimerHandle, this,
		&AZCCharBase::AddGravityForFlying,GetWorld()->GetDeltaSeconds(),true);
}

void AZCCharBase::SetFalling()
{
	GetCharacterMovement()->AirControl = 0.35f;

	//下降时避免回复精力
	ResetToWalk();
	ClearDrainRecoverStamina();
}

bool AZCCharBase::IsCharacterExhausted()
{
	bool Equal = CurrentMT == EMovementTypes::MT_Exhausted;
	return Equal;
}

float AZCCharBase::GetStaminaRatio() const
{
	return MaxStamina > 0.0f ? FMath::Clamp(CurStamina / MaxStamina, 0.0f, 1.0f) : 0.0f;
}

FVector AZCCharBase::CalculateDropLocation(float ForwardOffset, float TraceDistance) const
{
	FVector Start = GetActorLocation() + (GetActorForwardVector() * ForwardOffset);
	FVector End = Start - FVector(0, 0, TraceDistance);
	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params))
	{
		return HitResult.Location;
	}
	return Start;
}
#pragma endregion

#pragma region Stamina
void AZCCharBase::DrainStamina()
{
	if (CurStamina <= 0)
	{
		LocomotionManager(EMovementTypes::MT_Exhausted);
	}
	else
	{
		CurStamina =FMath::Clamp((CurStamina - StaminaDeletionAmount),0.0f, MaxStamina);
		BroadcastStaminaChanged();
	}
}

void AZCCharBase::StartDrainStamina()
{
	//清除已有计数器
    ClearDrainRecoverStamina();
	
	GetWorldTimerManager().SetTimer(DrainStaminaTimerHandle, this,
		&AZCCharBase::DrainStamina, StaminaDepletionRate, true);
	//显示UI
    if (LayoutRef)
    {
	    LayoutRef->ShowGaugeAnim(true);
    }
}

void AZCCharBase::RecoverStaminaTimer()
{
	if (CurStamina < MaxStamina)
	{
		CurStamina =FMath::Clamp((CurStamina + StaminaDeletionAmount),0.0f, MaxStamina);
		BroadcastStaminaChanged();
	}
	else
	{
		GetWorldTimerManager().ClearTimer(RecoverStaminaTimerHandle);
		LocomotionManager(EMovementTypes::MT_Walking);
		//隐藏UI
		if (LayoutRef)
		{
			LayoutRef->ShowGaugeAnim(false);
		}
	}
}

void AZCCharBase::StartRecoverStamina()
{
	//清除已有计时器
	ClearDrainRecoverStamina();
	
	GetWorldTimerManager().SetTimer(RecoverStaminaTimerHandle, this,
		&AZCCharBase::RecoverStaminaTimer, StaminaDepletionRate, true);
}

void AZCCharBase::ClearDrainRecoverStamina()
{
	GetWorldTimerManager().ClearTimer(DrainStaminaTimerHandle);
	GetWorldTimerManager().ClearTimer(RecoverStaminaTimerHandle);
}

void AZCCharBase::BroadcastStaminaChanged()
{
	OnStaminaChanged.Broadcast(CurStamina, MaxStamina, IsCharacterExhausted());
}

void AZCCharBase::AddGravityForFlying()
{
	//给玩家提供z轴向下的力
	LaunchCharacter(FVector(0.0f,0.0f,-100.0f),false,true);
}

#pragma endregion

#pragma region UI

int32 AZCCharBase::GetWSIndexInfo_Implementation()
{
	Debug::Print(TEXT("可执行CPP代码"));
	return -1;
}
#pragma endregion

#pragma region Runes
bool AZCCharBase::SelectRune(const ERunes RuneType)
{
	return RuneRuntime && RuneRuntime->SelectRune(RuneType);
}

ERunes AZCCharBase::GetSelectedRune() const
{
	return RuneRuntime ? RuneRuntime->GetSelectedRune() : ERunes::R_EMAX;
}

ERunes AZCCharBase::GetActivatedRune() const
{
	return RuneRuntime ? RuneRuntime->GetActiveRune() : ERunes::R_EMAX;
}

void AZCCharBase::AutoDeactivateAllRunes()
{
	if (RuneRuntime && RuneRuntime->CancelAll())
	{
		bReadyToThrow = false;
	}
	if (Combat
		&& !Combat->IsGuardBroken()
		&& GetActivatedRune() == ERunes::R_EMAX
		&& !InteractingActor
		&& CurrentMT != EMovementTypes::MT_Sprinting
		&& GetCharacterMovement()
		&& !GetCharacterMovement()->IsFalling())
	{
		Combat->SetGuardSuppressed(false);
	}
}

void AZCCharBase::ToggleRuneActivity()
{
	if (Combat && !Combat->CanAcceptCombatInput())
	{
		return;
	}

	//若在投掷状态激活技能则取消投掷
	if (InteractingActor)
	{
		if (InteractingActor->BaseMesh != nullptr)
		{
			//可能有后续操作比如放下物品
			//InteractingActor->NextActionInInteractionActor();
			if (IMyInterface* tempInterface = Cast<IMyInterface>(InteractingActor))
			{
				tempInterface->NextAction();
			}
			InteractingActor = nullptr;
		}
	}
	
	if (RuneRuntime)
	{
		const ERunes SelectedRune = RuneRuntime->GetSelectedRune();
		const bool bWillActivate = SelectedRune != ERunes::R_EMAX
			&& RuneRuntime->GetActiveRune() != SelectedRune;

		// 制冰激活包含可能失败的 Trace 与生成步骤，先准备该效果
		// 再发布 ActiveRune 状态切换，避免监听者观察到
		// 同一次广播中立即回滚的激活状态
		if (bWillActivate && SelectedRune == ERunes::R_Ice && !ActivateIceMode())
		{
			return;
		}

		RuneRuntime->ToggleSelectedRune();
	}
}

void AZCCharBase::HandleActiveRuneChanged(const ERunes PreviousRune, const ERunes CurrentRune)
{
	if (Combat && CurrentRune != ERunes::R_EMAX)
	{
		Combat->SetGuardSuppressed(true);
	}

	if (PreviousRune != ERunes::R_EMAX)
	{
		ApplyRuneActivation(PreviousRune, false);
	}

	if (CurrentRune != ERunes::R_EMAX)
	{
		LocomotionManager(EMovementTypes::MT_Walking);
		if (!ApplyRuneActivation(CurrentRune, true))
		{
			// 保护直接或未来的 Runtime 调用，避免在监听器内部递归广播
			// OnActiveRuneChanged
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
				this,
				[this, CurrentRune]
				{
					if (RuneRuntime && RuneRuntime->GetActiveRune() == CurrentRune)
					{
						RuneRuntime->CancelAll();
					}
				}));
			return;
		}
	}

	bFlipflopCrosshair = CurrentRune != ERunes::R_EMAX;
	CrossHairAndCameraMode(bFlipflopCrosshair);
	if (Combat
		&& CurrentRune == ERunes::R_EMAX
		&& !InteractingActor
		&& CurrentMT != EMovementTypes::MT_Sprinting
		&& GetCharacterMovement()
		&& !GetCharacterMovement()->IsFalling()
		&& !Combat->IsGuardBroken())
	{
		Combat->SetGuardSuppressed(false);
	}
}

bool AZCCharBase::ApplyRuneActivation(const ERunes RuneType, const bool bShouldActivate)
{
	switch (RuneType)
	{
	case ERunes::R_RBS:
	case ERunes::R_RBB:
		if (bRBActivated != bShouldActivate)
		{
			ToggleRemoteBomb();
		}
		return bRBActivated == bShouldActivate;
	case ERunes::R_Mag:
		if (bMagActivated != bShouldActivate)
		{
			ToggleMagnesis();
		}
		return bMagActivated == bShouldActivate;
	case ERunes::R_Stasis:
		if (bStasisActivated != bShouldActivate)
		{
			ToggleStasisMode();
		}
		return bStasisActivated == bShouldActivate;
	case ERunes::R_Ice:
		if (bShouldActivate)
		{
			return bIceActivated && IsValid(IceRef);
		}
		if (bIceActivated)
		{
			DeactivateIceMode();
		}
		return !bIceActivated;
	case ERunes::R_EMAX:
	default:
		return true;
	}
}

void AZCCharBase::ToggleRemoteBomb()
{
	bRBActivated = !bRBActivated;

	if (!bRBActivated)
	{
		//取消激活
		bHoldingBomb = false;
		bReadyToThrow = false;

		if (BombRef)
		{
			BombRef->Destroy();
			BombRef = nullptr;
		}
	}
}

void AZCCharBase::ReleaseRemoteBombSphere()
{
	bSphereBomb = true;
	ThrowAndIgniteBomb(bSphereBomb);
}

void AZCCharBase::ReleaseRemoteBombCube()
{
	bSphereBomb = false;
	ThrowAndIgniteBomb(bSphereBomb);
}

void AZCCharBase::ThrowAndIgniteBomb(bool bSphere)
{
	//处理扔出炸弹、引爆炸弹
	if (!bRBActivated) return;

	if (BombRef)
	{
		//若炸弹已生成，判断是投掷还是引爆
		if (bHoldingBomb)
		{
			//开启物理模拟
			BombRef->SM->SetSimulatePhysics(true);
			//设置投掷方向
			FVector ThrowDirection = GetThrowDirection();
			BombRef->SM->SetPhysicsLinearVelocity(ThrowDirection);
			//停止举起状态
			bHoldingBomb = false;
			bReadyToThrow = false;
		}
		else
		{
			//点燃炸弹
			BombRef->Detonate();
			BombRef = nullptr;
		}
	}
	else
	{
		//若炸弹参考无效则生成炸弹
		TSubclassOf<ABombBase> BombClass;
		if (bSphere)
		{
			BombClass = SphereBomb;
		}
		else
		{
			BombClass = CubeBomb;
		}

		if (BombClass == nullptr) return;
		FVector TempLocation = HeadPos->GetComponentLocation();
		BombRef = GetWorld()->SpawnActor<ABombBase>(BombClass,TempLocation,FRotator::ZeroRotator);
		if (!IsValid(BombRef))
		{
			bHoldingBomb = false;
			bReadyToThrow = false;
			return;
		}
		//贴附玩家头顶
		BombRef->AttachToComponent(HeadPos,FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		bHoldingBomb = true;
		//播放举起动画
		bReadyToThrow = true;
	}
}

const FVector AZCCharBase::GetThrowDirection()
{
	FVector  FinalDirection(
	FollowCamera->GetForwardVector().X,FollowCamera->GetForwardVector().Y,0.0f);
	//获取单位向量
	FinalDirection = FinalDirection.GetSafeNormal(0.0001f);
	//加一些高度
	FinalDirection = FinalDirection + FVector(0.0f,0.0f,0.5f);
	//乘以力度大小1000
	FinalDirection = FinalDirection * 1000.0f;
	return FinalDirection;
}

void AZCCharBase::FilterOutAllMagSM()
{
	TArray<AActor*> TempAllMagObjects;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(),StaticMeshClass,TempAllMagObjects);

	for (auto ArrayElem : TempAllMagObjects)
	{
		AStaticMeshActor* LocalMag = Cast<AStaticMeshActor>(ArrayElem);
		// 根据静态网格体材质中是否有Metal物理材质文件筛选
		EPhysicalSurface LocalSurfaceType =
			LocalMag->GetStaticMeshComponent()->GetMaterial(0)->GetPhysicalMaterial()->SurfaceType;
		if (LocalSurfaceType == SurfaceType1)// SurfaceType1相当于自定义的“Metal”
		{
			AllMagSMs.AddUnique(LocalMag);
		}
	}
}

void AZCCharBase::ToggleMagnesis()
{
	bMagActivated = !bMagActivated;

	// 重置所有可吸附物体的材质为“MagDeactive”
	UpdateMagnesisHintMats(AllMagSMs,nullptr);

	if (!bMagActivated)
	{
		//取消激活
		ReleaseMagnesis();
	}
}

void AZCCharBase::ReleaseMagnesis()
{
	//释放Magnesis物品
	PhysicsHandle->ReleaseComponent();
    //重置PhysicsObjectHolder的位置
	PhysicsObjectHolder->SetRelativeLocationAndRotation(FVector(0.0f,0.0f,0.0f),FQuat::Identity);
    //重置MagnesisObj指针为空
	MagnesisObj = nullptr;
	TempMagHitComp = nullptr;
	// 销毁特效
	if (BeamParticleComp != nullptr)
	{
		BeamParticleComp->DestroyComponent();
		BeamParticleComp = nullptr;
	}
}

void AZCCharBase::UpdateMagnesisHintMats(TArray<AStaticMeshActor*> Array, UPrimitiveComponent* HoverObj)
{
	for (auto ArrayElem : AllMagSMs)
	{
		if (ArrayElem == nullptr)return;
		UStaticMeshComponent* LocalSMC = ArrayElem->GetStaticMeshComponent();
		if (HoverObj == LocalSMC)
		{
			//设置材质为高亮
			LocalSMC->SetMaterial(0,MagHovered);
		}
		else
		{
			//是否技能还在激活状态
			if (bMagActivated)
			{
				//如果还在激活，鼠标没指向则恢复普通高亮
				LocalSMC->SetMaterial(0,MagNormal);
			}
			else
			{
				LocalSMC->SetMaterial(0,MagDeactivated);
			}
		}
	}
}

void AZCCharBase::SelectOrReleaseObject()
{
	if (!bMagActivated) return;

	GrabMagObj();
}

void AZCCharBase::GrabMagObj()
{
	if (MagnesisObj)
	{
		ReleaseMagnesis();
		return;
	}
	//抓取，射线检测
	FHitResult HitResult;
	FVector Start;
	FVector End;
	CameraLineTraceDir(Start,End,3000.0f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bReturnPhysicalMaterial = true;//此处重要
	GetWorld()->LineTraceSingleByChannel(HitResult,Start,End,ECC_Visibility,Params);
	
	if (!HitResult.bBlockingHit) return;
	MagnesisObj = HitResult.GetComponent();
	if (MagnesisObj == nullptr) return;
	EPhysicalSurface LocalPhys = UGameplayStatics::GetSurfaceType(HitResult);
	if (LocalPhys != SurfaceType1) return;
	if (!MagnesisObj->IsSimulatingPhysics()) return;
	//使用物理组件PhysicsHandle抓取
	FName CustomNone;
	FVector GrabLocation = MagnesisObj->GetComponentLocation();
	FRotator GrabRotation(0.0f,MagnesisObj->GetComponentRotation().Yaw,0.0f);
	PhysicsHandle->GrabComponentAtLocationWithRotation(MagnesisObj,CustomNone,GrabLocation,GrabRotation);
	PhysicsObjectHolder->SetWorldLocation(MagnesisObj->GetComponentLocation());
	//连线特效
	BeamParticleComp = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),MagDraggingVFX,GetActorLocation());
}

void AZCCharBase::CameraLineTraceDir(FVector& Start, FVector& End, const float Length)
{
	Start = FollowCamera->GetComponentLocation();
	End = Start + FollowCamera->GetForwardVector() * Length;
}

void AZCCharBase::MagDragObjTick()
{
	if (!bMagActivated) return;

	if (MagnesisObj)
	{
		//更新拖拽位置
		FVector NewLocation = PhysicsObjectHolder->GetComponentLocation();
		FRotator NewRotation(0.0f,MagnesisObj->GetComponentRotation().Yaw,0.0f);
		PhysicsHandle->SetTargetLocationAndRotation(NewLocation,NewRotation);

		// 更新连线特效
		if (BeamParticleComp != nullptr)
		{
			//连线由3组起始点组成，共6个点
			//设置第1个点起点
			BeamParticleComp->SetBeamSourcePoint(0,GetActorLocation(),0);
			//设置第1个点终点
			BeamParticleComp->SetBeamTargetPoint(0,MagnesisObj->GetComponentLocation(),0);

			//设置第2个点起点
			BeamParticleComp->SetBeamSourcePoint(1,GetActorLocation(),0);
			//设置第2个点终点
			BeamParticleComp->SetBeamTargetPoint(1,MagnesisObj->GetComponentLocation(),0);

			//设置第3个点起点
			BeamParticleComp->SetBeamSourcePoint(2,GetActorLocation(),0);
			//设置第3个点终点
			BeamParticleComp->SetBeamTargetPoint(2,MagnesisObj->GetComponentLocation(),0);
		}
	}
	else
	{
		//射线检测
		FHitResult HitResult;
        FVector Start;
		FVector	End;
		CameraLineTraceDir(Start,End,3000.0f);
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.bReturnPhysicalMaterial = true;//此处重要
		GetWorld()->LineTraceSingleByChannel(HitResult,Start,End,ECC_Visibility,Params);
		if (!HitResult.bBlockingHit)
		{
			//射线未击中有效对象，重置TempMagHitComp
			TempMagHitComp = nullptr;
			UpdateMagnesisHintMats(AllMagSMs,nullptr);
		}
		else
		{
			if (HitResult.GetComponent() == nullptr) return;
			//有聚焦对象发生变化时再更新，降低消耗
			if (HitResult.GetComponent() == TempMagHitComp) return;
			TempMagHitComp = HitResult.GetComponent();

			EPhysicalSurface LocalPhys = UGameplayStatics::GetSurfaceType(HitResult);
			if (LocalPhys == SurfaceType1 && TempMagHitComp->IsSimulatingPhysics())
			{
				UpdateMagnesisHintMats(AllMagSMs,TempMagHitComp);
			}
			else
			{
				UpdateMagnesisHintMats(AllMagSMs,nullptr);
			}
		}
	}
}

void AZCCharBase::ToggleIceMode()
{
	if (bIceActivated)
	{
		DeactivateIceMode();
	}
	else
	{
		ActivateIceMode();
	}
}

bool AZCCharBase::ActivateIceMode()
{
	if (bIceActivated && IsValid(IceRef))
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World || !IceActorClass)
	{
		return false;
	}

	FHitResult HitResult;
	FVector Start;
	FVector End;
	CameraLineTraceDir(Start,End,3000.0f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	World->LineTraceSingleByChannel(HitResult,Start,End,ECC_Visibility,Params);

	if (!HitResult.bBlockingHit)
	{
		return false;
	}
	//生成视觉上的冰柱对象，表示当前预生成的位置，无碰撞
	AActor* TempActor = World->SpawnActor<AActor>(IceActorClass,HitResult.Location,FRotator(0.0f,0.0f,0.0f));
	if (!TempActor)
	{
		return false;
	}
	IceRef = Cast<AIceActor>(TempActor);
	if (!IceRef)
	{
		TempActor->Destroy();
		return false;
	}

	//激活该模式
	bIceActivated = true;

	//时间轴动画
	IceRef->StartPlayAnimationLoop();
	return true;
}

void AZCCharBase::DeactivateIceMode()
{
	if (IsValid(IceRef))
	{
		//停止时间轴动画
		IceRef->StopPlayAnimation();
		//销毁IceRef并重置
		IceRef->Destroy();
	}
	IceRef = nullptr;
	//取消激活
	bIceActivated = false;
}

void AZCCharBase::UpdateIcePositionTick(float DeltaTime)
{
	//IceRef和bIceActivated都为假时跳出函数
	if (!IceRef || !bIceActivated) return;

	//检查射线是否检测到通道为Water的物体对象
	FHitResult HitResult;
	FVector Start;
	FVector End;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	CameraLineTraceDir(Start,End,5000.0f);
	//根据物体类型射线检测
	bool bHitAny = GetWorld()->LineTraceSingleByObjectType(HitResult,Start,End,ObjectParams,Params);
	if (!bHitAny)
	{
		//隐藏视觉显示的冰柱
		IceRef->BaseSceneRoot->SetVisibility(false,true);
		//停止执行
		return;
	}

	IceRef->BaseSceneRoot->SetVisibility(true,true);
	//平滑移动视觉显示的冰柱，使用插值
	FVector tempTargetLocation = HitResult.Location;
	FVector finalTargetLocation = UKismetMathLibrary::VInterpTo(IceRef->GetActorLocation(),tempTargetLocation,DeltaTime,10.0f);
	//每帧更新，实现缓动效果
	IceRef->SetActorLocation(finalTargetLocation);
	//把击中物体的法线方向转换成角度
	FRotator LocalRotator = UKismetMathLibrary::MakeRotFromZ(HitResult.ImpactNormal);
	IceRef->SetActorRotation(LocalRotator);
	//检查是否重叠
	IceRef->bCanPlace = IceRef->CheckOverlap();
	if (IceRef->bCanPlace)
	{
		IceRef->IceMesh->SetMaterial(0,IceEnabled);
	}
	else
	{
		IceRef->IceMesh->SetMaterial(0,IceDisabled);
	}
}

void AZCCharBase::CreateIce()
{
	if (bIceActivated && IsValid(IceRef))
	{
		IceRef->SpawnIce();
	}
}

void AZCCharBase::ToggleStasisMode()
{
	bStasisActivated = !bStasisActivated;
}

void AZCCharBase::AddStasisForce()
{
	if (!bStasisActivated) return;

	StartStasis();
}

void AZCCharBase::StartStasis()
{
	bool bSPhysics = false;
	UPrimitiveComponent* tempComp = nullptr;
	if (StasisComp)
	{
		//添加力
		StasisTrace(tempComp,bSPhysics);
		//一段时间内只能向同一个Actor加力，如果不是，则忽略新加力指令
		if (tempComp != StasisComp)return;
		AddForceForStasisActor();
	}
	else
	{
		//激活当前StasisActor
		StasisTrace(tempComp,bSPhysics);
		if (!bSPhysics)return;
		StasisComp = tempComp;
		StasisComp->SetSimulatePhysics(false);
		//将原始材质存起来，方便取消激活状态时外观回复原状
		OriginMatStasis = StasisComp->GetMaterial(0);
		//激活时显示特殊材质
		StasisComp->SetMaterial(0,StasisMat);
		//设置Timer,延时五秒
		GetWorld()->GetTimerManager().SetTimer(StasisTimerHandle,this,&AZCCharBase::BreakStasis,5.0f,false);
	}
}

void AZCCharBase::StasisTrace(UPrimitiveComponent*& HitComp, bool& bSimulatePhysc)
{
	FHitResult HitResult;
	FVector Start;
	FVector End;
	CameraLineTraceDir(Start,End,3000.0f);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	bool HitAny = GetWorld()->LineTraceSingleByChannel(HitResult,Start,End,ECC_Visibility,Params);

	if (HitAny)
	{
		HitComp = HitResult.GetComponent();
		bSimulatePhysc = HitResult.GetComponent()->IsSimulatingPhysics();
	}
	else
	{
		HitComp = nullptr;
		bSimulatePhysc = false;
	}
}

void AZCCharBase::AddForceForStasisActor()
{
	if (!IsValid(StasisComp) || !StasisClass || !GetWorld())
	{
		return;
	}

	//生成
	if (!IsValid(StasisForceActor))
	{
		StasisForceActor = Cast<AStaticActor>(
			GetWorld()->SpawnActor<AActor>(StasisClass,StasisComp->GetComponentLocation(),FRotator::ZeroRotator));
	}
	if (!IsValid(StasisForceActor))
	{
		return;
	}
	//更新StasisForceActor中的箭头方向，视觉上
	FRotator Rot = UKismetMathLibrary::Conv_VectorToRotator(FollowCamera->GetForwardVector()) ;
	UArrowComponent* ArrowCompStasis = StasisForceActor->GetArrowComponent();
	if (!ArrowCompStasis)
	{
		return;
	}
	ArrowCompStasis->SetWorldRotation(Rot);
	StasisForceActor->UpdateForceInfo();
}

void AZCCharBase::BreakStasis()
{
	RestoreStasisState(true);
}

void AZCCharBase::RestoreStasisState(const bool bApplyStoredImpulse)
{
	GetWorldTimerManager().ClearTimer(StasisTimerHandle);

	if (IsValid(StasisComp))
	{
		//重新设置物理模式 十分重要
		StasisComp->SetSimulatePhysics(true);
		if (bApplyStoredImpulse && IsValid(StasisForceActor))
		{
			StasisComp->AddImpulse(StasisForceActor->GteImpulse(), NAME_None, true);
		}
		//恢复原有材质
		StasisComp->SetMaterial(0,OriginMatStasis);
	}

	if (IsValid(StasisForceActor))
	{
		StasisForceActor->Destroy();
	}
	StasisForceActor = nullptr;
	StasisComp = nullptr;
	OriginMatStasis = nullptr;
}

void AZCCharBase::ReadyToThrow(UStaticMeshComponent* SMRef)
{
	if (SMRef == nullptr) return;
	if (Combat)
	{
		Combat->SetGuardSuppressed(true);
	}
	SMRef->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	SMRef->SetSimulatePhysics(true);
	CrossHairAndCameraMode(false);
	bReadyToThrow = false;
}

void AZCCharBase::HandleTargetChanged(AActor* PreviousTarget, AActor* CurrentTarget)
{
	const IZCTargetable* Targetable = IsValid(CurrentTarget)
		&& CurrentTarget->GetClass()->ImplementsInterface(UZCTargetable::StaticClass())
		? Cast<IZCTargetable>(CurrentTarget)
		: nullptr;
	const bool bHasValidTarget = Targetable && Targetable->CanBeTargetLocked();
	if (bHasValidTarget && PreviousTarget != CurrentTarget)
	{
		// 连续切敌沿用正在进行的角速度，从实际朝向继续转，不重放旧姿态
		if (!bTargetSwitchTurnActive)
		{
			TargetSwitchYawVelocity = 0.0f;
		}
		bTargetSwitchTurnActive = true;
	}
	SetTargetLockRotationMode(bHasValidTarget);
	if (Combat)
	{
		Combat->SetTargetLockActive(bHasValidTarget);
	}
}

bool AZCCharBase::CanUseTargetLock() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bIsGliding = CurrentMT == EMovementTypes::MT_Gliding
		|| (Movement && Movement->MovementMode == EMovementMode::MOVE_Flying);
	return CanBeTargetLocked()
		&& !bIsGliding
		&& GetActivatedRune() == ERunes::R_EMAX
		&& !InteractingActor
		&& !bReadyToThrow;
}

void AZCCharBase::UpdateTargetLockOrientation(const float DeltaTime)
{
	if (bDeathStarted || !TargetLock || !TargetLock->HasTarget())
	{
		return;
	}

	AActor* CurrentTarget = TargetLock->GetCurrentTarget();
	if (!IsValid(CurrentTarget) || !CurrentTarget->GetClass()->ImplementsInterface(UZCTargetable::StaticClass()))
	{
		TargetLock->ClearTarget();
		return;
	}

	const IZCTargetable* Targetable = Cast<IZCTargetable>(CurrentTarget);
	if (!Targetable || !Targetable->CanBeTargetLocked())
	{
		// 目标死亡等状态变化应立即退出锁定朝向，不等待组件下一次 Tick 兜底
		TargetLock->ClearTarget();
		return;
	}

	const FVector ToTarget = Targetable->GetTargetLockLocation() - GetActorLocation();
	if (ToTarget.ContainsNaN())
	{
		return;
	}

	const FVector HorizontalDirection(ToTarget.X, ToTarget.Y, 0.0f);
	if (HorizontalDirection.SizeSquared() <= FMath::Square(KINDA_SMALL_NUMBER))
	{
		return;
	}

	const float SafeDeltaTime = FMath::IsFinite(DeltaTime) ? FMath::Max(DeltaTime, 0.0f) : 0.0f;
	const float MaxYawStep = FMath::Max(TargetLockRotationSpeed, 0.0f) * SafeDeltaTime;
	if (!FMath::IsFinite(MaxYawStep))
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	if (!FMath::IsFinite(CurrentRotation.Yaw))
	{
		return;
	}

	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(HorizontalDirection.Y, HorizontalDirection.X));
	if (!FMath::IsFinite(TargetYaw))
	{
		return;
	}

	FRotator NewRotation = CurrentRotation;
	if (bTargetSwitchTurnActive)
	{
		const float MaxSpeed = FMath::IsFinite(TargetSwitchRotationSpeed)
			? FMath::Max(TargetSwitchRotationSpeed, 0.0f) : 420.0f;
		const float Acceleration = FMath::IsFinite(TargetSwitchRotationAcceleration)
			? FMath::Max(TargetSwitchRotationAcceleration, 1.0f) : 1800.0f;
		const float SlowdownAngle = FMath::IsFinite(TargetSwitchSlowdownAngle)
			? FMath::Max(TargetSwitchSlowdownAngle, 1.0f) : 60.0f;
		if (!FMath::IsFinite(TargetSwitchYawVelocity))
		{
			TargetSwitchYawVelocity = 0.0f;
		}

		// 短子步让加减速在不同帧率下保持一致，也防止大帧间隔跨过制动区
		float RemainingTime = FMath::Min(SafeDeltaTime, 0.25f);
		while (RemainingTime > KINDA_SMALL_NUMBER && bTargetSwitchTurnActive)
		{
			const float StepTime = FMath::Min(RemainingTime, 1.0f / 60.0f);
			RemainingTime -= StepTime;
			const float YawError = FMath::FindDeltaAngleDegrees(static_cast<float>(NewRotation.Yaw), TargetYaw);
			const float DesiredVelocity = MaxSpeed * FMath::Clamp(YawError / SlowdownAngle, -1.0f, 1.0f);
			const float PreviousVelocity = TargetSwitchYawVelocity;
			TargetSwitchYawVelocity = FMath::FInterpConstantTo(
				PreviousVelocity, DesiredVelocity, StepTime, Acceleration);
			const float YawStep = (PreviousVelocity + TargetSwitchYawVelocity) * 0.5f * StepTime;
			const bool bReachedTarget = (FMath::Abs(YawError) <= 0.5f && FMath::Abs(TargetSwitchYawVelocity) <= 10.0f)
				|| (YawStep * YawError > 0.0f && FMath::Abs(YawStep) >= FMath::Abs(YawError));
			if (bReachedTarget)
			{
				NewRotation.Yaw = TargetYaw;
				TargetSwitchYawVelocity = 0.0f;
				bTargetSwitchTurnActive = false;
			}
			else
			{
				NewRotation.Yaw = FMath::UnwindDegrees(NewRotation.Yaw + YawStep);
			}
		}
	}
	else
	{
		NewRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, FMath::UnwindDegrees(TargetYaw), MaxYawStep);
	}
	if (FMath::IsFinite(NewRotation.Yaw))
	{
		SetActorRotation(NewRotation);
	}
}

void AZCCharBase::SetTargetLockRotationMode(const bool bEnableTargetLockRotation)
{
	bTargetLockRotationActive = bEnableTargetLockRotation;
	if (!bEnableTargetLockRotation)
	{
		bTargetSwitchTurnActive = false;
		TargetSwitchYawVelocity = 0.0f;
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = !bEnableTargetLockRotation;
	}
}

bool AZCCharBase::CanMaintainGuard() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return !bDeathStarted && Movement && Movement->IsMovingOnGround()
		&& CurrentMT != EMovementTypes::MT_Sprinting
		&& CurrentMT != EMovementTypes::MT_Falling
		&& CurrentMT != EMovementTypes::MT_Gliding
		&& GetActivatedRune() == ERunes::R_EMAX
		&& !InteractingActor && !bReadyToThrow;
}

void AZCCharBase::UpdateGuardSuppression()
{
	if (!Combat)
	{
		return;
	}

	const bool bGameplaySuppressed = Combat->IsGuardBroken()
		|| Combat->GetWeaponState() == EZCWeaponState::Attacking
		|| !CanMaintainGuard();
	Combat->SetGuardSuppressed(bGameplaySuppressed);
}

#pragma endregion
