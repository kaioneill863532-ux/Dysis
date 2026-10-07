// 日落回廊 · 交互提示 HUD（M7 先行，纯 C++ AHUD，不建任何 WBP；字体 = 思源宋体 FontFace，缺资产时退回引擎字体）。
// 设计原则"不用文字指路"——正常玩法无 HUD；仅"摸一下"的目标名提示（GetInteractPrompt）与
// 调试信息（Dysis.Where 的屏幕版，按 F3 切换）。正式 UI（开场/结局/对话）仍走 WBP + 美术。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DysisHUD.generated.h"

class UDysisInteractComponent;
class UDysisDialogueComponent;
class UFontFace;

DECLARE_MULTICAST_DELEGATE_OneParam(FDysisNotificationShown, const FText& /*Text*/);

UCLASS()
class DYSIS_API ADysisHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADysisHUD();

	/** 是否显示调试面板。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	bool bShowDebug = false;

	/** UI 字体（思源宋体 FontFace，tools/import_fonts.py 导入）；空 = 引擎默认字体（CJK 回退可用）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	TObjectPtr<UFontFace> FontFace;

	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

	/** 场上正在播的对话组件。 */
	UPROPERTY(Transient)
	TObjectPtr<UDysisDialogueComponent> ActiveDialogue;

	// ── 通知系统（解谜反馈/提示/机关反馈——文案表所有非对话文本走这个显示）──

	/** 显示一条通知文本（底部，比对话框高一层，自动淡出）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void ShowNotification(const FText& Text, float DurationSeconds = 4.0f);

	/** 静态快捷方式：找 HUD → ShowNotification（任何 Actor 调用一行即可）。 */
	static void Notify(UWorld* World, const TCHAR* Text, float Duration = 4.0f);

	/** 有通知显示出来时广播（音效“提示文字出现”挂在这：Audio/DysisSfxDirector）。 */
	static FDysisNotificationShown OnNotificationShown;

protected:
	UDysisInteractComponent* ResolveInteract();
	void DrawDialogue();
	void DrawNotification();

	/** 画一行 UI 文字：有 FontFace 用思源宋体（16px 基准 × Scale），否则退回引擎默认字体。 */
	void DrawUIText(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale);

private:
	TWeakObjectPtr<UDysisInteractComponent> CachedInteract;

	// 通知状态
	FString NotificationText;
	float NotificationTimer = 0.0f;
	float NotificationDuration = 0.0f;
};
