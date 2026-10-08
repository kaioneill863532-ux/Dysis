// 日落回廊 · HUD（纯 C++ AHUD，不建 WBP）。
// 界面素材：使用UI/ 的按钮与游戏内图已导入 /Game/Dysis/UI/{Menu,InGame,Portraits}（tools/import_content_once.py），
// 缺资产时全部退回纯色/引擎字体，不崩。
// 结构：主菜单（开局；回车开始、设置页占位、退出）→ 游戏内（交互提示 / 对话框+立绘 / 通知 / 碎片收集）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DysisHUD.generated.h"

class UDysisInteractComponent;
class UDysisDialogueComponent;
class UFontFace;
class UTexture2D;

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

	/** 主菜单是否开着（开局 true，回车"开始游戏"后关；PrologueDirector 等它关了才开播开场对话）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|HUD")
	bool IsMenuOpen() const { return bMenuOpen; }

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

	// ── 主菜单（使用UI/主界面与设置 的图）──
	void DrawMenu();
	void DrawShards();
	/** 游戏内暂停菜单（P 键；返回游戏/设置/返回主界面）。 */
	void DrawPause();

	/** 画一行 UI 文字：有 FontFace 用思源宋体 SizePx 像素，否则退回引擎默认字体近似同大。 */
	void DrawUIText(const FString& Text, const FLinearColor& Color, float X, float Y, float SizePx, bool bShadow = true);

	/** 画一张 UI 图（缩放到 W×H；原比例可用 FitH/FitW 辅助）。 */
	void DrawUIImage(UTexture2D* Tex, float X, float Y, float W, float H, FLinearColor Tint);

	/** 懒加载 UI 图（/Game/Dysis/UI/...），按 Key 缓存；缺资产返回空（调用方跳过绘制）。 */
	UTexture2D* UITex(FName Key, const TCHAR* Path);

private:
	TWeakObjectPtr<UDysisInteractComponent> CachedInteract;

	// 通知状态
	FString NotificationText;
	float NotificationTimer = 0.0f;
	float NotificationDuration = 0.0f;

	// 主菜单状态（DrawHUD 每帧驱动）
	bool bMenuOpen = true;
	bool bSettingsOpen = false;
	int32 MenuIndex = 0;              // 0=开始游戏 1=设置 2=退出游戏
	bool bEnterWasDown = false;
	bool bUpDownWasDown = false;
	bool bBackWasDown = false;

	// 暂停菜单状态（P 键切换）
	bool bPauseOpen = false;
	int32 PauseIndex = 0;             // 0=返回游戏 1=设置 2=返回主界面
	bool bPauseWasDown = false;
	bool bPauseEnterDown = false;
	bool bPauseUpDownDown = false;

	/** UI 图缓存。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> TexCache;
};
