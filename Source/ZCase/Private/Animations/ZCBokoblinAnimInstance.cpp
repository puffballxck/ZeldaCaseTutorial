// 版权所有 Epic Games, Inc，保留所有权利

#include "Animations/ZCBokoblinAnimInstance.h"

#include "GameFramework/Pawn.h"
#include "Characters/ZCBokoblinEnemy.h"

void UZCBokoblinAnimInstance::NativeUpdateAnimation(const float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	const APawn* Pawn = TryGetPawnOwner();
	// AnimBP 可能在 Pawn 尚未完成初始化时先 Tick，缺失引用时保持安全默认值
	Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.0f;
	const AZCBokoblinEnemy* Enemy = Cast<AZCBokoblinEnemy>(Pawn);
	bParryDownPose = Enemy && Enemy->ShouldUseParryDownPose();
}
