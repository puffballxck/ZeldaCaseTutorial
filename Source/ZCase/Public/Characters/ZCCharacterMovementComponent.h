#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "ZCCharacterMovementComponent.generated.h"

class UAnimMontage;
class UAnimInstance;

/** 玩家攀爬的检测、碰撞移动及上下沿生命周期，不改变敌人的移动实现 */
UCLASS()
class ZCASE_API UZCCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:
	/** 仅表示正在墙面移动，不包含翻上和下爬过渡 */
	UFUNCTION(BlueprintPure, Category="Climbing")
	bool IsClimbing() const;
	/** 三种自定义移动模式的总入口，供角色玩法互斥使用 */
	UFUNCTION(BlueprintPure, Category="Climbing")
	bool IsClimbTraversalActive() const;
	/** 翻上 Montage 混出前提前切换基础姿势，物理模式仍保持到过渡结束 */
	bool ShouldUseClimbBasePose(float BlendLeadTime) const;
	/** 只查询地面边缘机会，不启动过渡 */
	UFUNCTION(BlueprintPure, Category="Climbing")
	bool CanClimbDownLedge() const;
	/** 按键时重新检测边缘，避免使用先前已失效的位置 */
	UFUNCTION(BlueprintCallable, Category="Climbing")
	bool TryStartClimbDownLedge();
	/** 主动松手、耗尽、受击等退出统一从这里进入 */
	UFUNCTION(BlueprintCallable, Category="Climbing")
	void StopClimbing(EZCClimbExitReason Reason);
	/** 墙面右和上的实际切向速度，供 BlendSpace 使用，单位为 cm/s */
	UFUNCTION(BlueprintPure, Category="Climbing")
	FVector2D GetClimbLocalVelocity() const { return ClimbLocalVelocity; }
	/** 角色将二维输入投影到墙面，CMC 仍使用标准 AddMovementInput 消费路径 */
	FVector GetClimbInputDirection(const FVector2D& Input) const;
	void ResetClimbInput();

	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void PhysFlying(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void PhysicsRotation(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Climbing", meta=(ClampMin="1"))
	float MaxClimbSpeed = 100.0f;
	UPROPERTY(EditAnywhere, Category="Climbing", meta=(ClampMin="1"))
	float ClimbAcceleration = 350.0f;
	UPROPERTY(EditAnywhere, Category="Climbing", meta=(ClampMin="1"))
	float ClimbBraking = 500.0f;
	/** 地面连续朝有效墙面移动才累计进入时间，空中贴墙可直接进入 */
	UPROPERTY(EditAnywhere, Category="Climbing", meta=(ClampMin="0.01"))
	float GroundEntryDuration = 0.20f;
	/** 空中只有胶囊接近墙面时才抓墙，不使用地面的进入计时 */
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="0"))
	float AirContactDistance = 6.0f;
	UPROPERTY(EditAnywhere, Category="Climbing", meta=(ClampMin="0"))
	float RegrabDelay = 0.50f;
	/** 球形墙面探测距离，不等于空中允许隔空抓墙的距离 */
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="1"))
	float WallReach = 35.0f;
	/** 胶囊与墙面保留的间隙，参与目标贴墙距离计算 */
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="0"))
	float SurfaceGap = 3.0f;
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="0", ClampMax="89"))
	float MinWallAngle = 60.0f;
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="1", ClampMax="90"))
	float MaxWallAngle = 90.0f;
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="0", ClampMax="0.2"))
	float SurfaceLossGrace = 0.08f;
	UPROPERTY(EditAnywhere, Category="Climbing|Detection", meta=(ClampMin="1"))
	float MaxLedgeHeight = 100.0f;
	/** 翻上只用 Montage 播姿势，胶囊沿检查过的两段路径由 PhysMantle 推进 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Climbing|Animation")
	TObjectPtr<UAnimMontage> MantleMontage;
	/** 下爬由根运动驱动，需要 Motion Warping 和对应的两个 Warp Target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Climbing|Animation")
	TObjectPtr<UAnimMontage> ClimbDownMontage;
	UPROPERTY(EditAnywhere, Category="Climbing|Debug")
	bool bDrawClimbDebug = false;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 本帧墙面采样，弱引用防止静态组件被移除后继续使用旧表面 */
	struct FSurface
	{
		FVector Point = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		TWeakObjectPtr<UPrimitiveComponent> Component;
	};
	/** 上下沿过渡的中继点、最终胶囊中心和目标组件 */
	struct FLedge
	{
		FVector Via = FVector::ZeroVector;
		FVector Target = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FSurface Surface;
		TWeakObjectPtr<UPrimitiveComponent> Platform;
	};
	/** 上中下三个球形 Sweep，可选按当前速度加预测采样，只合并方向一致的命中 */
	bool QuerySurface(const FVector& Center, const FVector& Facing, FSurface& OutSurface, bool bPredict) const;
	bool IsClimbHit(const FHitResult& Hit) const;
	bool IsCapsuleClear(const FVector& Center) const;
	bool IsPathClear(const FVector& Start, const FVector& End) const;
	bool FindStandingFloor(const FVector& Center, FFindFloorResult& OutFloor) const;
	FCollisionQueryParams MakeClimbQuery() const;
	float GetWallOffset(const FVector& Normal) const;
	bool FindMantle(FLedge& OutLedge) const;
	bool FindClimbDown(FLedge& OutLedge) const;
	/** 先确认资产可播，再缓存目标、切模式并绑定仅属于本次过渡的回调 */
	bool StartTransition(EZCCustomMovementMode Mode, const FLedge& Ledge, UAnimMontage* Montage);
	/** 动画结束或中断后按实际落点决定 Walking、Climbing 或 Falling */
	void FinishTransition(uint32 Generation, bool bInterrupted);
	/** 失效旧回调并恢复动画根运动设置，同时移除 Warp Target */
	void ClearTransition();
	void PhysClimbing(float DeltaTime, int32 Iterations);
	void PhysTransition(float DeltaTime, int32 Iterations);
	void PhysMantle(float DeltaTime);
	void EnterClimbing(const FSurface& Surface);
	void BuildWallAxes(FVector& Up, FVector& Right) const;
	bool CanAttemptClimb() const;

	/** 最近一次有效墙面，离开攀爬模式时清空 */
	FSurface CurrentSurface;
	FLedge ActiveLedge;
	/** 仅在攀爬物理中有效，模式切换时清零，吸附修正不计入此速度 */
	FVector2D ClimbLocalVelocity = FVector2D::ZeroVector;
	float EntryTime = 0.0f;
	float SurfaceLostTime = 0.0f;
	double RegrabAllowedAt = 0.0;
	/** 主动松手后地面需先松开朝墙输入，空中需先离开原墙或落地 */
	bool bNeedsInputRelease = false;
	bool bNeedsWallSeparation = false;
	FVector ReleasedNormal = FVector::ZeroVector;
	bool bSavedOrientRotation = true;
	bool bSavedControllerDesiredRotation = false;
	/** 每次清理递增，旧 Montage 回调不能结束后来启动的新过渡 */
	uint32 TransitionGeneration = 0;
	int32 TransitionMontageInstance = INDEX_NONE;
	TWeakObjectPtr<UAnimMontage> ActiveTransitionMontage;
	FTimerHandle TransitionTimeout;
	/** 翻上按 Montage 播放进度驱动两段胶囊路径，避免根轨迹直接推胶囊 */
	FVector MantleStart = FVector::ZeroVector;
	float MantleProgress = 0.0f;
	bool bMantleReachedVia = false;
	TWeakObjectPtr<UAnimInstance> MantleAnimInstance;
	uint8 SavedRootMotionMode = 0;
	bool bMissingMantleLogged = false;
	bool bMissingDownLogged = false;
};
