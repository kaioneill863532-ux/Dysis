// 日落回廊 · HUD（纯 C++ AHUD，不建 WBP）。按 Art/UI/source 里的示意图拼：
//   主界面：背景是实时场景（屋顶日落），右上 logo，“开始游戏 / 退出游戏”，选中项带选择标。
//   设置页：SETTING 标题，“回到游戏 / 回到主界面”。游戏里按 P 或 Esc（左上角的钟表图标）打开，打开时游戏暂停。
//   局内：左上钟表图标；右上树枝，挂着已经拿到的碎片；中上提示条（机关反馈、提示、互动提示）；
//         底部通栏文本框 + 名牌，右下是说话人的立绘（对话时才出现）。
// 素材由 Art/UI/ue_import_ui.py 导入到 /Game/Dysis/UI/{Menu,InGame,Portraits}；缺资产时退回纯色和默认字体，不崩。
// 位置是在示意图（6544×3746）里用图像匹配量出来的，见 DysisHUD.cpp 开头的表。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Fonts/SlateFontInfo.h"
#include "DysisHUD.generated.h"

class UDysisInteractComponent;
class UDysisDialogueComponent;
class UFont;
class UFontFace;
class UTexture2D;
class ACameraActor;
struct FStandaloneCompositeFont;

DECLARE_MULTICAST_DELEGATE_OneParam(FDysisNotificationShown, const FText& /*Text*/);
DECLARE_MULTICAST_DELEGATE(FDysisHudEvent);

UENUM()
enum class EDysisScreen : uint8
{
	MainMenu,
	Playing,
	Settings,
};

UCLASS()
class DYSIS_API ADysisHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADysisHUD();

	/** 是否显示调试面板（F3 切换）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	bool bShowDebug = false;

	/** 开局先进主界面。关掉就直接进游戏（测试用；控制台 Dysis.Start 也能跳过）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	bool bStartInMainMenu = true;

	/** UI 字体（正文；默认阿里巴巴普惠体 45 Light）。读不出来时退回引擎自带的中文字体。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	TObjectPtr<UFontFace> FontFace;

	UPROPERTY(EditAnywhere, Category = "Dysis|HUD")
	TObjectPtr<UFontFace> FontFaceBold;

	/** 主界面背景的机位（屋顶上，朝着浑天仪）和那一刻的时间。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面")
	FVector MenuCameraLocation = FVector(-462.0, -1269.0, 3330.0);

	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面")
	FRotator MenuCameraRotation = FRotator(12.0, 70.0, 0.0);

	/** 主界面开始时的时刻（时角，度）：傍晚，太阳还在天上，几秒后日落（示意图是日落那一刻）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面")
	float MenuTimeH = 56.0f;

	/** 主界面上时间自己往前走（傍晚 → 日落 → 夜 → 天亮 → 白天）：按白天的流速转一整圈要多少秒。0 = 不走。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面", meta = (ClampMin = "0.0"))
	float MenuDaySeconds = 144.0f;

	/** 太阳落山以后流速乘上这个数（这里夜比昼长，让夜走快一点）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面", meta = (ClampMin = "0.1"))
	float MenuNightSpeed = 2.0f;

	/** 主界面镜头用固定曝光（EV）：不让引擎自动把夜晚提亮成白天的样子。后一个数是入夜以后再加的量
	 *  （负数 = 夜里暗下来：0.6 − 1.0 = −0.4，比游戏里的夜（ADysisCharacter::NightExposureBias = −1.0）稍亮一点，看得清殿顶）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面")
	float MenuExposureBias = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Dysis|HUD|主界面")
	float MenuNightExposureBoost = -1.0f;


	virtual void DrawHUD() override;
	virtual void BeginPlay() override;

	/** 主界面是否开着（开局 true，“开始游戏”后关；开场导演等它关了才开播）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|HUD")
	bool IsMenuOpen() const { return Screen == EDysisScreen::MainMenu; }

	UFUNCTION(BlueprintPure, Category = "Dysis|HUD")
	bool IsSettingsOpen() const { return Screen == EDysisScreen::Settings; }

	/** 主界面“开始游戏”。bInstant = 不要黑场过渡（测试用）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void StartGame(bool bInstant = false);

	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void CloseSettings();

	/** 回到主界面：重新载入关卡，一切从头。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void ReturnToMainMenu();

	/** 场上正在播的对话组件（HUD 按它的当前句画文本框、名牌、立绘）。 */
	UPROPERTY(Transient)
	TObjectPtr<UDysisDialogueComponent> ActiveDialogue;

	// ── 提示条：解谜反馈、提示、机关反馈（文案表里所有非对话文本）──

	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void ShowNotification(const FText& Text, float DurationSeconds = 4.0f);

	/** 静态快捷方式：找 HUD → ShowNotification。 */
	static void Notify(UWorld* World, const TCHAR* Text, float Duration = 4.0f);

	/** 有通知显示出来时广播（音效“提示文字出现”挂在这）。 */
	static FDysisNotificationShown OnNotificationShown;

	/** 主界面点了“开始游戏”、画面切回玩家的那一刻。 */
	static FDysisHudEvent OnGameStarted;

	/** 关卡标题：屏幕中间的大字，淡入、停一会儿、淡出。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void ShowTitle(const FString& Main, const FString& Sub, float HoldSeconds = 2.4f);

	/** 屏幕中间出一次关卡名：一张罗马数字的图（1–5 = I–V，素材在 /Game/Dysis/UI/InGame/LevelI…LevelV），没有小标题。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void ShowLevelNumeral(int32 Number, float HoldSeconds = 2.4f);

	/** 黑场：在 Seconds 里变到 TargetAlpha（回档、开始游戏用）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void FadeTo(float TargetAlpha, float Seconds);

	UFUNCTION(BlueprintPure, Category = "Dysis|HUD")
	float GetFadeAlpha() const { return FadeAlpha; }

	/** 树枝上挂哪几片碎片：0 = 太阳，1 = 彩虹，2 = 月亮（存档里已经拿到的也会显示）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|HUD")
	void SetShard(int32 Index, bool bHave);

	static ADysisHUD* Get(const UObject* WorldContext);

protected:
	UDysisInteractComponent* ResolveInteract();

	void DrawMainMenu(float RealDt);
	void DrawSettings(float RealDt);
	void DrawInGame(float Dt);
	void DrawDialogue();
	void DrawHintBar(float Dt);
	/** 能互动的东西旁边浮出来的提示：一个圆角方块里写着 E，旁边一行字（“转动雕像”之类）。和提示条分开。 */
	void DrawInteractPrompt(float Dt);
	UTexture2D* KeycapTexture();
	void DrawShards();
	void DrawTitle(float Dt);
	void DrawDebug();

	/** 主界面的机位和时间；每帧调（Pawn 可能晚生成，也会抢回视角）。 */
	void HoldMenuView(float RealDt);
	void ApplyInputForScreen();

	// ── 画图、画字 ──
	/** 懒加载 UI 图（/Game/Dysis/UI/...），按 Key 缓存；缺资产返回空。 */
	UTexture2D* UITex(FName Key, const TCHAR* Path);
	/** 把一张图整张画到 (X, Y, W, H)。 */
	void DrawUIImage(UTexture2D* Tex, float X, float Y, float W, float H, const FLinearColor& Tint = FLinearColor::White);
	void DrawUIImageSwung(UTexture2D* Tex, float X, float Y, float W, float H, float AngleDeg, const FVector2D& Pivot, const FLinearColor& Tint = FLinearColor::White);
	/** 字体：Px = 设计稿 1080 高时的像素字号，按画布高度缩放。 */
	FSlateFontInfo MakeFont(float Px, bool bBold = false) const;
	FVector2D MeasureText(const FString& Text, const FSlateFontInfo& Font) const;
	/** 画一行字。AlignX：0 = X 是左边，0.5 = X 是中线，1 = X 是右边。 */
	void DrawUIText(const FString& Text, const FLinearColor& Color, float X, float Y, float Px, float AlignX = 0.0f, bool bBold = false, bool bShadow = true);
	/** 按宽度折行（中文按字断）。 */
	TArray<FString> WrapText(const FString& Text, const FSlateFontInfo& Font, float MaxWidth) const;
	/** 画一段折行的字，返回画了几行。AlignX 同上；Y 是第一行的顶。 */
	int32 DrawUIParagraph(const FString& Text, const FLinearColor& Color, float X, float Y, float Px, float MaxWidth, float AlignX = 0.0f, float LineGap = 1.35f);

	/** 鼠标在不在这个矩形里（画布像素）。 */
	bool MouseIn(float X, float Y, float W, float H) const;

private:
	TWeakObjectPtr<UDysisInteractComponent> CachedInteract;
	TSharedPtr<FStandaloneCompositeFont> CompositeFont;

	EDysisScreen Screen = EDysisScreen::MainMenu;
	bool bScreenInputApplied = false;
	EDysisScreen InputAppliedFor = EDysisScreen::Playing;

	// 主界面 / 设置页
	int32 MenuIndex = 0;        // 0 = 开始游戏，1 = 退出游戏
	int32 SettingsIndex = 0;    // 0 = 回到游戏，1 = 回到主界面
	float SelectorX = 0.0f;     // 选择标的平滑位置（设计稿像素）
	int32 StartPhase = 0;       // 0 = 没在开始；1 = 正在黑下去；2 = 正在亮起来
	float MenuTurn = 0.0f;      // 主界面上天已经转过的角度（度，0–360）
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MenuCamera;

	// 鼠标（每帧在 DrawHUD 开头取一次）
	bool bMouseValid = false;
	FVector2D MousePos = FVector2D::ZeroVector;
	bool bClick = false;

	// 提示条
	FString NotificationText;
	float NotificationTimer = 0.0f;
	float HintAlpha = 0.0f;
	FString HintShownText;

	// 互动提示（浮在互动点旁边）
	float PromptAlpha = 0.0f;
	FString PromptText;
	FVector2D PromptPos = FVector2D::ZeroVector;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Keycap;

	// 对话框
	float DialogueAlpha = 0.0f;
	FString LastSpeaker;

	// 关卡标题
	FString TitleMain, TitleSub;
	int32 TitleNumeral = 0;   // > 0 时标题是一张罗马数字的图，不是字
	mutable float FontTracking = 0.0f;   // 接下来画的字的字距（几分之几个字宽）；只有对话框用，用完归零
	float TitleT = -1.0f, TitleHold = 2.4f;

	// 碎片（位：1 太阳、2 彩虹、4 月亮）
	uint8 ShardMask = 0;

	// 黑场
	float FadeAlpha = 0.0f, FadeTarget = 0.0f, FadeRate = 0.0f;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> TexCache;

	/** 只为过引擎的一道检查：UE 5.8 的画布文字不带 UFont 就整个不画（哪怕给了 Slate 字体）。 */
	UPROPERTY(Transient)
	TObjectPtr<UFont> CanvasFont;
};