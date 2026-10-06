// 狄西斯的日落回廊 · 背景音乐管理器（来自美术/音频侧 zip 包 2026-10-06）
#include "DysisMusicManager.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

ADysisMusicManager::ADysisMusicManager()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DayAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("DayAudio"));
	DayAudio->SetupAttachment(Root);
	SetupAudio(DayAudio);

	NightAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("NightAudio"));
	NightAudio->SetupAttachment(Root);
	SetupAudio(NightAudio);
}

void ADysisMusicManager::SetupAudio(UAudioComponent* Audio)
{
	Audio->bAutoActivate = false;
	Audio->bAllowSpatialization = false;   // 背景音乐：不分方位、不随距离衰减
	Audio->bIsUISound = true;              // 暂停游戏时也继续播
}

void ADysisMusicManager::BeginPlay()
{
	Super::BeginPlay();
	DayAudio->SetVolumeMultiplier(MusicVolume);
	NightAudio->SetVolumeMultiplier(MusicVolume);
	if (bAutoPlayDay)
	{
		PlayDay();
	}
}

void ADysisMusicManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(NightTimer);
	Super::EndPlay(EndPlayReason);
}

void ADysisMusicManager::PlayDay()
{
	GetWorldTimerManager().ClearTimer(NightTimer);
	bNight = false;
	NightAudio->Stop();
	if (!DayMusic)
	{
		UE_LOG(LogTemp, Warning, TEXT("DysisMusic: DayMusic 没有设置"));
		return;
	}
	DayAudio->SetSound(DayMusic);
	DayAudio->SetVolumeMultiplier(MusicVolume);
	DayAudio->FadeIn(FadeInSeconds, 1.0f);
}

void ADysisMusicManager::SwitchToNight()
{
	if (bNight)
	{
		return;
	}
	bNight = true;

	float Wait = SilenceSeconds;
	if (DayAudio->IsPlaying())
	{
		DayAudio->FadeOut(FadeOutSeconds, 0.0f);
		Wait += FadeOutSeconds;
	}
	if (Wait <= KINDA_SMALL_NUMBER)
	{
		StartNightTrack();
	}
	else
	{
		GetWorldTimerManager().SetTimer(NightTimer, this, &ADysisMusicManager::StartNightTrack, Wait, false);
	}
}

void ADysisMusicManager::StartNightTrack()
{
	DayAudio->Stop();
	if (!NightMusic)
	{
		UE_LOG(LogTemp, Warning, TEXT("DysisMusic: NightMusic 没有设置"));
		return;
	}
	NightAudio->SetSound(NightMusic);
	NightAudio->SetVolumeMultiplier(MusicVolume);
	NightAudio->FadeIn(FadeInSeconds, 1.0f);
}

void ADysisMusicManager::StopMusic(float FadeSeconds)
{
	GetWorldTimerManager().ClearTimer(NightTimer);
	for (UAudioComponent* Audio : { DayAudio.Get(), NightAudio.Get() })
	{
		if (Audio && Audio->IsPlaying())
		{
			Audio->FadeOut(FMath::Max(FadeSeconds, 0.0f), 0.0f);
		}
	}
}

void ADysisMusicManager::SetMusicVolume(float NewVolume)
{
	MusicVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
	DayAudio->SetVolumeMultiplier(MusicVolume);
	NightAudio->SetVolumeMultiplier(MusicVolume);
}

ADysisMusicManager* ADysisMusicManager::GetDysisMusicManager(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	// Mac clang：迭代器判一次（for+return 触发 -Werror）
	TActorIterator<ADysisMusicManager> It(World);
	return It ? *It : nullptr;
}

// ───── 控制台命令（PIE 里按 ~ 输入，方便试听和调音量） ─────
namespace DysisMusicConsole
{
	static void Night(const TArray<FString>& Args, UWorld* World)
	{
		if (ADysisMusicManager* M = ADysisMusicManager::GetDysisMusicManager(World)) { M->SwitchToNight(); }
	}
	static void Day(const TArray<FString>& Args, UWorld* World)
	{
		if (ADysisMusicManager* M = ADysisMusicManager::GetDysisMusicManager(World)) { M->PlayDay(); }
	}
	static void Volume(const TArray<FString>& Args, UWorld* World)
	{
		if (ADysisMusicManager* M = ADysisMusicManager::GetDysisMusicManager(World))
		{
			if (Args.Num() > 0) { M->SetMusicVolume(FCString::Atof(*Args[0])); }
			UE_LOG(LogTemp, Display, TEXT("DysisMusic: MusicVolume = %.2f"), M->MusicVolume);
		}
	}
	static void Stop(const TArray<FString>& Args, UWorld* World)
	{
		if (ADysisMusicManager* M = ADysisMusicManager::GetDysisMusicManager(World)) { M->StopMusic(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 2.0f); }
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdNight(TEXT("Dysis.Music.Night"), TEXT("入夜：白天淡出、安静、夜晚淡入"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Night));
	static FAutoConsoleCommandWithWorldAndArgs CmdDay(TEXT("Dysis.Music.Day"), TEXT("重新播白天音乐"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Day));
	static FAutoConsoleCommandWithWorldAndArgs CmdVolume(TEXT("Dysis.Music.Volume"), TEXT("Dysis.Music.Volume 0.6  设音乐总音量（不带数字则显示当前值）"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Volume));
	static FAutoConsoleCommandWithWorldAndArgs CmdStop(TEXT("Dysis.Music.Stop"), TEXT("Dysis.Music.Stop 2  淡出停止（秒）"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stop));
}
