#pragma once

#include "Commandlets/Commandlet.h"
#include "ZCBokoblinSetupCommandlet.generated.h"

/** 使用引擎资产接口构建可继续手动编辑的波克布林演示资产 */
UCLASS()
class UZCBokoblinSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	// 配置命令行为编辑器模式并启用控制台日志
	UZCBokoblinSetupCommandlet();
	// 执行资产、敌人实例与导航配置，Inspect 参数仅输出拳击采样
	virtual int32 Main(const FString& Params) override;
};
