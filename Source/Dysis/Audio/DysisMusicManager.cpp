// 狄西斯的日落回廊 · 背景音乐管理器（来自美术/音频侧 zip 包 2026-10-06；2026-10-10 改成只发号施令）
#include "DysisMusicManager.h"
#include "DysisMusicSubsystem.h"

#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

ADysisMusicManager::ADysisMusicManager()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

UDysisMusicSubsystem* ADysisMusicManager::Sub() const
{
	return UDysisMusicSubsystem::Get(this);
}

void ADysisMusicManager::BeginPlay()
{
	Super::BeginPlay();
	UDysisMusicSubsystem* S = Sub();
	if (!S) return;
	S->SetSounds(DayMusic, NightMusic);
	S->SetVolume(MusicVolume);
	S->SetMuted(false);
	S->SetDuck(1.0f, DuckDownSeconds, DuckUpSeconds);
	// 进关卡（主界面）：白天的曲子。上一关结尾已经在放白天的曲子的话就接着放，不重新开始
	if (bAutoPlayDay && S->GetWanted() != UDysisMusicSubsystem::Day) PlayDay();
}

void ADysisMusicManager::PlayDay()
{
	if (!DayMusic) { UE_LOG(LogTemp, Warning, TEXT("DysisMusic: DayMusic 没有设置")); return; }
	if (UDysisMusicSubsystem* S = Sub()) S->Play(UDysisMusicSubsystem::Day, FadeOutSeconds, FadeInSeconds);
}

void ADysisMusicManager::RiseDay()
{
	if (UDysisMusicSubsystem* S = Sub()) S->Play(UDysisMusicSubsystem::Day, FadeOutSeconds, RiseSeconds);
}

void ADysisMusicManager::SwitchToNight()
{
	UDysisMusicSubsystem* S = Sub();
	if (!S || S->GetWanted() == UDysisMusicSubsystem::Night) return;
	if (!NightMusic) { UE_LOG(LogTemp, Warning, TEXT("DysisMusic: NightMusic 没有设置")); return; }
	// 白天的还响着：等它淡出、再安静一会儿；本来就没在放（还没踩上那道光就入夜了，只有测试里会这样）就直接淡入
	const bool bDayAudible = S->GetGain(UDysisMusicSubsystem::Day) > 0.01f;
	S->Play(UDysisMusicSubsystem::Night, FadeOutSeconds, FadeInSeconds, bDayAudible ? FadeOutSeconds + SilenceSeconds : 0.0f);
}

void ADysisMusicManager::CrossToDay()
{
	if (UDysisMusicSubsystem* S = Sub()) S->Play(UDysisMusicSubsystem::Day, EndingCrossSeconds, EndingCrossSeconds);
}

void ADysisMusicManager::StopMusic(float FadeSeconds)
{
	if (UDysisMusicSubsystem* S = Sub()) S->Play(-1, FMath::Max(FadeSeconds, 0.0f), 0.0f);
}

void ADysisMusicManager::SetDucked(bool bDucked)
{
	if (UDysisMusicSubsystem* S = Sub()) S->SetDuck(bDucked ? DuckLevel : 1.0f, DuckDownSeconds, DuckUpSeconds);
}

void ADysisMusicManager::SetMuted(bool bMuted)
{
	if (UDysisMusicSubsystem* S = Sub()) S->SetMuted(bMuted);
}

void ADysisMusicManager::SetMusicVolume(float NewVolume)
{
	MusicVolume = FMath::Clamp(NewVolume, 0.0f, 2.0f);
	if (UDysisMusicSubsystem* S = Sub()) S->SetVolume(MusicVolume);
}

bool ADysisMusicManager::IsNight() const
{
	const UDysisMusicSubsystem* S = Sub();
	return S && S->GetWanted() == UDysisMusicSubsystem::Night;
}

FString ADysisMusicManager::DescribeMusic() const
{
	const UDysisMusicSubsystem* S = Sub();
	if (!S) return TEXT("{}");
	const int32 W = S->GetWanted();
	return FString::Printf(TEXT("{\"want\":\"%s\",\"day\":%.3f,\"night\":%.3f,\"duck\":%.3f}"),
		W == UDysisMusicSubsystem::Day ? TEXT("day") : W == UDysisMusicSubsystem::Night ? TEXT("night") : TEXT("none"),
		S->GetGain(UDysisMusicSubsystem::Day), S->GetGain(UDysisMusicSubsystem::Night), S->GetDuck());
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
