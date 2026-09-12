// 请在项目设置的说明页面填写版权声明

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/ZCGameplayTypes.h"
#include "ZCRuneRuntimeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCSelectedRuneChangedSignature,
	ERunes, PreviousRune,
	ERunes, CurrentRune);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FZCActiveRuneChangedSignature,
	ERunes, PreviousRune,
	ERunes, CurrentRune);

/**
 * 持有符文选择和激活状态
 *
 * ActiveRune 始终为 R_EMAX 或当前选中的符文，调用方
 * 通过 SelectRune、ToggleSelectedRune 和 CancelAll 表达意图，而不是
 * 维护多个独立的激活布尔值
 */
UCLASS(ClassGroup=(ZCase), meta=(BlueprintSpawnableComponent))
class ZCASE_API UZCRuneRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UZCRuneRuntimeComponent();

	/** 返回当前选择，R_EMAX 表示没有有效符文 */
	UFUNCTION(BlueprintPure, Category="ZCase|Runes")
	ERunes GetSelectedRune() const { return SelectedRune; }

	/** 返回当前激活状态，始终为 R_EMAX 或 SelectedRune */
	UFUNCTION(BlueprintPure, Category="ZCase|Runes")
	ERunes GetActiveRune() const { return ActiveRune; }

	/** 选择符文，选择不同符文前会先取消当前激活符文 */
	UFUNCTION(BlueprintCallable, Category="ZCase|Runes")
	bool SelectRune(ERunes NewRune);

	/** 激活当前选中的符文，若它已激活则改为取消 */
	UFUNCTION(BlueprintCallable, Category="ZCase|Runes")
	bool ToggleSelectedRune();

	/** 取消当前激活符文，但不改变当前选择 */
	UFUNCTION(BlueprintCallable, Category="ZCase|Runes")
	bool CancelAll();

	UPROPERTY(BlueprintAssignable, Category="ZCase|Runes")
	FZCSelectedRuneChangedSignature OnSelectedRuneChanged;

	UPROPERTY(BlueprintAssignable, Category="ZCase|Runes")
	FZCActiveRuneChangedSignature OnActiveRuneChanged;

private:
	/** 更新激活符文并广播一次状态变化 */
	void SetActiveRune(ERunes NewActiveRune);

	/** 当前选中的符文，切换选择前会先取消旧激活状态 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ZCase|Runes", meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERunes> SelectedRune = ERunes::R_EMAX;

	/** 当前实际生效的符文，未激活时为 R_EMAX */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ZCase|Runes", meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERunes> ActiveRune = ERunes::R_EMAX;
};
