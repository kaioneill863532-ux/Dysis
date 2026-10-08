// 日落回廊 · HUD 实现：素材图（Menu/InGame/Portraits）+ 思源宋体；缺资产逐处退回纯色/默认字体。
#include "DysisHUD.h"
#include "DysisDialogueComponent.h"
#include "Interaction/DysisInteractComponent.h"
#include "Mechanisms/DysisInteractable.h"
#include "Sky/DysisTimeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Save/DysisSaveGame.h"
#include "Save/DysisSaveSubsystem.h"

ADysisHUD::ADysisHUD()
{
	PrimaryActorTick.bCanEverTick = false;   // DrawHUD 每帧由引擎调，不需要自己的 Tick
}

void ADysisHUD::BeginPlay()
{
	Super::BeginPlay();
	// 思源宋体（tools/import_fonts.py 导入的 FontFace）。资产不在（旧分支/被删）时静默退回默认字体。
	if (!FontFace)
	{
		FontFace = LoadObject<UFontFace>(this, TEXT("/Game/Dysis/UI/Fonts/SourceHanSerifSC-Regular.SourceHanSerifSC-Regular"));
	}
}

// ───────────────────────── 基础绘制 ─────────────────────────

void ADysisHUD::DrawUIText(const FString& Text, const FLinearColor& Color, float X, float Y, float SizePx, bool bShadow)
{
	if (!Canvas || Text.IsEmpty()) return;
	if (FontFace)
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), FSlateFontInfo(FontFace, FMath::Max(4.0f, SizePx)), Color);
		if (bShadow) Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), FVector2D(1.5f, 1.5f));
		Canvas->DrawItem(Item);
	}
	else
	{
		DrawText(Text, Color, X, Y, nullptr, SizePx / 18.0f, bShadow);
	}
}

void ADysisHUD::DrawUIImage(UTexture2D* Tex, float X, float Y, float W, float H, FLinearColor Tint)
{
	if (!Tex || !Canvas) return;
	DrawTexture(Tex, X, Y, W, H, 0.0f, 0.0f, float(Tex->GetSizeX()), float(Tex->GetSizeY()), Tint);
}

UTexture2D* ADysisHUD::UITex(FName Key, const TCHAR* Path)
{
	if (TObjectPtr<UTexture2D>* Found = TexCache.Find(Key))
	{
		return Found->Get();
	}
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, Path);
	TexCache.Add(Key, Tex);   // 空也记（缺资产别每帧打 LoadObject）
	return Tex;
}

// ───────────────────────── 通知 ─────────────────────────

void ADysisHUD::ShowNotification(const FText& Text, float DurationSeconds)
{
	NotificationText = Text.ToString();
	NotificationTimer = DurationSeconds;
	NotificationDuration = DurationSeconds;
}

void ADysisHUD::Notify(UWorld* World, const TCHAR* Text, float Duration)
{
	if (!World) return;
	if (APlayerController* PC = World->GetFirstPlayerController())
		if (ADysisHUD* HUD = Cast<ADysisHUD>(PC->GetHUD()))
			HUD->ShowNotification(FText::FromString(Text), Duration);
}

void ADysisHUD::DrawNotification()
{
	if (NotificationTimer <= 0.0f || !Canvas || NotificationText.IsEmpty()) return;

	// 通知：反馈与提示图打底，文字画图上——文案表所有非对话文本的显示口。
	const float Alpha = (NotificationTimer > NotificationDuration - 0.5f)
		? (NotificationDuration - NotificationTimer) / 0.5f   // 前 0.5s 淡入
		: (NotificationTimer < 1.0f ? NotificationTimer / 1.0f : 1.0f);  // 后 1s 淡出

	const float BoxH = 52.0f;
	const float BoxW = FMath::Max(NotificationText.Len() * 24.0f + 80.0f, 320.0f);
	const float X = (Canvas->SizeX - BoxW) * 0.5f;
	const float Y = Canvas->SizeY - 160.0f;

	if (UTexture2D* Notify = UITex("Notify", TEXT("/Game/Dysis/UI/InGame/Notify.Notify")))
	{
		DrawUIImage(Notify, X, Y, BoxW, BoxH, FLinearColor(1, 1, 1, Alpha));
	}
	else
	{
		DrawRect(FLinearColor(0, 0, 0.05f, Alpha * 0.5f), X, Y, BoxW, BoxH);
	}
	DrawUIText(NotificationText, FLinearColor(0.95f, 0.92f, 0.85f, Alpha), Canvas->SizeX * 0.5f - NotificationText.Len() * 11.0f, Y + 14.0f, 22.0f);
}

// ───────────────────────── 对话 ─────────────────────────

void ADysisHUD::DrawDialogue()
{
	if (!ActiveDialogue || !ActiveDialogue->IsPlaying() || !Canvas) return;
	const int32 Idx = ActiveDialogue->CurrentLine;
	if (!ActiveDialogue->Lines.IsValidIndex(Idx)) return;

	const FString Line = ActiveDialogue->Lines[Idx].ToString();
	const float BoxH = 130.0f;
	const float BoxW = FMath::Min(Canvas->SizeX - 120.0f, 1180.0f);
	const float BoxX = (Canvas->SizeX - BoxW) * 0.5f;
	const float BoxY = Canvas->SizeY - BoxH - 40.0f;

	// 立绘：行首人名 → Portraits/{Helios,Dysis,Selene,Iris}.png（对话框左侧，半身）。
	FName PortraitKey = NAME_None;
	const TCHAR* PortraitPath = nullptr;
	if (Line.StartsWith(TEXT("赫利俄斯")))      { PortraitKey = "Helios"; PortraitPath = TEXT("/Game/Dysis/UI/Portraits/Helios.Helios"); }
	else if (Line.StartsWith(TEXT("狄西斯")))   { PortraitKey = "Dysis";  PortraitPath = TEXT("/Game/Dysis/UI/Portraits/Dysis.Dysis"); }
	else if (Line.StartsWith(TEXT("塞勒涅")))   { PortraitKey = "Selene"; PortraitPath = TEXT("/Game/Dysis/UI/Portraits/Selene.Selene"); }
	else if (Line.StartsWith(TEXT("伊莉丝")))   { PortraitKey = "Iris";   PortraitPath = TEXT("/Game/Dysis/UI/Portraits/Iris.Iris"); }
	if (PortraitPath)
	{
		const float PortraitH = 300.0f;
		if (UTexture2D* P = UITex(PortraitKey, PortraitPath))
		{
			const float PW = float(P->GetSizeX()) / float(P->GetSizeY()) * PortraitH;
			DrawUIImage(P, BoxX - PW * 0.72f, Canvas->SizeY - PortraitH - 8.0f, PW, PortraitH, FLinearColor(1, 1, 1, 0.94f));
		}
	}

	// 文本框：文本框图打底（缺图退深色条），台词 + 句点进度 + [E] 提示。
	if (UTexture2D* Box = UITex("TextBox", TEXT("/Game/Dysis/UI/InGame/TextBox.TextBox")))
	{
		DrawUIImage(Box, BoxX, BoxY, BoxW, BoxH, FLinearColor(1, 1, 1, 0.9f));
	}
	else
	{
		DrawRect(FLinearColor(0, 0, 0.04f, 0.62f), BoxX, BoxY, BoxW, BoxH);
	}
	DrawUIText(Line, FLinearColor(0.95f, 0.94f, 0.9f, 0.97f), BoxX + 36.0f, BoxY + 26.0f, 26.0f);
	FString Dots;
	for (int32 i = 0; i < ActiveDialogue->Lines.Num(); ++i) Dots += (i == Idx ? TEXT("●") : TEXT("○"));
	DrawUIText(Dots, FLinearColor(0.75f, 0.78f, 0.92f, 0.85f), BoxX + 36.0f, BoxY + BoxH - 30.0f, 16.0f);
	DrawUIText(TEXT("[E] 下一句"), FLinearColor(0.72f, 0.74f, 0.82f, 0.85f), BoxX + BoxW - 140.0f, BoxY + BoxH - 34.0f, 18.0f);
}

// ───────────────────────── 主菜单 ─────────────────────────

namespace
{
	// 菜单项文案（与 使用UI/主界面与设置 的按钮图一一对应；图在 → 画图，图缺 → 画字）。
	struct FMenuItem { const TCHAR* Label; const TCHAR* TexPath; };
	const FMenuItem GMenuItems[] = {
		{ TEXT("开始游戏"), TEXT("/Game/Dysis/UI/Menu/StartGame.StartGame") },
		{ TEXT("设 置"),    TEXT("/Game/Dysis/UI/Menu/Settings.Settings") },
		{ TEXT("退出游戏"), TEXT("/Game/Dysis/UI/Menu/QuitGame.QuitGame") },
	};
}

void ADysisHUD::DrawMenu()
{
	if (!Canvas) return;
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;

	// 底：深夜蓝（正式版换主界面背景图/3D 场景）。
	DrawRect(FLinearColor(0.015f, 0.02f, 0.055f, 0.985f), 0, 0, W, H);
	DrawRect(FLinearColor(0.35f, 0.22f, 0.08f, 0.25f), 0, H * 0.12f, W, 3.0f);   // 标题下的金线
	DrawRect(FLinearColor(0.35f, 0.22f, 0.08f, 0.25f), 0, H * 0.12f + 1.0f, W * 0.35f, 1.0f);

	DrawUIText(TEXT("狄西斯的日落回廊"), FLinearColor(0.93f, 0.87f, 0.72f, 1.0f), W * 0.5f - 8.0f * 28.0f, H * 0.13f, 56.0f);
	DrawUIText(TEXT("DYSIS AND THE CELESTIAL CLOISTER"), FLinearColor(0.55f, 0.52f, 0.45f, 0.9f), W * 0.5f - 32.0f * 4.5f, H * 0.13f + 72.0f, 18.0f);

	if (bSettingsOpen)
	{
		// 设置页 v1：设置示意图占位（美术出正式设置页后换）；返回主界面按钮 + 设置选择标。
		if (UTexture2D* S = UITex("SettingsMock", TEXT("/Game/Dysis/UI/References/Mockups/Settings.Settings")))
		{
			const float SW = W * 0.72f;
			const float SH = SW / FMath::Max(1.0f, float(S->GetSizeX()) / float(S->GetSizeY()));
			DrawUIImage(S, (W - SW) * 0.5f, (H - SH) * 0.42f, SW, SH, FLinearColor(1, 1, 1, 0.96f));
		}
		else
		{
			DrawUIText(TEXT("设 置"), FLinearColor(0.9f, 0.87f, 0.8f, 1.0f), W * 0.5f - 3.0f * 18.0f, H * 0.3f, 36.0f);
		}
		// 返回主界面（按钮图；缺图退文字）。Enter 或 Backspace 都能返回。
		if (UTexture2D* Back = UITex("BackToMenu", TEXT("/Game/Dysis/UI/Menu/BackToMenu.BackToMenu")))
		{
			const float BH = 56.0f;
			const float BW = BH * FMath::Max(0.5f, float(Back->GetSizeX()) / FMath::Max(1, Back->GetSizeY()));
			const float BX = W * 0.5f - BW * 0.5f;
			DrawUIImage(Back, BX, H * 0.86f, BW, BH, FLinearColor(1, 1, 1, 1));
			if (UTexture2D* SelS = UITex("SelectorSettings", TEXT("/Game/Dysis/UI/Menu/SelectorSettings.SelectorSettings")))
			{
				DrawUIImage(SelS, BX - 58.0f, H * 0.86f + BH * 0.5f - 22.0f, 44.0f, 44.0f, FLinearColor(1, 1, 1, 1));
			}
		}
		else
		{
			DrawUIText(TEXT("[Backspace] 返回主菜单"), FLinearColor(0.6f, 0.6f, 0.65f, 0.9f), W * 0.5f - 8.0f * 9.0f, H * 0.88f, 18.0f);
		}
		return;
	}

	// 三个菜单项：有按钮图画图（选中放大 + 选择标），缺图画字。
	const float ItemY0 = H * 0.42f;
	const float ItemGap = 92.0f;
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bSel = (i == MenuIndex);
		UTexture2D* Tex = UITex(FName(*FString::Printf(TEXT("Menu%d", i))), GMenuItems[i].TexPath);
		float ItemW = 260.0f, ItemH = 64.0f;
		if (Tex)
		{
			const float Ratio = float(Tex->GetSizeX()) / FMath::Max(1, Tex->GetSizeY());
			ItemH = bSel ? 72.0f : 60.0f;
			ItemW = ItemH * Ratio;
		}
		const float IX = W * 0.5f - ItemW * 0.5f;
		const float IY = ItemY0 + i * ItemGap;
		if (Tex)
		{
			DrawUIImage(Tex, IX, IY, ItemW, ItemH, bSel ? FLinearColor(1, 1, 1, 1) : FLinearColor(0.62f, 0.62f, 0.62f, 0.9f));
		}
		else
		{
			const FString Label = GMenuItems[i].Label;
			DrawUIText(Label, bSel ? FLinearColor(0.95f, 0.85f, 0.6f, 1.0f) : FLinearColor(0.6f, 0.6f, 0.62f, 0.9f),
				W * 0.5f - Label.Len() * 16.0f, IY, 32.0f);
		}
		if (bSel)
		{
			if (UTexture2D* Sel = UITex("SelectorMain", TEXT("/Game/Dysis/UI/Menu/SelectorMain.SelectorMain")))
			{
				DrawUIImage(Sel, IX - 64.0f, IY + ItemH * 0.5f - 24.0f, 48.0f, 48.0f, FLinearColor(1, 1, 1, 1));
			}
			else
			{
				DrawRect(FLinearColor(0.85f, 0.7f, 0.35f, 0.95f), IX - 40.0f, IY + ItemH * 0.5f - 2.0f, 18.0f, 4.0f);
			}
		}
	}

	DrawUIText(TEXT("[↑↓] 选择    [Enter] 确认"), FLinearColor(0.5f, 0.5f, 0.55f, 0.85f), W * 0.5f - 9.0f * 10.0f, H * 0.86f, 20.0f);
}

// ───────────────────────── 碎片收集（右上角）─────────────────────────

void ADysisHUD::DrawShards()
{
	if (!Canvas || !GetWorld()) return;
	UDysisSaveSubsystem* Save = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UDysisSaveSubsystem>()
		: nullptr;
	if (!Save || !Save->GetCurrent()) return;

	struct FShard { EDysisNiche Niche; const TCHAR* TexPath; };
	const FShard Shards[] = {
		{ EDysisNiche::Sun,     TEXT("/Game/Dysis/UI/InGame/SunShard.SunShard") },
		{ EDysisNiche::Rainbow, TEXT("/Game/Dysis/UI/InGame/RainbowShard.RainbowShard") },
		{ EDysisNiche::Moon,    TEXT("/Game/Dysis/UI/InGame/MoonShard.MoonShard") },
	};

	const float IconH = 52.0f;
	float X = Canvas->SizeX - 60.0f;
	// 收藏品树枝打底（碎片挂在枝上；缺图就只画碎片）。
	if (UTexture2D* VineTex = UITex("Vine", TEXT("/Game/Dysis/UI/InGame/Vine.Vine")))
	{
		DrawUIImage(VineTex, Canvas->SizeX - 320.0f, 12.0f, 300.0f, 76.0f, FLinearColor(1, 1, 1, 0.9f));
	}
	for (const FShard& S : Shards)
	{
		if (!Save->GetCurrent()->HasNiche(S.Niche)) continue;
		if (UTexture2D* T = UITex(FName(*FString::Printf(TEXT("Shard%d", int32(S.Niche)))), S.TexPath))
		{
			const float IW = float(T->GetSizeX()) / FMath::Max(1, T->GetSizeY()) * IconH;
			X -= IW;
			DrawUIImage(T, X, 28.0f, IW, IconH, FLinearColor(1, 1, 1, 0.97f));
			X -= 22.0f;
		}
	}
}

// ───────────────────────── 暂停菜单（P 键）─────────────────────────

void ADysisHUD::DrawPause()
{
	if (!Canvas) return;
	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;
	APlayerController* PC = GetOwningPlayerController();

	DrawRect(FLinearColor(0.0f, 0.0f, 0.03f, 0.62f), 0, 0, W, H);
	DrawUIText(TEXT("暂 停"), FLinearColor(0.93f, 0.87f, 0.72f, 1.0f), W * 0.5f - 2.0f * 20.0f, H * 0.20f, 40.0f);

	struct FPauseItem { const TCHAR* Label; const TCHAR* TexPath; };
	const FPauseItem Items[] = {
		{ TEXT("返回游戏"),   TEXT("/Game/Dysis/UI/Menu/BackToGame.BackToGame") },
		{ TEXT("设 置"),      TEXT("/Game/Dysis/UI/Menu/Settings.Settings") },
		{ TEXT("返回主界面"), TEXT("/Game/Dysis/UI/Menu/BackToMenu.BackToMenu") },
	};

	const bool bE = PC && PC->IsInputKeyDown(EKeys::Enter);
	const bool bUD = PC && (PC->IsInputKeyDown(EKeys::Up) || PC->IsInputKeyDown(EKeys::Down));
	const bool bEnterEdge = bE && !bPauseEnterDown;
	const bool bUDEdge = bUD && !bPauseUpDownDown;
	if (bUDEdge) PauseIndex = (PauseIndex + (PC->IsInputKeyDown(EKeys::Down) ? 1 : 2)) % 3;
	if (bEnterEdge)
	{
		if (PauseIndex == 0)      bPauseOpen = false;                        // 返回游戏
		else if (PauseIndex == 1) { bPauseOpen = false; bMenuOpen = true; bSettingsOpen = true; }  // 设置（借主菜单的设置页）
		else                      { bPauseOpen = false; bMenuOpen = true; }  // 返回主界面
	}

	const float ItemY0 = H * 0.36f;
	const float ItemGap = 96.0f;
	for (int32 i = 0; i < 3; ++i)
	{
		const bool bSel = (i == PauseIndex);
		UTexture2D* Tex = UITex(FName(*FString::Printf(TEXT("Pause%d", i))), Items[i].TexPath);
		float ItemH = 64.0f, ItemW = 240.0f;
		if (Tex)
		{
			ItemH = bSel ? 70.0f : 58.0f;
			ItemW = ItemH * FMath::Max(0.5f, float(Tex->GetSizeX()) / FMath::Max(1, Tex->GetSizeY()));
		}
		const float IX = W * 0.5f - ItemW * 0.5f;
		const float IY = ItemY0 + i * ItemGap;
		if (Tex)
		{
			DrawUIImage(Tex, IX, IY, ItemW, ItemH, bSel ? FLinearColor(1, 1, 1, 1) : FLinearColor(0.6f, 0.6f, 0.6f, 0.9f));
		}
		else
		{
			const FString Label = Items[i].Label;
			DrawUIText(Label, bSel ? FLinearColor(0.95f, 0.85f, 0.6f, 1.0f) : FLinearColor(0.6f, 0.6f, 0.62f, 0.9f),
				W * 0.5f - Label.Len() * 15.0f, IY, 30.0f);
		}
		if (bSel && Tex)
		{
			if (UTexture2D* Sel = UITex("SelectorMain", TEXT("/Game/Dysis/UI/Menu/SelectorMain.SelectorMain")))
			{
				DrawUIImage(Sel, IX - 62.0f, IY + ItemH * 0.5f - 22.0f, 44.0f, 44.0f, FLinearColor(1, 1, 1, 1));
			}
		}
	}

	DrawUIText(TEXT("[↑↓] 选择   [Enter] 确认   [P] 返回游戏"), FLinearColor(0.55f, 0.55f, 0.6f, 0.85f), W * 0.5f - 12.0f * 9.0f, H * 0.86f, 20.0f);
	bPauseEnterDown = bE;
	bPauseUpDownDown = bUD;
}

// ───────────────────────── 每帧 ─────────────────────────

void ADysisHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	// 主菜单开着：只画菜单（不读游戏内 UI；输入边沿在这里处理）。
	APlayerController* PC = GetOwningPlayerController();
	const bool bEnter = PC && PC->IsInputKeyDown(EKeys::Enter);
	const bool bUpDown = PC && (PC->IsInputKeyDown(EKeys::Up) || PC->IsInputKeyDown(EKeys::Down));
	const bool bBack = PC && PC->IsInputKeyDown(EKeys::BackSpace);
	if (bMenuOpen)
	{
		const bool bEnterPressed = bEnter && !bEnterWasDown;
		const bool bUpDownPressed = bUpDown && !bUpDownWasDown;
		const bool bBackPressed = bBack && !bBackWasDown;
		if (bSettingsOpen)
		{
			if (bBackPressed || bEnterPressed) bSettingsOpen = false;
		}
		else
		{
			if (bUpDownPressed) MenuIndex = (MenuIndex + (PC->IsInputKeyDown(EKeys::Down) ? 1 : 2)) % 3;
			if (bEnterPressed)
			{
				if (MenuIndex == 0)      bMenuOpen = false;   // 开始游戏（世界本来就在跑，关掉菜单即入局）
				else if (MenuIndex == 1) bSettingsOpen = true;
				else if (PC && GetWorld())
				{
					UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
					return;
				}
			}
		}
		DrawMenu();
		bEnterWasDown = bEnter;
		bUpDownWasDown = bUpDown;
		bBackWasDown = bBack;
		return;
	}
	bEnterWasDown = bEnter;
	bUpDownWasDown = bUpDown;
	bBackWasDown = bBack;

	// ── 暂停菜单（P 键切换；开着时不画游戏内 UI）──
	const bool bPDown = PC && PC->IsInputKeyDown(EKeys::P);
	if (bPDown && !bPauseWasDown)
	{
		bPauseOpen = !bPauseOpen;
		PauseIndex = 0;
	}
	bPauseWasDown = bPDown;
	if (bPauseOpen)
	{
		DrawPause();
		return;
	}

	// ── 交互提示：屏幕中心下方（"指尖光点"的文字替身）──
	if (UDysisInteractComponent* Interact = ResolveInteract())
	{
		AActor* Target = nullptr;
		if (Interact->CanInteractNow(Target))
		{
			if (const IDysisInteractable* Interactable = Cast<IDysisInteractable>(Target))
			{
				const FText Prompt = Interactable->GetInteractPrompt();
				if (!Prompt.IsEmpty())
				{
					const FString Line = TEXT("[E] ") + Prompt.ToString();
					DrawUIText(Line, FLinearColor(1.0f, 0.95f, 0.8f, 0.95f), Canvas->SizeX * 0.5f - Line.Len() * 8.5f, Canvas->SizeY * 0.5f + 48.0f, 22.0f);
				}
			}
		}
	}

	// ── 对白 / 通知 / 碎片 ──
	DrawDialogue();
	DrawNotification();
	if (NotificationTimer > 0.0f) NotificationTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	DrawShards();

	// ── 调试面板（Dysis.Where 的屏幕版）──
	if (bShowDebug && PC)
	{
		if (const APawn* Pawn = PC->GetPawn())
			if (const UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>())
			{
				const float Clock = 12.0f + Time->H / 15.0f;
				const FString Info = FString::Printf(TEXT("zone=%s  H=%.2f  clock=%02d:%02d  %s"),
					*Time->Zone, Time->H, int32(Clock) % 24, int32(Clock * 60.0f) % 60,
					Time->bNight ? TEXT("night") : TEXT("day"));
				DrawUIText(Info, FLinearColor(0.4f, 1.0f, 0.4f, 0.9f), 24.0f, 24.0f, 18.0f);
			}
	}
}
