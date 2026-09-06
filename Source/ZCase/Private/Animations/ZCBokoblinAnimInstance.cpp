// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animations/ZCBokoblinAnimInstance.h"

#include "GameFramework/Pawn.h"

void UZCBokoblinAnimInstance::NativeUpdateAnimation(const float DeltaTime)
{
	Super::NativeUpdateAnimation(DeltaTime);

	const APawn* Pawn = TryGetPawnOwner();
	Speed = Pawn ? Pawn->GetVelocity().Size2D() : 0.0f;
}
