// 版权所有 Epic Games, Inc，保留所有权利

#include "Modules/ModuleManager.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "ZCAnimationRootFixer.h"

#define LOCTEXT_NAMESPACE "FZCaseEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogZCaseEditor, Log, All);

namespace
{
// 显示批处理结果，并根据完成状态设置通知图标
void ShowNotification(const FText& Message, const SNotificationItem::ECompletionState State)
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 6.0f;
	Info.bUseSuccessFailIcons = true;
	if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
	{
		Item->SetCompletionState(State);
	}
}

// 从内容浏览器当前选择中筛选并加载动画序列
TArray<UAnimSequence*> GetSelectedAnimationSequences()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	TArray<UAnimSequence*> Animations;
	Animations.Reserve(SelectedAssets.Num());
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (UAnimSequence* Animation = Cast<UAnimSequence>(AssetData.GetAsset()))
		{
			Animations.Add(Animation);
		}
	}
	return Animations;
}
}

// 注册编辑器动画修正菜单并汇总批量处理结果
class FZCaseEditorModule final : public IModuleInterface
{
public:
	// 等待工具菜单初始化后注册自定义入口
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FZCaseEditorModule::RegisterMenus));
	}

	// 卸载模块时移除回调和菜单，避免残留对象引用
	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	// 在工具菜单中添加所选动画根轨道修正操作
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		FToolMenuSection& Section = ToolsMenu->AddSection(
			TEXT("ZCaseAnimationTools"),
			LOCTEXT("ZCaseAnimationToolsSection", "ZCase Animation Tools"));

		Section.AddMenuEntry(
			TEXT("ZCaseFixSelectedAnimationRoots"),
			LOCTEXT("FixSelectedAnimationRootsLabel", "Fix Selected Link Animation Roots"),
			LOCTEXT("FixSelectedAnimationRootsTooltip", "Set the first root key target to Scale 100 and Rotation (Pitch 0, Yaw 90, Roll 0) while preserving animation deltas. Does not reimport or save assets."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FZCaseEditorModule::FixSelectedAnimationRoots)));
	}

	// 确认选择后逐个修正动画，支持取消并保留未保存状态
	void FixSelectedAnimationRoots()
	{
		const TArray<UAnimSequence*> Animations = GetSelectedAnimationSequences();
		if (Animations.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT(
				"NoAnimationsSelected",
				"Select one or more Animation Sequence assets in the Content Browser, then run this tool again."));
			return;
		}

		const FText Confirmation = FText::Format(
			LOCTEXT(
				"ConfirmAnimationRootFix",
				"Apply the Link root correction to {0} selected Animation Sequence(s)?\n\n"
				"First root key target:\n"
				"  Scale: 100, 100, 100\n"
				"  Rotation: Pitch 0, Yaw 90, Roll 0\n\n"
				"The tool preserves translation and relative rotation/scale changes. It does not reimport or save assets. You can inspect the result and use Undo before saving."),
			FText::AsNumber(Animations.Num()));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Confirmation) != EAppReturnType::Yes)
		{
			return;
		}

		const FZCAnimationRootFixSettings Settings;
		int32 AppliedCount = 0;
		int32 AlreadyCorrectCount = 0;
		int32 FailedCount = 0;

		FScopedSlowTask SlowTask(
			Animations.Num(),
			LOCTEXT("FixingAnimationRoots", "Fixing selected animation root tracks..."),
			true);
		SlowTask.MakeDialog(true);

		for (UAnimSequence* Animation : Animations)
		{
			if (SlowTask.ShouldCancel())
			{
				break;
			}

			SlowTask.EnterProgressFrame(1.0f, FText::FromString(Animation->GetName()));
			FString Details;
			const EZCAnimationRootFixResult Result = FZCAnimationRootFixer::ApplyToAnimation(Animation, Settings, Details);
			switch (Result)
			{
			case EZCAnimationRootFixResult::Applied:
				++AppliedCount;
				break;
			case EZCAnimationRootFixResult::AlreadyCorrect:
				++AlreadyCorrectCount;
				break;
			default:
				++FailedCount;
				break;
			}

			if (Result == EZCAnimationRootFixResult::Applied || Result == EZCAnimationRootFixResult::AlreadyCorrect)
			{
				UE_LOG(
					LogZCaseEditor,
					Display,
					TEXT("Animation root fix [%s] %s: %s"),
					FZCAnimationRootFixer::LexToString(Result),
					*GetPathNameSafe(Animation),
					*Details);
			}
			else
			{
				UE_LOG(
					LogZCaseEditor,
					Warning,
					TEXT("Animation root fix [%s] %s: %s"),
					FZCAnimationRootFixer::LexToString(Result),
					*GetPathNameSafe(Animation),
					*Details);
			}
		}

		const FText Summary = FText::Format(
			LOCTEXT(
				"AnimationRootFixSummary",
				"Animation root fix finished: {0} changed, {1} already correct, {2} failed. Changed assets are unsaved; inspect them before saving. See Output Log for details."),
			FText::AsNumber(AppliedCount),
			FText::AsNumber(AlreadyCorrectCount),
			FText::AsNumber(FailedCount));
		ShowNotification(
			Summary,
			FailedCount == 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
};

IMPLEMENT_MODULE(FZCaseEditorModule, ZCaseEditor)

#undef LOCTEXT_NAMESPACE
