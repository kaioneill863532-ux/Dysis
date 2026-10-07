#include "DysisPrologueDirector.h"
#include "DysisDialogueComponent.h"
#include "DysisCopy.h"
#include "DysisHUD.h"
#include "GameFramework/PlayerController.h"
#include "Sky/DysisSkyLibrary.h"

ADysisPrologueDirector::ADysisPrologueDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisPrologueDirector::BeginPlay()
{
	Super::BeginPlay();

	// 建开场对话组件，台词从文案表取（9 句赫利俄斯×狄西斯——文案表·游戏剧情·游戏开场）。
	Dialogue = NewObject<UDysisDialogueComponent>(this);
	Dialogue->RegisterComponent();
	Dialogue->Lines.SetNum(DysisCopy::OpeningDialogueCount);
	for (int32 i = 0; i < DysisCopy::OpeningDialogueCount; ++i)
		Dialogue->Lines[i] = FText::FromString(DysisCopy::OpeningDialogue[i]);
	Dialogue->ForcedH = bFreezeTimeDuringDialogue
		? (float)UDysisSkyLibrary::DysisConst(TEXT("H_I"))   // 22.37 = 13:29（开场）
		: -1.0f;
	Dialogue->SecondsPerLine = 5.0f;   // 开场对话稍慢（叙事节奏）

	Countdown = bAutoStart ? StartDelaySeconds : -1.0f;
}

void ADysisPrologueDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bStarted || Countdown < 0.0f) { SetActorTickEnabled(false); return; }
	// 主菜单开着不倒计时（玩家点"开始游戏"后 2 秒内开播，对话不会被菜单吃掉）。
	if (GetWorld())
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (const ADysisHUD* HUD = Cast<ADysisHUD>(PC->GetHUD()))
				if (HUD->IsMenuOpen())
				{
					Countdown = FMath::Max(Countdown, StartDelaySeconds);   // 关菜单后重新给足延迟
					return;
				}
	Countdown -= DeltaTime;
	if (Countdown <= 0.0f && Dialogue)
	{
		bStarted = true;
		Dialogue->Play();
		SetActorTickEnabled(false);   // 播完即休眠（对话组件自己管理后续）
	}
}
