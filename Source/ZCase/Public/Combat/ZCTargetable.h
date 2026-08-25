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

/** 目标锁定系统从候选对象读取的最小信息契约。 */
class ZCASE_API IZCTargetable
{
	GENERATED_BODY()

public:
	/** 返回对象当前是否允许被目标锁定。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	virtual bool CanBeTargetLocked() const = 0;

	/** 返回用于锁定镜头或准星的世界空间位置。 */
	UFUNCTION(BlueprintCallable, Category = "ZCase|Target Lock")
	virtual FVector GetTargetLockLocation() const = 0;
};
