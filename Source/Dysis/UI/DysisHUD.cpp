#include "DysisHUD.h"
#include "DysisDialogueComponent.h"
#include "Interaction/DysisInteractComponent.h"
#include "Mechanisms/DysisInteractable.h"
#include "Sky/DysisTimeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/FontFace.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"

FDysisNotificationShown ADysisHUD::OnNotificationShown;

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

void ADysisHUD::DrawUIText(const FString& Text, const FLinearColor& Color, float X, float Y, float Scale)
{
	if (!Canvas) return;
	if (FontFace)
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), FSlateFontInfo(FontFace, 16.0f * Scale), Color);
		Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), FVector2D(1.0f, 1.0f));
		Canvas->DrawItem(Item);
	}
	else
	{
		DrawText(Text, Color, X, Y, nullptr, Scale, false);
	}
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

void ADysisHUD::ShowNotification(const FText& Text, float DurationSeconds)
{
	NotificationText = Text.ToString();
	NotificationTimer = DurationSeconds;
	NotificationDuration = DurationSeconds;
	OnNotificationShown.Broadcast(Text);
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

	// 通知框：底部中间偏上（比对话低一层），淡入淡出由透明度渐变——文案表所有非对话文本的显示口。
	const float Alpha = (NotificationTimer > NotificationDuration - 0.5f)
		? (NotificationDuration - NotificationTimer) / 0.5f   // 前 0.5s 淡入
		: (NotificationTimer < 1.0f ? NotificationTimer / 1.0f : 1.0f);  // 后 1s 淡出

	const float Y = Canvas->SizeY - 140.0f;
	const float TextW = NotificationText.Len() * 14.0f;
	const float X = (Canvas->SizeX - TextW) * 0.5f;
	FLinearColor Color(0.95f, 0.92f, 0.85f, Alpha * 0.9f);   // 暖白

	DrawRect(FLinearColor(0, 0, 0.05f, Alpha * 0.4f), X - 20.0f, Y - 8.0f, TextW + 40.0f, 32.0f);
	DrawUIText(NotificationText, Color, X, Y, 0.85f);
}

void ADysisHUD::DrawDialogue()
{
	if (!ActiveDialogue || !ActiveDialogue->IsPlaying() || !Canvas) return;
	const int32 Idx = ActiveDialogue->CurrentLine;
	if (!ActiveDialogue->Lines.IsValidIndex(Idx)) return;

	// 底部一行半透明底 + 台词 + 句点进度 + [E] 提示（正式版换美术对白框）。
	const FString Line = ActiveDialogue->Lines[Idx].ToString();
	const float Y = Canvas->SizeY - 90.0f;
	DrawRect(FLinearColor(0, 0, 0.04f, 0.55f), 0, Y - 12.0f, Canvas->SizeX, 52.0f);
	DrawUIText(Line, FLinearColor(0.92f, 0.92f, 1.0f, 0.95f), 40.0f, Y, 1.1f);
	FString Dots;
	for (int32 i = 0; i < ActiveDialogue->Lines.Num(); ++i) Dots += (i == Idx ? TEXT("●") : TEXT("○"));
	DrawUIText(Dots, FLinearColor(0.7f, 0.75f, 0.9f, 0.8f), 40.0f, Y + 26.0f, 0.7f);
	DrawUIText(TEXT("[E] 下一句"), FLinearColor(0.6f, 0.62f, 0.7f, 0.8f), Canvas->SizeX - 150.0f, Y + 26.0f, 0.7f);
}

void ADysisHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	// ── 交互提示：屏幕中心下方，目标名（拉/转动/拿起碎片……）——"指尖光点"的文字替身，正式版换 UI 资产 ──
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
					const float X = Canvas->SizeX * 0.5f - Line.Len() * 4.0f;
					DrawUIText(Line, FLinearColor(1.0f, 0.95f, 0.8f, 0.9f), X, Canvas->SizeY * 0.5f + 40.0f, 0.9f);
				}
			}
		}
	}

	// ── 对白：正在播的对话组件画底部台词（对话期间交互键当"下一句"用，见 Character 的接线）──
	DrawDialogue();

	// ── 通知：解谜反馈/提示/机关反馈（文案表非对话文本的显示口）──
	DrawNotification();
	if (NotificationTimer > 0.0f) NotificationTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

	// ── 调试面板（Dysis.Where 的屏幕版）──
	if (bShowDebug)
	{
		if (APlayerController* PC = GetOwningPlayerController())
			if (const APawn* Pawn = PC->GetPawn())
				if (const UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>())
				{
					const float Clock = 12.0f + Time->H / 15.0f;
					const FString Info = FString::Printf(TEXT("zone=%s  H=%.2f  clock=%02d:%02d  %s"),
						*Time->Zone, Time->H, int32(Clock) % 24, int32(Clock * 60.0f) % 60,
						Time->bNight ? TEXT("night") : TEXT("day"));
					DrawUIText(Info, FLinearColor::Green, 24.0f, 24.0f, 0.8f);
				}
	}
}
