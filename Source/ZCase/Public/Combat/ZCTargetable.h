// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ZCTargetable.generated.h"

UINTERFACE(BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class ZCASE_API UZCTargetable : public UInterface
{
	GENERATED_BODY()
};

/** The information target lock needs from a candidate actor. */
class ZCASE_API IZCTargetable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	virtual bool CanBeTargetLocked() const = 0;

	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	virtual FVector GetTargetLockLocation() const = 0;
};
