// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "ZCBokoblinAnimInstance.generated.h"

/** Minimal locomotion data consumed by the Bokoblin AnimBP BlendSpace. */
UCLASS()
class ZCASE_API UZCBokoblinAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ZCase|Bokoblin|Animation")
	float Speed = 0.0f;
};
