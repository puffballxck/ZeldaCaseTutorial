// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "ZCRunesSelections.generated.h"

class AZCCharBase;
/** 旧版符文选择控件，负责把 UI 选择转发给角色 */
UCLASS()
class ZCASE_API UZCRunesSelections : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 当前被控件驱动的角色引用 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	AZCCharBase* PlayerRef;

	/** 将 UI 选择的符文交给角色处理 */
	UFUNCTION(BlueprintCallable,category="Test")
	void SelectRuneType(TEnumAsByte<ERunes> RuneType);
	
	
};
