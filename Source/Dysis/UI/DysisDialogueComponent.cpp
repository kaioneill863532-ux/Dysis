#include "DysisDialogueComponent.h"
#include "DysisHUD.h"
#include "Sky/DysisTimeComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/ConsoleManager.h"

UDysisDialogueComponent::UDysisDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // 只在播放期间 Tick
}

UDysisTimeComponent* UDysisDialogueComponent::ResolveTime()
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}

void UDysisDialogueComponent::Play()
{
	if (Lines.Num() == 0) return;
	CurrentLine = 0;
	LineTimer = SecondsPerLine;
	SetComponentTickEnabled(true);
	// 注册到 HUD（一次只画一个；后播的顶掉先播的）。
	if (GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (ADysisHUD* H = Cast<ADysisHUD>(PC->GetHUD()))
				H->ActiveDialogue = this;
	if (ForcedH >= 0.0f)
		if (UDysisTimeComponent* Time = ResolveTime())
			Time->SetForcedTime(ForcedH);   // 过场钉住时刻（灰盒 state.forceH 同义）
}

void UDysisDialogueComponent::Advance()
{
	if (CurrentLine < 0) return;
	++CurrentLine;
	if (CurrentLine >= Lines.Num())
	{
		CurrentLine = -1;
		SetComponentTickEnabled(false);
		if (ForcedH >= 0.0f)
			if (UDysisTimeComponent* Time = ResolveTime())
				Time->ClearForcedTime();   // 演完放开时刻，天跟着人走
		return;
	}
	LineTimer = SecondsPerLine;
}

void UDysisDialogueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (CurrentLine < 0) { SetComponentTickEnabled(false); return; }
	if (SecondsPerLine > 0.0f)
	{
		LineTimer -= DeltaTime;
		if (LineTimer <= 0.0f) Advance();
	}
}

// ── 控制台：Dysis.Dialogue——六句原台词（设计 9.3）的演示，验证播放/翻页/钉时刻整链。──────
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GDysisDialogue(
		TEXT("Dysis.Dialogue"),
		TEXT("演示众神对话（设计 9.3 六句）：HUD 底部逐句，E=下一句；结束后放开 ForcedH"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (!World || !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn())
			{ UE_LOG(LogTemp, Warning, TEXT("Dysis.Dialogue：要 PIE 里（有玩家）")); return; }
			UDysisDialogueComponent* D = NewObject<UDysisDialogueComponent>(World->GetFirstPlayerController()->GetPawn());
			D->RegisterComponent();
			D->Lines = {
				FText::FromString(TEXT("伊里斯：日和月从来不同时在天上，所以你们的话，一直是我替你们传。")),
				FText::FromString(TEXT("赫利俄斯：告诉塞勒涅：今天最后一缕光，我交给那个在塔上奔跑的时辰了。")),
				FText::FromString(TEXT("塞勒涅：狄西斯？她总是来得太晚，又走得太早。")),
				FText::FromString(TEXT("伊里斯：所以她才需要这座塔。她沿着回廊每走一步，你们就在天上挪一寸。")),
				FText::FromString(TEXT("塞勒涅：那就让她把光带下来吧。我在水里等她。")),
				FText::FromString(TEXT("赫利俄斯：等她集齐了日、虹、月，黎明会来接她。")),
			};
			D->SecondsPerLine = 4.0f;
			D->ForcedH = -1.0f;   // 演示不钉时刻；虹之龛正式版设 RELIEF_H=31.05
			D->Play();
		}));
}
