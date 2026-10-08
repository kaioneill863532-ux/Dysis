#include "DysisHUD.h"
#include "DysisCopy.h"
#include "DysisDialogueComponent.h"
#include "Interaction/DysisInteractComponent.h"
#include "Mechanisms/DysisInteractable.h"
#include "Save/DysisSaveGame.h"
#include "Save/DysisSaveSubsystem.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

FDysisNotificationShown ADysisHUD::OnNotificationShown;
FDysisHudEvent ADysisHUD::OnGameStarted;

namespace
{
	// ───── 局内界面：元素在示意图（6544×3746）里的位置和大小，图像匹配量出来的 ─────
	constexpr float MockW = 6544.0f, MockH = 3746.0f;
	struct FBox4 { float X, Y, W, H; };
	constexpr FBox4 ClockBox   = { 198.0f, 83.0f, 493.0f, 485.0f };        // 左上：钟表图标（= 设置）
	constexpr FBox4 VineBox    = { 4442.0f, 55.0f, 2102.0f, 458.0f };      // 右上：树枝
	constexpr FBox4 SunBox     = { 4678.0f, 186.0f, 349.0f, 348.0f };      // 树枝上挂的三片碎片
	constexpr FBox4 RainbowBox = { 5307.0f, 158.0f, 556.0f, 484.0f };
	constexpr FBox4 MoonBox    = { 6003.0f, 175.0f, 481.0f, 424.0f };
	constexpr FBox4 HintBox    = { 1024.0f, 538.0f, 4155.0f, 477.0f };     // 中上：提示条（面板在图里 x 13–4130、y 63–410）
	constexpr FBox4 TextBox    = { 0.0f, 2515.0f, 6544.0f, 1231.0f };      // 底部：文本框（主面板 y 290–1130，名牌 x 4270–5830、y 20–365）
	// 立绘都是右下对齐；这里是原图大小
	struct FPortrait { const TCHAR* Key; const TCHAR* Path; float W, H; };
	const FPortrait PortraitDysis  = { TEXT("PorDysis"),  TEXT("/Game/Dysis/UI/Portraits/Dysis.Dysis"),   1606.0f, 2376.0f };
	const FPortrait PortraitIris   = { TEXT("PorIris"),   TEXT("/Game/Dysis/UI/Portraits/Iris.Iris"),     2395.0f, 2540.0f };
	const FPortrait PortraitSelene = { TEXT("PorSelene"), TEXT("/Game/Dysis/UI/Portraits/Selene.Selene"), 1937.0f, 2380.0f };
	const FPortrait PortraitHelios = { TEXT("PorHelios"), TEXT("/Game/Dysis/UI/Portraits/Helios.Helios"), 2280.0f, 2493.0f };

	// ───── 主界面 / 设置页：按钮图都是 1920×1080 的整屏图层，内容已经摆在该在的地方 ─────
	constexpr float MenuW = 1920.0f, MenuH = 1080.0f;
	constexpr FBox4 LogoBox = { 1092.0f, 31.0f, 1126.0f, 668.0f };         // logo（透明底 1455×863）在主界面示意图里的位置
	constexpr float MainRowY[2] = { 556.0f, 654.0f };                      // “开始游戏”“退出游戏”两行的点击范围（顶）
	constexpr float MainRowX = 1200.0f, MainRowW = 450.0f, MainRowH = 78.0f, MainRowStep = 98.0f;
	constexpr float SetRowY[2] = { 562.0f, 678.0f };                       // “回到游戏”“回到主界面”
	constexpr float SetRowX = 700.0f, SetRowW = 520.0f, SetRowH = 86.0f, SetRowStep = 116.0f;

	const FLinearColor Cream(0.96f, 0.90f, 0.76f, 1.0f);

	const FPortrait* PortraitFor(const FString& Speaker)
	{
		if (Speaker.Contains(TEXT("狄西斯"))) return &PortraitDysis;
		if (Speaker.Contains(TEXT("伊莉丝")) || Speaker.Contains(TEXT("伊里斯"))) return &PortraitIris;
		if (Speaker.Contains(TEXT("塞勒涅"))) return &PortraitSelene;
		if (Speaker.Contains(TEXT("赫利俄斯"))) return &PortraitHelios;
		return nullptr;
	}

	float Approach(float V, float Target, float Rate, float Dt)
	{
		return V < Target ? FMath::Min(Target, V + Rate * Dt) : FMath::Max(Target, V - Rate * Dt);
	}

	// 控制台：Dysis.Start——跳过主界面直接进游戏（测试用）。
	FAutoConsoleCommandWithWorld GDysisStart(
		TEXT("Dysis.Start"),
		TEXT("跳过主界面，直接开始游戏"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (ADysisHUD* H = ADysisHUD::Get(World)) H->StartGame(true);
		}));
}

ADysisHUD::ADysisHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

ADysisHUD* ADysisHUD::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ADysisHUD>(PC->GetHUD()) : nullptr;
}

void ADysisHUD::BeginPlay()
{
	Super::BeginPlay();
	// 界面字体：阿里巴巴普惠体 3.0（正文 45 Light，名牌和标题 65 Medium），由 Art/UI/ue_import_fonts.py 导入
	if (!FontFace) FontFace = LoadObject<UFontFace>(this, TEXT("/Game/Dysis/UI/Fonts/PuHuiTi_Light.PuHuiTi_Light"));
	if (!FontFaceBold) FontFaceBold = LoadObject<UFontFace>(this, TEXT("/Game/Dysis/UI/Fonts/PuHuiTi_Medium.PuHuiTi_Medium"));
	const FString EngineCjkFont = FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf");
	if (FontFace)
	{
		// FontFace 不能直接当“字体对象”交给 Slate（那样画不出字）：要包成一个复合字体。
		CompositeFont = MakeShared<FStandaloneCompositeFont>();
		FTypefaceEntry Regular(TEXT("Regular"));
		Regular.Font = FFontData(FontFace);
		CompositeFont->DefaultTypeface.Fonts.Add(Regular);
		FTypefaceEntry Bold(TEXT("Bold"));
		Bold.Font = FFontData(FontFaceBold ? FontFaceBold.Get() : FontFace.Get());
		CompositeFont->DefaultTypeface.Fonts.Add(Bold);
		// 字体文件读不出来（比如文件不完整）时量出来的行高是 0：字会画歪，干脆整套换成引擎自带的中文字体。
		if (FSlateApplication::IsInitialized()
			&& FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->GetMaxCharacterHeight(FSlateFontInfo(CompositeFont, 24.0f, TEXT("Regular"))) <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("DysisHUD：UI 字体 %s 读不出来（字体文件可能不完整），改用引擎自带的中文字体。"), *FontFace->GetPathName());
			CompositeFont = MakeShared<FStandaloneCompositeFont>();
			CompositeFont->DefaultTypeface.AppendFont(TEXT("Regular"), EngineCjkFont, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			CompositeFont->DefaultTypeface.AppendFont(TEXT("Bold"), EngineCjkFont, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		}
		// 兜底：主字体里没有的字改用引擎自带的中文字体，至少看得见。
		CompositeFont->FallbackTypeface.Typeface.AppendFont(TEXT("Regular"), EngineCjkFont, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}
	// UE 5.8：FCanvasTextItem 只给 Slate 字体、不给 UFont 的话，引擎当成“没有字”直接不画。
	// 所以带一个空的“运行时字体”过这道检查；真正用的字体还是上面的 Slate 字体。
	CanvasFont = NewObject<UFont>(this);
	CanvasFont->FontCacheType = EFontCacheType::Runtime;
	Screen = bStartInMainMenu ? EDysisScreen::MainMenu : EDysisScreen::Playing;
	bScreenInputApplied = false;
}

// ───────────────────────── 画图、画字 ─────────────────────────

UTexture2D* ADysisHUD::UITex(FName Key, const TCHAR* Path)
{
	if (TObjectPtr<UTexture2D>* Found = TexCache.Find(Key)) return Found->Get();
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, Path);
	TexCache.Add(Key, Tex);   // 空也记（缺资产别每帧去找）
	return Tex;
}

void ADysisHUD::DrawUIImage(UTexture2D* Tex, float X, float Y, float W, float H, const FLinearColor& Tint)
{
	if (!Tex || !Canvas || Tint.A <= 0.002f) return;
	DrawTexture(Tex, X, Y, W, H, 0.0f, 0.0f, 1.0f, 1.0f, Tint);   // 贴图坐标是 0–1 的比例
}

FSlateFontInfo ADysisHUD::MakeFont(float Px, bool bBold) const
{
	const float CanvasPx = Px * (Canvas ? Canvas->SizeY / 1080.0f : 1.0f);
	const float Points = FMath::Max(4.0f, CanvasPx * 72.0f / 96.0f);   // Slate 的字号是磅
	if (CompositeFont.IsValid()) return FSlateFontInfo(CompositeFont, Points, bBold ? TEXT("Bold") : TEXT("Regular"));
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Points);
}

FVector2D ADysisHUD::MeasureText(const FString& Text, const FSlateFontInfo& Font) const
{
	if (!FSlateApplication::IsInitialized() || Text.IsEmpty()) return FVector2D::ZeroVector;
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const auto Size = Measure->Measure(Text, Font);
	return FVector2D(Size.X, Size.Y);
}

void ADysisHUD::DrawUIText(const FString& Text, const FLinearColor& Color, float X, float Y, float Px, float AlignX, bool bBold, bool bShadow)
{
	if (!Canvas || Text.IsEmpty() || Color.A <= 0.002f) return;
	const FSlateFontInfo Font = MakeFont(Px, bBold);
	if (AlignX != 0.0f) X -= float(MeasureText(Text, Font).X) * AlignX;
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Font = CanvasFont;
	if (bShadow) Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f * Color.A), FVector2D(1.5f, 1.5f));
	Canvas->DrawItem(Item);
}

TArray<FString> ADysisHUD::WrapText(const FString& Text, const FSlateFontInfo& Font, float MaxWidth) const
{
	TArray<FString> Lines;
	FString Line;
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR Ch = Text[i];
		if (Ch == TEXT('\n')) { Lines.Add(Line); Line.Reset(); continue; }
		Line.AppendChar(Ch);
		if (MeasureText(Line, Font).X > MaxWidth && Line.Len() > 1)
		{
			// 标点不放在行首：超宽的这个字如果是标点，就让它留在这一行
			const bool bPunct = FString(TEXT("，。、；：？！”’）》…—")).Contains(FString::Chr(Ch));
			if (bPunct) { Lines.Add(Line); Line.Reset(); }
			else { Line.LeftChopInline(1); Lines.Add(Line); Line = FString::Chr(Ch); }
		}
	}
	if (!Line.IsEmpty()) Lines.Add(Line);
	return Lines;
}

int32 ADysisHUD::DrawUIParagraph(const FString& Text, const FLinearColor& Color, float X, float Y, float Px, float MaxWidth, float AlignX, float LineGap)
{
	const FSlateFontInfo Font = MakeFont(Px);
	const TArray<FString> Lines = WrapText(Text, Font, MaxWidth);
	const float LineH = Px * (Canvas->SizeY / 1080.0f) * LineGap;
	for (int32 i = 0; i < Lines.Num(); ++i) DrawUIText(Lines[i], Color, X, Y + i * LineH, Px, AlignX);
	return Lines.Num();
}

bool ADysisHUD::MouseIn(float X, float Y, float W, float H) const
{
	return bMouseValid && MousePos.X >= X && MousePos.X <= X + W && MousePos.Y >= Y && MousePos.Y <= Y + H;
}

// ───────────────────────── 提示条、标题、黑场 ─────────────────────────

void ADysisHUD::ShowNotification(const FText& Text, float DurationSeconds)
{
	NotificationText = Text.ToString();
	NotificationTimer = FMath::Max(0.5f, DurationSeconds);
	OnNotificationShown.Broadcast(Text);
}

void ADysisHUD::Notify(UWorld* World, const TCHAR* Text, float Duration)
{
	if (ADysisHUD* H = Get(World)) H->ShowNotification(FText::FromString(Text), Duration);
}

void ADysisHUD::ShowTitle(const FString& Main, const FString& Sub, float HoldSeconds)
{
	TitleMain = Main; TitleSub = Sub; TitleHold = HoldSeconds; TitleT = 0.0f;
}

void ADysisHUD::FadeTo(float TargetAlpha, float Seconds)
{
	FadeTarget = FMath::Clamp(TargetAlpha, 0.0f, 1.0f);
	FadeRate = Seconds > KINDA_SMALL_NUMBER ? 1.0f / Seconds : 1000.0f;
}

UDysisInteractComponent* ADysisHUD::ResolveInteract()
{
	UDysisInteractComponent* Interact = CachedInteract.Get();
	if (!Interact)
	{
		if (APlayerController* PC = GetOwningPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Interact = Pawn->FindComponentByClass<UDysisInteractComponent>())) CachedInteract = Interact;
	}
	return Interact;
}

// ───────────────────────── 切换界面 ─────────────────────────

void ADysisHUD::ApplyInputForScreen()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || (bScreenInputApplied && InputAppliedFor == Screen)) return;
	bScreenInputApplied = true; InputAppliedFor = Screen;
	const bool bUi = Screen != EDysisScreen::Playing;
	PC->bShowMouseCursor = bUi;
	PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput();
	if (bUi)
	{
		PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true);
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ADysisHUD::HoldMenuView(float RealDt)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !GetWorld()) return;
	if (!MenuCamera)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		MenuCamera = GetWorld()->SpawnActor<ACameraActor>(MenuCameraLocation, MenuCameraRotation, Params);
		if (MenuCamera && MenuCamera->GetCameraComponent()) MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	if (MenuCamera && PC->GetViewTarget() != MenuCamera) PC->SetViewTarget(MenuCamera);

	// 主界面的天自己往前走（H 每 360 是一天）；太阳落山以后走快一点
	const float SunAlt = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(UDysisSkyLibrary::DysisSunDir(MenuTimeH + MenuTurn).Z), -1.0f, 1.0f)));
	const float Rate = MenuDaySeconds > KINDA_SMALL_NUMBER ? 360.0f / MenuDaySeconds : 0.0f;
	MenuTurn = FMath::Fmod(MenuTurn + Rate * (SunAlt < -1.0f ? MenuNightSpeed : 1.0f) * RealDt, 360.0f);
	if (APawn* Pawn = PC->GetPawn())
		if (UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>())
			Time->SetForcedTime(MenuTimeH + MenuTurn);

	// 固定曝光：自动曝光会把月夜提亮得和白天一样，主界面上就看不出昼夜了
	if (UCameraComponent* Cam = MenuCamera ? MenuCamera->GetCameraComponent() : nullptr)
	{
		const float DuskT = FMath::Clamp((SunAlt - 1.5f) / (-9.0f - 1.5f), 0.0f, 1.0f);
		const float Dusk = DuskT * DuskT * (3.0f - 2.0f * DuskT);   // 灰盒的 smoothstep(1.5, -9, alt)
		FPostProcessSettings& PP = Cam->PostProcessSettings;
		PP.bOverride_AutoExposureMethod = true;
		PP.AutoExposureMethod = AEM_Manual;
		PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		PP.AutoExposureApplyPhysicalCameraExposure = false;
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = MenuExposureBias + MenuNightExposureBoost * Dusk;
	}
}

void ADysisHUD::StartGame(bool bInstant)
{
	if (Screen != EDysisScreen::MainMenu || StartPhase != 0) return;
	if (bInstant)
	{
		StartPhase = 1; FadeAlpha = 1.0f; FadeTarget = 1.0f;   // 下一帧走完切换
		return;
	}
	StartPhase = 1;
	FadeTo(1.0f, 0.4f);
}

void ADysisHUD::OpenSettings()
{
	if (Screen != EDysisScreen::Playing) return;
	Screen = EDysisScreen::Settings; SettingsIndex = 0; SelectorX = 0.0f;
	UGameplayStatics::SetGamePaused(this, true);
}

void ADysisHUD::CloseSettings()
{
	if (Screen != EDysisScreen::Settings) return;
	Screen = EDysisScreen::Playing;
	UGameplayStatics::SetGamePaused(this, false);
}

void ADysisHUD::ReturnToMainMenu()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

// ───────────────────────── 主界面 ─────────────────────────

void ADysisHUD::DrawMainMenu(float RealDt)
{
	APlayerController* PC = GetOwningPlayerController();
	const float S = Canvas->SizeY / MenuH;
	const float OX = Canvas->SizeX - MenuW * S;   // 内容靠右：比 16:9 宽的时候右边对齐
	auto Layer = [&](UTexture2D* T, float DY, float A) { DrawUIImage(T, OX, DY * S, MenuW * S, MenuH * S, FLinearColor(1, 1, 1, A)); };

	UTexture2D* Logo = UITex("Logo", TEXT("/Game/Dysis/UI/Menu/Logo.Logo"));
	UTexture2D* Start = UITex("StartGame", TEXT("/Game/Dysis/UI/Menu/StartGame.StartGame"));
	UTexture2D* Quit = UITex("QuitGame", TEXT("/Game/Dysis/UI/Menu/QuitGame.QuitGame"));
	UTexture2D* Selector = UITex("SelectorMain", TEXT("/Game/Dysis/UI/Menu/SelectorMain.SelectorMain"));

	// 输入：上下键 / W S 换行，回车 / 空格 / E 确认；鼠标移上去选中，点一下确认
	const bool bBusy = StartPhase != 0;
	bool bConfirm = false;
	if (PC && !bBusy)
	{
		if (PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::Down) || PC->WasInputKeyJustPressed(EKeys::S))
			MenuIndex = 1 - MenuIndex;
		for (int32 i = 0; i < 2; ++i)
			if (MouseIn(OX + MainRowX * S, MainRowY[i] * S, MainRowW * S, MainRowH * S)) { MenuIndex = i; if (bClick) bConfirm = true; }
		if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::E)) bConfirm = true;
	}

	if (Logo) DrawUIImage(Logo, OX + LogoBox.X * S, LogoBox.Y * S, LogoBox.W * S, LogoBox.H * S);
	else DrawUIText(TEXT("狄西斯的日落回廊"), Cream, OX + 1430.0f * S, 300.0f * S, 56.0f, 0.5f, true);

	SelectorX = Approach(SelectorX, MenuIndex * MainRowStep, 900.0f, RealDt);
	if (Start && Quit)
	{
		Layer(Start, 0.0f, MenuIndex == 0 ? 1.0f : 0.72f);
		Layer(Quit, 0.0f, MenuIndex == 1 ? 1.0f : 0.72f);
		Layer(Selector, SelectorX, 1.0f);
	}
	else
	{
		DrawUIText(TEXT("开始游戏"), MenuIndex == 0 ? Cream : Cream.CopyWithNewOpacity(0.6f), OX + 1430.0f * S, 575.0f * S, 34.0f, 0.5f);
		DrawUIText(TEXT("退出游戏"), MenuIndex == 1 ? Cream : Cream.CopyWithNewOpacity(0.6f), OX + 1430.0f * S, 673.0f * S, 34.0f, 0.5f);
	}

	if (bConfirm)
	{
		if (MenuIndex == 0) StartGame(false);
		else if (PC) UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

// ───────────────────────── 设置页（游戏里按 P / Esc） ─────────────────────────

void ADysisHUD::DrawSettings(float RealDt)
{
	APlayerController* PC = GetOwningPlayerController();
	const float S = Canvas->SizeY / MenuH;
	const float OX = (Canvas->SizeX - MenuW * S) * 0.5f;   // 内容居中
	DrawRect(FLinearColor(0.05f, 0.02f, 0.01f, 0.55f), 0, 0, Canvas->SizeX, Canvas->SizeY);
	auto Layer = [&](UTexture2D* T, float DY, float A) { DrawUIImage(T, OX, DY * S, MenuW * S, MenuH * S, FLinearColor(1, 1, 1, A)); };

	UTexture2D* Title = UITex("Settings", TEXT("/Game/Dysis/UI/Menu/Settings.Settings"));
	UTexture2D* Resume = UITex("BackToGame", TEXT("/Game/Dysis/UI/Menu/BackToGame.BackToGame"));
	UTexture2D* ToMenu = UITex("BackToMenu", TEXT("/Game/Dysis/UI/Menu/BackToMenu.BackToMenu"));
	UTexture2D* Selector = UITex("SelectorSettings", TEXT("/Game/Dysis/UI/Menu/SelectorSettings.SelectorSettings"));

	bool bConfirm = false, bResume = false;
	if (PC)
	{
		if (PC->WasInputKeyJustPressed(EKeys::Up) || PC->WasInputKeyJustPressed(EKeys::W) || PC->WasInputKeyJustPressed(EKeys::Down) || PC->WasInputKeyJustPressed(EKeys::S))
			SettingsIndex = 1 - SettingsIndex;
		for (int32 i = 0; i < 2; ++i)
			if (MouseIn(OX + SetRowX * S, SetRowY[i] * S, SetRowW * S, SetRowH * S)) { SettingsIndex = i; if (bClick) bConfirm = true; }
		if (PC->WasInputKeyJustPressed(EKeys::Enter) || PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::E)) bConfirm = true;
		if (PC->WasInputKeyJustPressed(EKeys::P) || PC->WasInputKeyJustPressed(EKeys::Escape) || PC->WasInputKeyJustPressed(EKeys::BackSpace)) bResume = true;
	}

	SelectorX = Approach(SelectorX, SettingsIndex * SetRowStep, 1000.0f, RealDt);
	if (Title && Resume && ToMenu)
	{
		Layer(Title, 0.0f, 1.0f);
		Layer(Resume, 0.0f, SettingsIndex == 0 ? 1.0f : 0.72f);
		Layer(ToMenu, 0.0f, SettingsIndex == 1 ? 1.0f : 0.72f);
		Layer(Selector, SelectorX, 1.0f);
	}
	else
	{
		DrawUIText(TEXT("SETTING"), Cream, Canvas->SizeX * 0.5f, 360.0f * S, 72.0f, 0.5f);
		DrawUIText(TEXT("回到游戏"), SettingsIndex == 0 ? Cream : Cream.CopyWithNewOpacity(0.6f), Canvas->SizeX * 0.5f, 585.0f * S, 36.0f, 0.5f);
		DrawUIText(TEXT("回到主界面"), SettingsIndex == 1 ? Cream : Cream.CopyWithNewOpacity(0.6f), Canvas->SizeX * 0.5f, 700.0f * S, 36.0f, 0.5f);
	}

	if (bResume || (bConfirm && SettingsIndex == 0)) CloseSettings();
	else if (bConfirm && SettingsIndex == 1) ReturnToMainMenu();
}

// ───────────────────────── 局内 ─────────────────────────

void ADysisHUD::DrawShards()
{
	const float S = Canvas->SizeX / MockW;
	DrawUIImage(UITex("Vine", TEXT("/Game/Dysis/UI/InGame/Vine.Vine")), VineBox.X * S, VineBox.Y * S, VineBox.W * S, VineBox.H * S);
	const UGameInstance* GI = GetGameInstance();
	UDysisSaveSubsystem* Save = GI ? GI->GetSubsystem<UDysisSaveSubsystem>() : nullptr;
	const UDysisSaveGame* Data = Save ? Save->GetCurrent() : nullptr;
	struct FShard { EDysisNiche Niche; FName Key; const TCHAR* Path; const FBox4* Box; };
	const FShard Shards[3] = {
		{ EDysisNiche::Sun,     "SunShard",     TEXT("/Game/Dysis/UI/InGame/SunShard.SunShard"),         &SunBox },
		{ EDysisNiche::Rainbow, "RainbowShard", TEXT("/Game/Dysis/UI/InGame/RainbowShard.RainbowShard"), &RainbowBox },
		{ EDysisNiche::Moon,    "MoonShard",    TEXT("/Game/Dysis/UI/InGame/MoonShard.MoonShard"),       &MoonBox },
	};
	for (int32 i = 0; i < 3; ++i)
	{
		const FShard& Sh = Shards[i];
		if ((ShardMask & (1 << i)) || (Data && Data->HasNiche(Sh.Niche)))
			DrawUIImage(UITex(Sh.Key, Sh.Path), Sh.Box->X * S, Sh.Box->Y * S, Sh.Box->W * S, Sh.Box->H * S);
	}
}

void ADysisHUD::SetShard(int32 Index, bool bHave)
{
	if (Index < 0 || Index > 2) return;
	if (bHave) ShardMask |= uint8(1 << Index); else ShardMask &= uint8(~(1 << Index));
}

void ADysisHUD::DrawHintBar(float Dt)
{
	const float S = Canvas->SizeX / MockW;
	// 内容：有通知显示通知；没有通知、也不在对话里时，显示脚边能互动的东西
	FString Want;
	if (NotificationTimer > 0.0f) { Want = NotificationText; NotificationTimer -= Dt; }
	else if (!(ActiveDialogue && ActiveDialogue->IsPlaying()))
	{
		if (UDysisInteractComponent* Interact = ResolveInteract())
		{
			FText Prompt;
			if (Interact->GetPrompt(Prompt)) Want = TEXT("E　") + Prompt.ToString();
		}
	}
	if (!Want.IsEmpty()) HintShownText = Want;
	HintAlpha = Approach(HintAlpha, Want.IsEmpty() ? 0.0f : 1.0f, Want.IsEmpty() ? 2.5f : 6.0f, Dt);
	if (HintAlpha <= 0.01f || HintShownText.IsEmpty()) return;

	DrawUIImage(UITex("Notify", TEXT("/Game/Dysis/UI/InGame/Notify.Notify")), HintBox.X * S, HintBox.Y * S, HintBox.W * S, HintBox.H * S, FLinearColor(1, 1, 1, HintAlpha));
	// 字放在面板正中：最多两行，放不下就把字缩小
	const float CX = (HintBox.X + 2071.0f) * S, CY = (HintBox.Y + 236.0f) * S, MaxW = 3700.0f * S;
	float Px = 27.0f;
	TArray<FString> Lines = WrapText(HintShownText, MakeFont(Px), MaxW);
	if (Lines.Num() > 2) { Px = 22.0f; Lines = WrapText(HintShownText, MakeFont(Px), MaxW); }
	const float LineH = Px * (Canvas->SizeY / 1080.0f) * 1.4f;
	const float Top = CY - Lines.Num() * LineH * 0.5f + LineH * 0.08f;
	for (int32 i = 0; i < Lines.Num(); ++i) DrawUIText(Lines[i], Cream.CopyWithNewOpacity(HintAlpha), CX, Top + i * LineH, Px, 0.5f);
}

void ADysisHUD::DrawDialogue()
{
	const float S = Canvas->SizeX / MockW;
	const bool bPlaying = ActiveDialogue && ActiveDialogue->IsPlaying() && ActiveDialogue->Lines.IsValidIndex(ActiveDialogue->CurrentLine);
	const float RealDt = FMath::Clamp(float(FApp::GetDeltaTime()), 0.0f, 0.1f);
	DialogueAlpha = Approach(DialogueAlpha, bPlaying ? 1.0f : 0.0f, bPlaying ? 5.0f : 3.0f, RealDt);
	if (DialogueAlpha <= 0.01f) return;

	// “说话人：台词”拆开；名字写在名牌上，立绘按名字换
	FString Line = bPlaying ? ActiveDialogue->Lines[ActiveDialogue->CurrentLine].ToString() : FString();
	FString Speaker = LastSpeaker, Body;
	if (bPlaying)
	{
		if (!Line.Split(TEXT("："), &Speaker, &Body)) { Speaker.Reset(); Body = Line; }
		LastSpeaker = Speaker;
	}
	const FLinearColor White(1, 1, 1, DialogueAlpha);
	const float BoxY = Canvas->SizeY - (MockH - TextBox.Y) * S;
	DrawUIImage(UITex("TextBox", TEXT("/Game/Dysis/UI/InGame/TextBox.TextBox")), 0.0f, BoxY, TextBox.W * S, TextBox.H * S, White);
	if (const FPortrait* P = PortraitFor(Speaker))
		DrawUIImage(UITex(P->Key, P->Path), Canvas->SizeX - P->W * S, Canvas->SizeY - P->H * S, P->W * S, P->H * S, White);
	if (!bPlaying) return;

	const FLinearColor Ink = Cream.CopyWithNewOpacity(DialogueAlpha);
	// 名牌（图里 x 4270–5830、y 20–365）：名字从左边的星芒后面写起
	if (!Speaker.IsEmpty())
	{
		const float NamePx = 34.0f;
		const float NameH = float(MeasureText(Speaker, MakeFont(NamePx, true)).Y);
		DrawUIText(Speaker, Ink, 4500.0f * S, BoxY + 192.0f * S - NameH * 0.5f, NamePx, 0.0f, true);
	}
	// 正文：主面板里（x 560–4050），最多三行
	DrawUIParagraph(Body, Ink, 560.0f * S, BoxY + 430.0f * S, 30.0f, 3490.0f * S, 0.0f, 1.5f);
	DrawUIText(TEXT("E　继续"), Ink.CopyWithNewOpacity(0.55f * DialogueAlpha), 4040.0f * S, BoxY + 1010.0f * S, 20.0f, 1.0f, false, false);
}

void ADysisHUD::DrawTitle(float Dt)
{
	if (TitleT < 0.0f) return;
	TitleT += Dt;
	const float In = 0.6f, Out = 1.2f;
	const float A = TitleT < In ? TitleT / In : TitleT < In + TitleHold ? 1.0f : 1.0f - (TitleT - In - TitleHold) / Out;
	if (A <= 0.0f) { TitleT = -1.0f; return; }
	const float CX = Canvas->SizeX * 0.5f, S = Canvas->SizeY / 1080.0f;
	DrawUIText(TitleMain, Cream.CopyWithNewOpacity(A), CX, 372.0f * S, 68.0f, 0.5f, true);
	DrawUIText(TitleSub, Cream.CopyWithNewOpacity(A * 0.9f), CX, 470.0f * S, 30.0f, 0.5f);
}

void ADysisHUD::DrawInGame(float Dt)
{
	const float S = Canvas->SizeX / MockW;
	DrawUIImage(UITex("SettingsInGame", TEXT("/Game/Dysis/UI/InGame/SettingsInGame.SettingsInGame")), ClockBox.X * S, ClockBox.Y * S, ClockBox.W * S, ClockBox.H * S);
	DrawShards();
	DrawHintBar(Dt);
	DrawDialogue();
}

void ADysisHUD::DrawDebug()
{
	const APlayerController* PC = GetOwningPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const UDysisTimeComponent* Time = Pawn ? Pawn->FindComponentByClass<UDysisTimeComponent>() : nullptr;
	if (!Time) return;
	const float Clock = FMath::Fmod(12.0f + Time->H / 15.0f, 24.0f);
	const FString Info = FString::Printf(TEXT("zone=%s  H=%.2f  %02d:%02d  %s  foot=(%.0f, %.0f, %.0f)"),
		*Time->Zone, Time->H, int32(Clock), int32(Clock * 60.0f) % 60, Time->bNight ? TEXT("night") : TEXT("day"),
		Time->FootCm.X, Time->FootCm.Y, Time->FootCm.Z);
	DrawUIText(Info, FLinearColor(0.4f, 1.0f, 0.4f, 0.95f), 24.0f, Canvas->SizeY - 40.0f, 18.0f);
}

// ───────────────────────── 每帧 ─────────────────────────

void ADysisHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	APlayerController* PC = GetOwningPlayerController();
	const float RealDt = FMath::Clamp(float(FApp::GetDeltaTime()), 0.0f, 0.1f);
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

	bMouseValid = false; bClick = false;
	if (PC)
	{
		float MX = 0.0f, MY = 0.0f;
		if (PC->GetMousePosition(MX, MY)) { bMouseValid = true; MousePos = FVector2D(MX, MY); }
		bClick = PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
		if (PC->WasInputKeyJustPressed(EKeys::F3)) bShowDebug = !bShowDebug;
	}
	ApplyInputForScreen();

	switch (Screen)
	{
	case EDysisScreen::MainMenu:
		HoldMenuView(RealDt);
		DrawMainMenu(RealDt);
		break;
	case EDysisScreen::Settings:
		DrawInGame(0.0f);
		DrawSettings(RealDt);
		break;
	case EDysisScreen::Playing:
		if (PC && StartPhase == 0 && (PC->WasInputKeyJustPressed(EKeys::P) || PC->WasInputKeyJustPressed(EKeys::Escape))) { OpenSettings(); break; }
		DrawInGame(Dt);
		break;
	}
	DrawTitle(Dt);

	// 黑场；“开始游戏”：黑下去 → 画面切回玩家、天放开 → 亮起来
	FadeAlpha = Approach(FadeAlpha, FadeTarget, FadeRate, RealDt);
	if (StartPhase == 1 && FadeAlpha >= 0.995f)
	{
		Screen = EDysisScreen::Playing;
		if (PC)
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				PC->SetViewTarget(Pawn);
				if (UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>()) Time->ClearForcedTime();
			}
		}
		if (MenuCamera) { MenuCamera->Destroy(); MenuCamera = nullptr; }
		ApplyInputForScreen();
		OnGameStarted.Broadcast();
		StartPhase = 2;
		FadeTo(0.0f, 0.7f);
	}
	else if (StartPhase == 2 && FadeAlpha <= 0.005f) StartPhase = 0;
	if (FadeAlpha > 0.002f) DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, FadeAlpha), 0, 0, Canvas->SizeX, Canvas->SizeY);

	if (bShowDebug && Screen == EDysisScreen::Playing) DrawDebug();
}