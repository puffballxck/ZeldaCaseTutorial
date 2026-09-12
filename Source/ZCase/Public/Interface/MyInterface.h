// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MyInterface.generated.h"

// 此类无需修改
UINTERFACE(MinimalAPI)
class UMyInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ZCASE_API IMyInterface
{
	GENERATED_BODY()

	// 在此类中添加接口函数，派生类通过继承该接口实现这些函数
public:
	virtual void NextAction() = 0;
};
