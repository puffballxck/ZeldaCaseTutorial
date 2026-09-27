#include "Characters/ZCCharacterMovementComponent.h"
#include "Characters/ZCCharBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogZCClimbing, Log, All);

namespace
{
// Channel2 只由允许攀爬的静态墙面阻挡，Pawn 的常规碰撞仍由胶囊 Sweep 处理
constexpr ECollisionChannel ClimbChannel = ECC_GameTraceChannel2;
// 翻上改为代码位移后不再创建此目标，清理时仍移除旧配置可能留下的同名目标
const FName MantleTarget(TEXT("ClimbMantle"));
// 下爬 Montage 的两个 Motion Warping Notify 依次使用边缘中继点和贴墙终点
const FName DownForwardTarget(TEXT("ClimbDownForward"));
const FName DownTarget(TEXT("ClimbDown"));
bool IsTraversalMode(EMovementMode Mode, uint8 Custom)
{
	return Mode == MOVE_Custom && Custom >= uint8(EZCCustomMovementMode::Climbing)
		&& Custom <= uint8(EZCCustomMovementMode::ClimbDownLedge);
}
}

bool UZCCharacterMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == uint8(EZCCustomMovementMode::Climbing);
}

bool UZCCharacterMovementComponent::IsClimbTraversalActive() const
{
	return IsTraversalMode(MovementMode, CustomMovementMode);
}

bool UZCCharacterMovementComponent::ShouldUseClimbBasePose(float BlendLeadTime) const
{
	// 仅翻上平台需要在 Montage 尾段提前准备地面基础姿势
	if (!IsClimbTraversalActive()) return false;
	if (CustomMovementMode != uint8(EZCCustomMovementMode::Mantling)) return true;
	UAnimInstance* Anim = MantleAnimInstance.Get();
	const UAnimMontage* Montage = ActiveTransitionMontage.Get();
	const FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(TransitionMontageInstance) : nullptr;
	if (!Montage || !Instance || Instance->Montage != Montage) return true;
	// 正常混出仍由 PhysMantle 驱动；中断会经原来的委托立即处理，不在这里切移动模式
	if (Instance->IsStopped()) return false;
	const float PlayRate = Instance->GetPlayRate() * Montage->RateScale;
	if (!Instance->bEnableAutoBlendOut || PlayRate <= KINDA_SMALL_NUMBER) return true;
	const float BlendOutTime = Montage->BlendOutTriggerTime >= 0.0f ? Montage->BlendOutTriggerTime
		: Montage->GetDefaultBlendOutTime() * Instance->DefaultBlendTimeMultiplier;
	// 当前上墙 Montage 为单段正向播放；使用实例时间，暂停不会被墙钟计时强行推进
	const float TimeToEnd = (Montage->GetPlayLength() - Instance->GetPosition()) / PlayRate;
	return TimeToEnd > BlendOutTime + FMath::Max(0.0f, BlendLeadTime);
}

float UZCCharacterMovementComponent::GetMaxSpeed() const
{
	return IsClimbing() ? MaxClimbSpeed : Super::GetMaxSpeed();
}

float UZCCharacterMovementComponent::GetMaxAcceleration() const
{
	return IsClimbing() ? ClimbAcceleration : Super::GetMaxAcceleration();
}

float UZCCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	return IsClimbing() ? ClimbBraking : Super::GetMaxBrakingDeceleration();
}

FCollisionQueryParams UZCCharacterMovementComponent::MakeClimbQuery() const
{
	// 自身和挂在身上的装备不能充当可攀爬墙面
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZCClimb), false, CharacterOwner);
	if (CharacterOwner)
	{
		TArray<AActor*> Attached;
		CharacterOwner->GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}
	return Params;
}

bool UZCCharacterMovementComponent::IsClimbHit(const FHitResult& Hit) const
{
	// 命中必须来自静态、非模拟物理的表面，普通角色和动态物体不参与首版攀爬
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!Hit.IsValidBlockingHit() || !IsValid(Component) || Component->Mobility != EComponentMobility::Static
		|| Component->IsSimulatingPhysics() || Cast<APawn>(Hit.GetActor()) || Hit.ImpactNormal.ContainsNaN())
	{
		return false;
	}
	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	// 以世界竖直分量换算墙面对水平面的倾角，夹紧 Dot 避免 Acos 的数值越界
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.Z, -1.0, 1.0)));
	return !Normal.IsNearlyZero() && Angle >= MinWallAngle && Angle <= MaxWallAngle + 0.1f;
}

float UZCCharacterMovementComponent::GetWallOffset(const FVector& Normal) const
{
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	// 直立胶囊沿墙面法线的支撑半径，斜墙不能仅使用胶囊 Radius
	return Radius + (Capsule->GetScaledCapsuleHalfHeight() - Radius) * FMath::Abs(Normal.Z) + SurfaceGap;
}

bool UZCCharacterMovementComponent::QuerySurface(
	const FVector& Center, const FVector& Facing, FSurface& OutSurface, bool bPredict) const
{
	OutSurface = FSurface();
	if (!HasValidData() || Facing.IsNearlyZero()) return false;
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float HeightOffset = FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
	const float Reach = Capsule->GetScaledCapsuleRadius() + WallReach + HeightOffset * 0.5f;
	const FVector Forward = Facing.GetSafeNormal2D();
	const FVector Prediction = bPredict ? Velocity.GetClampedToMaxSize(MaxClimbSpeed) * 0.12f : FVector::ZeroVector;
	TArray<FHitResult, TInlineAllocator<4>> Hits;
	// 中心、上端和下端分别探测，移动中额外查询预测位置以提前发现墙面缺口
	for (int32 Index = 0; Index < (bPredict && !Prediction.IsNearlyZero() ? 4 : 3); ++Index)
	{
		const float Height = Index == 1 ? HeightOffset : Index == 2 ? -HeightOffset : 0.0f;
		const FVector Start = Center + FVector::UpVector * Height + (Index == 3 ? Prediction : FVector::ZeroVector);
		const FVector End = Start + Forward * Reach;
		FHitResult Hit;
		GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ClimbChannel,
			FCollisionShape::MakeSphere(6.0f), MakeClimbQuery());
		if (bDrawClimbDebug) DrawDebugLine(GetWorld(), Start, End, Hit.bBlockingHit ? FColor::Green : FColor::Red);
		if (IsClimbHit(Hit) && FVector::DotProduct(-Forward, Hit.ImpactNormal.GetSafeNormal2D()) > 0.65f)
		{
			Hits.Add(Hit);
		}
	}
	if (Hits.IsEmpty()) return false;
	// 攀爬中以旧法线作为参考，阻止墙缝两侧或急转角命中被平均成虚构平面
	const FVector Reference = IsClimbing() && !CurrentSurface.Normal.IsNearlyZero()
		? CurrentSurface.Normal : Hits[0].ImpactNormal;
	int32 ValidCount = 0;
	for (const FHitResult& Hit : Hits)
	{
		// 首版只合并连续墙面，不把急转角或对向墙的法线平均成虚构表面
		if (FVector::DotProduct(Reference, Hit.ImpactNormal) < 0.85f) continue;
		OutSurface.Point += Hit.ImpactPoint;
		OutSurface.Normal += Hit.ImpactNormal;
		if (!OutSurface.Component.IsValid()) OutSurface.Component = Hit.GetComponent();
		++ValidCount;
	}
	if (!ValidCount) return false;
	OutSurface.Point /= ValidCount;
	OutSurface.Normal.Normalize();
	return !OutSurface.Normal.IsNearlyZero();
}

bool UZCCharacterMovementComponent::IsCapsuleClear(const FVector& Center) const
{
	// 完整直立胶囊空间检查，与角色实际碰撞响应保持一致
	if (!HasValidData()) return false;
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	return !GetWorld()->OverlapBlockingTestByChannel(Center, FQuat::Identity, Capsule->GetCollisionObjectType(),
		Capsule->GetCollisionShape(), MakeClimbQuery(), FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
}

bool UZCCharacterMovementComponent::IsPathClear(const FVector& Start, const FVector& End) const
{
	// 过渡路径先扫完整胶囊，再检查终点是否被静态物体占据
	if (!HasValidData()) return false;
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	FHitResult Hit;
	GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, Capsule->GetCollisionObjectType(),
		Capsule->GetCollisionShape(), MakeClimbQuery(), FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	return !Hit.bBlockingHit && IsCapsuleClear(End);
}

bool UZCCharacterMovementComponent::FindStandingFloor(const FVector& Center, FFindFloorResult& OutFloor) const
{
	FindFloor(Center, OutFloor, false);
	if (!OutFloor.IsWalkableFloor() || OutFloor.FloorDist > MAX_FLOOR_DIST + 3.0f || !IsCapsuleClear(Center)) return false;
	// 脚下中心有效还不够，四个支撑采样防止窄边缘触发 Walking 后立刻掉落
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() * 0.65f;
	for (const FVector& Offset : {FVector(Radius, 0, 0), FVector(-Radius, 0, 0), FVector(0, Radius, 0), FVector(0, -Radius, 0)})
	{
		FHitResult Hit;
		const FVector Start = Center + Offset;
		GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + 8.0f),
			Capsule->GetCollisionObjectType(), MakeClimbQuery(), FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
		if (!Hit.IsValidBlockingHit() || !IsWalkable(Hit)) return false;
	}
	return true;
}

void UZCCharacterMovementComponent::BuildWallAxes(FVector& Up, FVector& Right) const
{
	// 世界 Up 投影到墙面形成攀爬上方向，再由墙面法线求局部右方向
	Up = FVector::VectorPlaneProject(FVector::UpVector, CurrentSurface.Normal).GetSafeNormal();
	Right = FVector::CrossProduct(Up, -CurrentSurface.Normal).GetSafeNormal();
}

FVector UZCCharacterMovementComponent::GetClimbInputDirection(const FVector2D& Input) const
{
	if (!IsClimbing() || Input.ContainsNaN()) return FVector::ZeroVector;
	FVector Up, Right;
	BuildWallAxes(Up, Right);
	return (Right * Input.X + Up * Input.Y).GetClampedToMaxSize(1.0f);
}

void UZCCharacterMovementComponent::ResetClimbInput()
{
	// 清掉移动组件和角色尚未消费的输入，避免切模式后沿旧方向继续走
	EntryTime = 0;
	Acceleration = FVector::ZeroVector;
	if (CharacterOwner) CharacterOwner->ConsumeMovementInputVector();
}

bool UZCCharacterMovementComponent::CanAttemptClimb() const
{
	// 角色负责精力、战斗、符文等玩法互斥，CMC 只接收可进入的物理模式
	const AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner);
	return Player && Player->CanStartClimbing() && (IsMovingOnGround() || IsFalling()
		|| (MovementMode == MOVE_Flying && Player->CurrentMT == EMovementTypes::MT_Gliding));
}

void UZCCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// 自动抓墙在移动更新前决定，地面用持续输入计时，空中和滑翔用朝向加接触距离
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	if (!HasValidData()) return;
	if (IsClimbTraversalActive()) return;
	const bool bGrounded = IsMovingOnGround();
	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	if (bNeedsInputRelease && (InputDirection.IsNearlyZero()
		|| FVector::DotProduct(InputDirection, -ReleasedNormal.GetSafeNormal2D()) < 0.2f))
	{
		bNeedsInputRelease = false;
	}
	// 松手后即使没有方向输入也不能原地抓回，离墙或落地后才重新开放空中抓墙
	if (bNeedsWallSeparation)
	{
		FSurface ReleasedSurface;
		bNeedsWallSeparation = !bGrounded
			&& QuerySurface(Center, -ReleasedNormal, ReleasedSurface, false)
			&& FVector::DotProduct(Center - ReleasedSurface.Point, ReleasedSurface.Normal)
				<= GetWallOffset(ReleasedSurface.Normal) + AirContactDistance + 5.0f;
	}
	const FVector Facing = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	FSurface Surface;
	// 任一进入条件失效都清空地面计时，下一次靠近不能继承旧的按住时长
	if (!CanAttemptClimb() || bNeedsWallSeparation || (bGrounded && bNeedsInputRelease)
		|| GetWorld()->GetTimeSeconds() < RegrabAllowedAt
		|| (bGrounded && InputDirection.IsNearlyZero()) || !QuerySurface(Center, Facing, Surface, false)
		|| (bGrounded && FVector::DotProduct(InputDirection, -Surface.Normal.GetSafeNormal2D()) <= 0.65f))
	{
		EntryTime = 0;
		return;
	}
	const float WallOffset = GetWallOffset(Surface.Normal);
	const float WallDistance = FVector::DotProduct(Center - Surface.Point, Surface.Normal);
	const FVector Target = Center + Surface.Normal * (WallOffset - WallDistance);
	// 空中按胶囊实际支撑半径判断接触，避免使用较长探测距离隔空吸墙
	if ((!bGrounded && WallDistance - (WallOffset - SurfaceGap) > AirContactDistance)
		|| FVector::Dist(Center, Target) > WallReach || !IsPathClear(Center, Target))
	{
		EntryTime = 0;
		return;
	}
	if (!bGrounded)
	{
		// 空中确认近距离接触且目标胶囊可达后立即抓墙，不等待 GroundEntryDuration
		EntryTime = 0;
		EnterClimbing(Surface);
		return;
	}
	EntryTime += DeltaSeconds;
	if (EntryTime >= GroundEntryDuration) EnterClimbing(Surface);
}

void UZCCharacterMovementComponent::EnterClimbing(const FSurface& Surface)
{
	CurrentSurface = Surface;
	bNeedsWallSeparation = false;
	bNeedsInputRelease = false;
	SurfaceLostTime = 0;
	StopMovementImmediately();
	SetMovementMode(MOVE_Custom, uint8(EZCCustomMovementMode::Climbing));
}

void UZCCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// MovementMode 是物理真相，角色的 CurrentMT 与战斗、符文状态在模式稳定后同步
	const bool bWasTraversal = IsTraversalMode(PreviousMovementMode, PreviousCustomMode);
	const bool bNowTraversal = IsClimbTraversalActive();
	const bool bTraversalModeChanged = bWasTraversal && bNowTraversal
		&& PreviousCustomMode != CustomMovementMode;
	if (!bWasTraversal && bNowTraversal)
	{
		// 暂停引擎默认朝移动方向旋转，让攀爬物理自行面向墙壁
		bSavedOrientRotation = bOrientRotationToMovement;
		bSavedControllerDesiredRotation = bUseControllerDesiredRotation;
		bOrientRotationToMovement = false;
		bUseControllerDesiredRotation = false;
		PendingLaunchVelocity = FVector::ZeroVector;
		ClearAccumulatedForces();
		CharacterOwner->StopJumping();
	}
	if (bWasTraversal && !bNowTraversal)
	{
		// 所有退出路径共享清理，不能留下旧表面、旋转标志或 Montage 回调
		ClearTransition();
		CurrentSurface = FSurface();
		SurfaceLostTime = 0;
		bOrientRotationToMovement = bSavedOrientRotation;
		bUseControllerDesiredRotation = bSavedControllerDesiredRotation;
		RegrabAllowedAt = FMath::Max(RegrabAllowedAt, double(GetWorld()->GetTimeSeconds()) + RegrabDelay);
	}
	ClimbLocalVelocity = FVector2D::ZeroVector;
	ResetClimbInput();
	if (AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner))
	{
		if (bWasTraversal != bNowTraversal || bTraversalModeChanged)
		{
			Player->HandleClimbTraversalChanged(bNowTraversal);
		}
	}
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UZCCharacterMovementComponent::PhysFlying(float DeltaTime, int32 Iterations)
{
	const AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner);
	if (!Player || Player->CurrentMT != EMovementTypes::MT_Gliding)
	{
		Super::PhysFlying(DeltaTime, Iterations);
		return;
	}
	if (DeltaTime < MIN_TICK_TIME || !HasValidData()) return;

	RestorePreAdditiveRootMotionVelocity();
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		// 水平运动沿用 Flying 的加速与阻尼，缓降直接参与本帧碰撞移动
		Velocity.Z = 0.0f;
		CalcVelocity(DeltaTime, 0.5f * GetPhysicsVolume()->FluidFriction, true, GetMaxBrakingDeceleration());
		Velocity.Z = -100.0f;
	}
	ApplyRootMotionToVelocity(DeltaTime);

	++Iterations;
	bJustTeleported = false;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit(1.0f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (!HasValidData() || MovementMode != MOVE_Flying) return;
	if (Hit.IsValidBlockingHit())
	{
		// Flying 不会自动着陆；滑翔撞到可站立地面时交回标准着陆流程
		if (Velocity.Z <= 0.0f && IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
		{
			SetMovementMode(MOVE_Falling);
			ProcessLanded(Hit, DeltaTime * (1.0f - Hit.Time), Iterations);
			return;
		}
		HandleImpact(Hit, DeltaTime, Delta);
		if (!HasValidData() || MovementMode != MOVE_Flying) return;
		SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}
	if (HasValidData() && MovementMode == MOVE_Flying && !bJustTeleported
		&& !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

void UZCCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!IsClimbTraversalActive()) Super::PhysicsRotation(DeltaTime);
}

void UZCCharacterMovementComponent::StopClimbing(EZCClimbExitReason Reason)
{
	if (!IsClimbTraversalActive()) return;
	// 松手除固定冷却外还要释放朝墙输入或离开原墙，防止下一帧自动重新抓回
	ReleasedNormal = CurrentSurface.Normal;
	bNeedsInputRelease = Reason == EZCClimbExitReason::Released;
	bNeedsWallSeparation = Reason == EZCClimbExitReason::Released;
	RegrabAllowedAt = GetWorld()->GetTimeSeconds() + RegrabDelay;
	ClearTransition();
	StopMovementImmediately();
	SetMovementMode(Reason == EZCClimbExitReason::Death ? MOVE_None : MOVE_Falling);
}

void UZCCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (IsClimbing()) PhysClimbing(DeltaTime, Iterations);
	else if (IsClimbTraversalActive()) PhysTransition(DeltaTime, Iterations);
	else Super::PhysCustom(DeltaTime, Iterations);
}

void UZCCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations)
{
	// 按 CMC 子步推进，探测、切向移动、贴墙修正和精力消耗使用同一模拟步长
	float Remaining = DeltaTime;
	while (Remaining >= MIN_TICK_TIME && Iterations < MaxSimulationIterations && IsClimbing() && HasValidData())
	{
		++Iterations;
		const float Step = GetSimulationTimeStep(Remaining, Iterations);
		Remaining -= Step;
		FSurface Surface;
		const FVector Center = UpdatedComponent->GetComponentLocation();
		const bool bFoundSurface = QuerySurface(Center, -CurrentSurface.Normal, Surface, true);
		FLedge Ledge;
		if (Acceleration.Z > KINDA_SMALL_NUMBER && FindMantle(Ledge)
			&& StartTransition(EZCCustomMovementMode::Mantling, Ledge, MantleMontage)) return;
		if (!bFoundSurface)
		{
			// 短暂漏检先停在原处，超过容错时间才下落，不能凭过期墙面无限移动
			SurfaceLostTime += Step;
			Velocity = FVector::ZeroVector;
			ClimbLocalVelocity = FVector2D::ZeroVector;
			if (SurfaceLostTime > SurfaceLossGrace)
			{
				StopClimbing(EZCClimbExitReason::LostSurface);
				StartNewPhysics(Remaining, Iterations);
				return;
			}
			continue;
		}
		SurfaceLostTime = 0;
		Surface.Normal = FMath::VInterpTo(CurrentSurface.Normal, Surface.Normal, Step, 12.0f).GetSafeNormal();
		CurrentSurface = Surface;
		FFindFloorResult Floor;
		if (Acceleration.Z < 0 && FindStandingFloor(Center, Floor))
		{
			SetMovementMode(MOVE_Walking);
			StartNewPhysics(Remaining, Iterations);
			return;
		}
		Acceleration = FVector::VectorPlaneProject(Acceleration, Surface.Normal).GetClampedToMaxSize(ClimbAcceleration);
		// 加速度和旧速度都限制在墙面切平面，随后由 Sweep 处理障碍碰撞
		Velocity = FVector::VectorPlaneProject(Velocity, Surface.Normal);
		CalcVelocity(Step, 0, false, ClimbBraking);
		Velocity = Velocity.GetClampedToMaxSize(MaxClimbSpeed);
		const FRotator Facing(0, (-Surface.Normal).Rotation().Yaw, 0);
		const FQuat Rotation = FMath::RInterpTo(UpdatedComponent->GetComponentRotation(), Facing, Step, 12.0f).Quaternion();
		FHitResult Hit;
		SafeMoveUpdatedComponent(Velocity * Step, Rotation, true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			HandleImpact(Hit, Step, Velocity * Step);
			SlideAlongSurface(Velocity * Step, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
		// 先记录切向移动，再进行吸附，防止待机吸附被计入动画速度与精力消耗
		Velocity = FVector::VectorPlaneProject((UpdatedComponent->GetComponentLocation() - Center) / Step, Surface.Normal);
		FVector Up, Right;
		BuildWallAxes(Up, Right);
		ClimbLocalVelocity = FVector2D(FVector::DotProduct(Velocity, Right), FVector::DotProduct(Velocity, Up));
		const float Error = GetWallOffset(Surface.Normal) - FVector::DotProduct(UpdatedComponent->GetComponentLocation() - Surface.Point, Surface.Normal);
		if (FMath::Abs(Error) > WallReach)
		{
			StopClimbing(EZCClimbExitReason::LostSurface);
			return;
		}
		const float Correction = FMath::Clamp(Error * FMath::Min(Step * 12.0f, 1.0f), -150.0f * Step, 150.0f * Step);
		// 贴墙修正限速并继续 Sweep，不能瞬移穿过旁边的障碍
		SafeMoveUpdatedComponent(Surface.Normal * Correction, Rotation, true, Hit);
		if (AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner))
		{
			if (ClimbLocalVelocity.SizeSquared() > FMath::Square(1.0f))
			{
				// 只对真实切向移动扣精力，顶墙不动和纯吸附不收费
				Player->ConsumeTraversalStamina(Step);
				if (!IsClimbing()) return;
			}
		}
	}
}

bool UZCCharacterMovementComponent::FindMantle(FLedge& OutLedge) const
{
	// 墙顶必须结束且后方平台可站立，先算完整胶囊终点，再核查上移和前移两段路径
	if (!IsClimbing() || !CurrentSurface.Component.IsValid()) return false;
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector Forward = -CurrentSurface.Normal.GetSafeNormal2D();
	FHitResult Upper;
	const FVector UpperStart = Center + FVector::UpVector * (HalfHeight + 8.0f);
	// 胶囊上方仍有墙时继续正常攀爬，不把任意墙面误判为顶沿
	GetWorld()->LineTraceSingleByChannel(Upper, UpperStart, UpperStart + Forward * (Radius + WallReach), ClimbChannel, MakeClimbQuery());
	if (Upper.bBlockingHit) return false;
	const FVector Above = Center + Forward * (Radius * 2.0f + SurfaceGap) + FVector::UpVector * (HalfHeight + MaxLedgeHeight);
	FHitResult Top;
	GetWorld()->LineTraceSingleByChannel(Top, Above, Above - FVector::UpVector * (MaxLedgeHeight + HalfHeight),
		Capsule->GetCollisionObjectType(), MakeClimbQuery());
	if (!Top.IsValidBlockingHit() || !IsWalkable(Top) || !IsValid(Top.GetComponent())
		|| Top.GetComponent()->Mobility != EComponentMobility::Static) return false;
	OutLedge.Target = Top.ImpactPoint + FVector::UpVector * (HalfHeight + 2.5f);
	// Via 保持原水平位置先抬升，Target 再向平台内侧前移
	OutLedge.Via = FVector(Center.X, Center.Y, OutLedge.Target.Z);
	OutLedge.Rotation = FRotator(0, Forward.Rotation().Yaw, 0);
	OutLedge.Platform = Top.GetComponent();
	OutLedge.Surface = CurrentSurface;
	FFindFloorResult Floor;
	return FindStandingFloor(OutLedge.Target, Floor) && IsPathClear(Center, OutLedge.Via)
		&& IsPathClear(OutLedge.Via, OutLedge.Target);
}

bool UZCCharacterMovementComponent::FindClimbDown(FLedge& OutLedge) const
{
	// 仅允许在静止地面主动下爬，移动中不能凭一次边缘命中切入过渡
	if (!HasValidData() || !IsMovingOnGround() || !CanAttemptClimb()
		|| !Velocity.IsNearlyZero(3.0f) || !Acceleration.IsNearlyZero(1.0f)) return false;
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Center = UpdatedComponent->GetComponentLocation();
	const FVector Forward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	FFindFloorResult Floor;
	if (!FindStandingFloor(Center, Floor) || !Floor.HitResult.GetComponent()
		|| Floor.HitResult.GetComponent()->Mobility != EComponentMobility::Static) return false;
	OutLedge.Platform = Floor.HitResult.GetComponent();
	FFindFloorResult Ahead;
	FindFloor(Center + Forward * Radius * 2.5f, Ahead, false);
	// 前方仍有可行走地面说明还没到下沿，禁止在普通平地触发
	if (Ahead.IsWalkableFloor() && Ahead.FloorDist <= MaxStepHeight) return false;
	const FVector LowCenter = Center - FVector::UpVector * (HalfHeight * 2.0f);
	FHitResult Wall;
	GetWorld()->LineTraceSingleByChannel(Wall, LowCenter + Forward * Radius * 3.0f,
		LowCenter - Forward * Radius, ClimbChannel, MakeClimbQuery());
	if (!IsClimbHit(Wall)) return false;
	OutLedge.Target = Wall.ImpactPoint + Wall.ImpactNormal * GetWallOffset(Wall.ImpactNormal);
	// 先平移到边缘外，再下降至胶囊能贴墙的位置
	OutLedge.Via = FVector(OutLedge.Target.X, OutLedge.Target.Y, Center.Z);
	OutLedge.Rotation = FRotator(0, (-Wall.ImpactNormal).Rotation().Yaw, 0);
	if (!QuerySurface(OutLedge.Target, -Wall.ImpactNormal, OutLedge.Surface, false)) return false;
	return IsPathClear(Center, OutLedge.Via) && IsPathClear(OutLedge.Via, OutLedge.Target);
}

bool UZCCharacterMovementComponent::CanClimbDownLedge() const
{
	FLedge Ledge;
	return FindClimbDown(Ledge);
}

bool UZCCharacterMovementComponent::TryStartClimbDownLedge()
{
	// 按键瞬间重新执行完整检测，不复用 CanClimbDownLedge 的旧查询结果
	FLedge Ledge;
	return FindClimbDown(Ledge) && StartTransition(EZCCustomMovementMode::ClimbDownLedge, Ledge, ClimbDownMontage);
}

bool UZCCharacterMovementComponent::StartTransition(EZCCustomMovementMode Mode, const FLedge& Ledge, UAnimMontage* Montage)
{
	// 翻上由代码控制胶囊位移，只要求可播放的姿势 Montage
	// 下爬仍由根运动驱动，因此还要求 Motion Warping 组件和根运动数据
	AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner);
	UAnimInstance* Anim = CharacterOwner && CharacterOwner->GetMesh()
		? CharacterOwner->GetMesh()->GetAnimInstance()
		: nullptr;
	bool& bLogged = Mode == EZCCustomMovementMode::Mantling ? bMissingMantleLogged : bMissingDownLogged;
	const bool bMantle = Mode == EZCCustomMovementMode::Mantling;
	if (!Player || !Anim || !Montage || (!bMantle && (!Player->MotionWarping || !Montage->HasRootMotion())))
	{
		if (!bLogged) UE_LOG(LogZCClimbing, Warning, TEXT("Climb transition %d rejected: Player=%s Anim=%s Montage=%s; climb-down requires MotionWarping and root motion"), int32(Mode), *GetNameSafe(Player), *GetNameSafe(Anim), *GetNameSafe(Montage));
		bLogged = true;
		return false;
	}
	ClearTransition();
	// 在切模式前固定本次目标和起点，后续每帧都只使用这次探测到的几何结果
	ActiveLedge = Ledge;
	CurrentSurface = Ledge.Surface;
	MantleStart = UpdatedComponent->GetComponentLocation();
	bMantleReachedVia = false;
	StopMovementImmediately();
	SetMovementMode(MOVE_Custom, uint8(Mode));
	if (bMantle)
	{
		// 保留根运动提取和根锁定来播放姿态，但不让异常根轨迹驱动胶囊
		MantleAnimInstance = Anim;
		SavedRootMotionMode = uint8(Anim->RootMotionMode.GetValue());
		Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
	// 下爬 Warp Target 对齐的是 Mesh 根位置，不能直接把胶囊中心作为动画根骨目标
	const FVector MeshOffset = CharacterOwner->GetMesh()->GetRelativeLocation();
	const auto RootTarget = [&Ledge, &MeshOffset](const FVector& Center)
	{
		return Center + Ledge.Rotation.RotateVector(MeshOffset);
	};
	if (!bMantle)
	{
		// 翻上不创建 Warp Target，下爬分别向边缘中继点和最终贴墙点对齐
		Player->MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(DownForwardTarget, RootTarget(Ledge.Via), Ledge.Rotation);
		Player->MotionWarping->AddOrUpdateWarpTargetFromLocationAndRotation(DownTarget, RootTarget(Ledge.Target), Ledge.Rotation);
	}
	ActiveTransitionMontage = Montage;
	const float Duration = Anim->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::Duration, 0, false);
	if (Duration <= 0)
	{
		UE_LOG(LogZCClimbing, Warning, TEXT("Climb Montage_Play failed: %s"), *GetNameSafe(Montage));
		FinishTransition(TransitionGeneration, true);
		return false;
	}
	UE_LOG(LogZCClimbing, Log, TEXT("Climb transition %d started: %s, duration=%.3f"), int32(Mode), *GetNameSafe(Montage), Duration);
	FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
	TransitionMontageInstance = Instance ? Instance->GetInstanceID() : INDEX_NONE;
	// Generation 隔离旧动画的异步回调，超时兜底防止 Montage 不发完成回调时卡住
	const uint32 Generation = TransitionGeneration;
	FOnMontageEnded End;
	End.BindWeakLambda(this, [this, Generation](UAnimMontage*, bool bInterrupted) { FinishTransition(Generation, bInterrupted); });
	Anim->Montage_SetEndDelegate(End, Montage);
	FOnMontageBlendingOutStarted Blend;
	Blend.BindWeakLambda(this, [this, Generation](UAnimMontage*, bool bInterrupted)
	{
		if (bInterrupted) FinishTransition(Generation, true);
	});
	Anim->Montage_SetBlendingOutDelegate(Blend, Montage);
	GetWorld()->GetTimerManager().SetTimer(TransitionTimeout, FTimerDelegate::CreateWeakLambda(this,
		[this, Generation]() { FinishTransition(Generation, true); }), Duration + 0.5f, false);
	return true;
}

void UZCCharacterMovementComponent::ClearTransition()
{
	// 先增加代次使迟到回调失效，再解绑并停止本过渡持有的 Montage
	++TransitionGeneration;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TransitionTimeout);
	UAnimMontage* Montage = ActiveTransitionMontage.Get();
	ActiveTransitionMontage.Reset();
	if (CharacterOwner && CharacterOwner->GetMesh())
	{
		if (UAnimInstance* Anim = CharacterOwner->GetMesh()->GetAnimInstance())
		{
			if (Montage)
			{
				FOnMontageEnded EmptyEnd;
				FOnMontageBlendingOutStarted EmptyBlend;
				Anim->Montage_SetEndDelegate(EmptyEnd, Montage);
				Anim->Montage_SetBlendingOutDelegate(EmptyBlend, Montage);
				if (Anim->Montage_IsPlaying(Montage)) Anim->Montage_Stop(0.05f, Montage);
			}
		}
	}
	TransitionMontageInstance = INDEX_NONE;
	if (UAnimInstance* Anim = MantleAnimInstance.Get())
	{
		// 翻上临时忽略根运动，退出时必须恢复进入前的模式
		Anim->SetRootMotionMode(static_cast<ERootMotionMode::Type>(SavedRootMotionMode));
	}
	MantleAnimInstance.Reset();
	MantleProgress = 0.0f;
	bMantleReachedVia = false;
	if (AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner))
	{
		if (Player->MotionWarping)
		{
			Player->MotionWarping->RemoveWarpTarget(MantleTarget);
			Player->MotionWarping->RemoveWarpTarget(DownForwardTarget);
			Player->MotionWarping->RemoveWarpTarget(DownTarget);
		}
	}
	ActiveLedge = FLedge();
}

void UZCCharacterMovementComponent::FinishTransition(uint32 Generation, bool bInterrupted)
{
	// 动画结束仅表示播放完成，必须再核查目标组件和实际胶囊位置
	if (Generation != TransitionGeneration || !IsClimbTraversalActive() || IsClimbing()) return;
	const bool bDown = CustomMovementMode == uint8(EZCCustomMovementMode::ClimbDownLedge);
	const bool bTargetsValid = ActiveLedge.Platform.IsValid() && ActiveLedge.Surface.Component.IsValid();
	const FVector Facing = -CurrentSurface.Normal;
	const FVector Target = ActiveLedge.Target;
	const bool bReachedMantleTarget = !bDown && bTargetsValid && MantleProgress >= 1.0f
		&& UpdatedComponent->GetComponentLocation().Equals(Target, 3.0f);
	// Montage 正常结束不等于成功上墙；未到达目标按失败退出，不能宣告完成
	if (!bDown && !bInterrupted && !bReachedMantleTarget)
	{
		UE_LOG(LogZCClimbing, Warning, TEXT("Mantle ended before reaching target: progress=%.3f position=%s target=%s"),
			MantleProgress, *UpdatedComponent->GetComponentLocation().ToCompactString(), *Target.ToCompactString());
		bInterrupted = true;
	}
	ClearTransition();
	const AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner);
	if (!Player || Player->IsDeathStarted()) { StopClimbing(EZCClimbExitReason::Death); return; }
	if (Player->IsCharacterExhausted()) { StopClimbing(EZCClimbExitReason::Exhausted); return; }
	FFindFloorResult Floor;
	FSurface Surface;
	const FVector Center = UpdatedComponent->GetComponentLocation();
	UE_LOG(LogZCClimbing, Log, TEXT("Climb transition ended: interrupted=%d position=%s"), bInterrupted, *Center.ToCompactString());
	// 站得稳才回 Walking，下爬或可恢复的中断优先尝试重新抓墙，其余进入下落
	if (bTargetsValid && FindStandingFloor(Center, Floor)) SetMovementMode(MOVE_Walking);
	else if (bTargetsValid && (bDown || bInterrupted) && QuerySurface(Center, Facing, Surface, false)
		&& IsCapsuleClear(Center) && FMath::Abs(FVector::DotProduct(Center - Surface.Point, Surface.Normal) - GetWallOffset(Surface.Normal)) <= WallReach)
	{
		EnterClimbing(Surface);
	}
	else StopClimbing(EZCClimbExitReason::Interrupted);
}

void UZCCharacterMovementComponent::PhysTransition(float DeltaTime, int32 Iterations)
{
	// 目标平台、墙面或 Montage 已失效时立刻终止，避免使用悬空引用继续运动
	if (DeltaTime < MIN_TICK_TIME || !HasValidData()) return;
	if (!ActiveLedge.Platform.IsValid() || !ActiveLedge.Surface.Component.IsValid() || !ActiveTransitionMontage.IsValid())
	{
		StopClimbing(EZCClimbExitReason::Interrupted);
		return;
	}
	if (CustomMovementMode == uint8(EZCCustomMovementMode::Mantling))
	{
		// 翻上走预检过的两段 Sweep 路径，动画仅负责可见姿势
		PhysMantle(DeltaTime);
		return;
	}
	RestorePreAdditiveRootMotionVelocity();
	// 下爬消费 Montage 根运动，Sweep 命中阻挡时按过渡中断处理
	Velocity = FVector::ZeroVector;
	ApplyRootMotionToVelocity(DeltaTime);
	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.bBlockingHit) FinishTransition(TransitionGeneration, true);
	if (AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner))
	{
		Player->ConsumeTraversalStamina(DeltaTime);
	}
}

void UZCCharacterMovementComponent::PhysMantle(float DeltaTime)
{
	// 以 Montage 实例播放位置为时钟，暂停动画时胶囊也停止前进
	UAnimInstance* Anim = MantleAnimInstance.Get();
	UAnimMontage* Montage = ActiveTransitionMontage.Get();
	if (!Anim || !Montage)
	{
		FinishTransition(TransitionGeneration, true);
		return;
	}
	// 混出时实例仍存在，但已退出 ActiveMontagesMap；Montage_GetPosition 会返回 0
	// 按启动时记录的 ID 读取同一实例，不把混出误判为动画回到开头
	const FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(TransitionMontageInstance);
	if (!Instance || Instance->Montage != Montage || !FMath::IsFinite(Instance->GetPosition()))
	{
		UE_LOG(LogZCClimbing, Warning, TEXT("Mantle playback instance missing or invalid: id=%d montage=%s"),
			TransitionMontageInstance, *GetNameSafe(Montage));
		FinishTransition(TransitionGeneration, true);
		return;
	}
	// 前 80% 完成移动；暂停时进度不变，动画跳段或回退也不能把胶囊拉回墙外
	MantleProgress = FMath::Max(MantleProgress, FMath::Clamp(Instance->GetPosition()
		/ FMath::Max(Montage->GetPlayLength() * 0.8f, KINDA_SMALL_NUMBER), 0.0f, 1.0f));
	const float Progress = MantleProgress;
	constexpr float LiftEnd = 0.55f;
	const uint32 Generation = TransitionGeneration;
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const auto MoveTo = [this, Generation](const FVector& Target)
	{
		// 每段实际位移仍由碰撞 Sweep 完成，预检不能替代运行时阻挡处理
		FHitResult Hit;
		SafeMoveUpdatedComponent(Target - UpdatedComponent->GetComponentLocation(), ActiveLedge.Rotation.Quaternion(), true, Hit);
		if (!HasValidData() || TransitionGeneration != Generation || !IsClimbTraversalActive()) return false;
		if (Hit.bBlockingHit)
		{
			UE_LOG(LogZCClimbing, Warning, TEXT("Mantle path blocked: actor=%s normal=%s position=%s target=%s"),
				*GetNameSafe(Hit.GetActor()), *Hit.ImpactNormal.ToCompactString(),
				*UpdatedComponent->GetComponentLocation().ToCompactString(), *Target.ToCompactString());
			FinishTransition(Generation, true);
			return false;
		}
		return true;
	};
	if (!bMantleReachedVia)
	{
		// 第一段只抬升胶囊，先越过墙顶高度
		const float Alpha = FMath::Clamp(Progress / LiftEnd, 0.0f, 1.0f);
		if (!MoveTo(FMath::Lerp(MantleStart, ActiveLedge.Via, FMath::SmoothStep(0.0f, 1.0f, Alpha)))) return;
		bMantleReachedVia = Progress >= LiftEnd;
	}
	if (bMantleReachedVia)
	{
		// 第二段保持高度向平台内侧移动，最终由落脚检测决定是否成功
		const float Alpha = FMath::Clamp((Progress - LiftEnd) / (1.0f - LiftEnd), 0.0f, 1.0f);
		if (!MoveTo(FMath::Lerp(ActiveLedge.Via, ActiveLedge.Target, FMath::SmoothStep(0.0f, 1.0f, Alpha)))) return;
	}
	Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	if (AZCCharBase* Player = Cast<AZCCharBase>(CharacterOwner)) Player->ConsumeTraversalStamina(DeltaTime);
}

void UZCCharacterMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTransition();
	Super::EndPlay(EndPlayReason);
}
