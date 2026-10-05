// 日落回廊 · 交互提示 HUD（M7 先行，零资产：纯 C++ AHUD + 引擎字体，不建任何 WBP）。
// 设计原则"不用文字指路"——正常玩法无 HUD；仅"摸一下"的目标名提示（GetInteractPrompt）与
// 调试信息（Dysis.Where 的屏幕版，按 F3 切换）。正式 UI（开场/结局/对话）仍走 WBP + 美术。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DysisHUD.generated.h"

class UDysisInteractComponent;
class UDysisDialogueComponent;

UCLASS()
class DYSIS_API ADysisHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADysisHUD();

	/** 是否显示调试面板。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	bool bShowDebug = false;

	virtual void DrawHUD() override;

	/** 场上正在播的对话组件。 */
	UPROPERTY(Transient)
	TObjectPtr<UDysisDialogueComponent> ActiveDialogue;

	// ── 通知系统（解谜反馈/提示/机关反馈——文案表所有非对话文本走这个显示）──

	/** 显示一条通知文本（底部，比对话框高一层，自动淡出）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void ShowNotification(const FText& Text, float DurationSeconds = 4.0f);

	/** 静态快捷方式：找 HUD → ShowNotification（任何 Actor 调用一行即可）。 */
	static void Notify(UWorld* World, const TCHAR* Text, float Duration = 4.0f);

protected:
	UDysisInteractComponent* ResolveInteract();
	void DrawDialogue();
	void DrawNotification();

private:
	TWeakObjectPtr<UDysisInteractComponent> CachedInteract;

	// 通知状态
	FString NotificationText;
	float NotificationTimer = 0.0f;
	float NotificationDuration = 0.0f;
};
