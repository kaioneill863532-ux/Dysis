#include "DysisMusicSubsystem.h"
#include "Audio/DysisUserVolume.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"

UDysisMusicSubsystem* UDysisMusicSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UDysisMusicSubsystem>() : nullptr;
}

UWorld* UDysisMusicSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

void UDysisMusicSubsystem::SetSounds(USoundBase* DaySound, USoundBase* NightSound)
{
	// 换了曲子（一般不会）：正在放的那个播放器停掉，下次要放时按新曲子重建
	USoundBase* In[NumTracks] = { DaySound, NightSound };
	for (int32 i = 0; i < NumTracks; ++i)
	{
		if (Sounds[i] == In[i]) continue;
		Sounds[i] = In[i];
		if (Audio[i]) { Audio[i]->Stop(); Audio[i] = nullptr; }
		Gain[i] = 0.0f;
	}
}

void UDysisMusicSubsystem::Play(int32 Track, float FadeOutSeconds, float FadeInSeconds, float DelaySeconds)
{
	Wanted = Track >= 0 && Track < NumTracks ? Track : -1;
	for (int32 i = 0; i < NumTracks; ++i)
	{
		const bool bOn = i == Wanted;
		Target[i] = bOn ? 1.0f : 0.0f;
		const float Seconds = bOn ? FadeInSeconds : FadeOutSeconds;
		Rate[i] = Seconds > KINDA_SMALL_NUMBER ? 1.0f / Seconds : 1.0e6f;
		Delay[i] = bOn ? FMath::Max(DelaySeconds, 0.0f) : 0.0f;
	}
}

void UDysisMusicSubsystem::SetDuck(float Level, float DownSeconds, float UpSeconds)
{
	DuckTarget = FMath::Clamp(Level, 0.0f, 1.0f);
	DuckDownRate = DownSeconds > KINDA_SMALL_NUMBER ? 1.0f / DownSeconds : 1.0e6f;
	DuckUpRate = UpSeconds > KINDA_SMALL_NUMBER ? 1.0f / UpSeconds : 1.0e6f;
}

void UDysisMusicSubsystem::Tick(float DeltaTime)
{
	// 用真实时间走：游戏暂停（设置页）时音乐照样淡入淡出
	const float Dt = FMath::Clamp(float(FApp::GetDeltaTime()), 0.0f, 0.1f);
	Duck = Duck > DuckTarget ? FMath::Max(DuckTarget, Duck - DuckDownRate * Dt) : FMath::Min(DuckTarget, Duck + DuckUpRate * Dt);
	UWorld* World = GetTickableGameObjectWorld();
	for (int32 i = 0; i < NumTracks; ++i)
	{
		if (Delay[i] > 0.0f) { Delay[i] -= Dt; continue; }
		Gain[i] = Gain[i] < Target[i] ? FMath::Min(Target[i], Gain[i] + Rate[i] * Dt) : FMath::Max(Target[i], Gain[i] - Rate[i] * Dt);
		const bool bWant = Target[i] > 0.0f || Gain[i] > 0.0f;
		if (bWant && !Audio[i] && Sounds[i] && World)
		{
			// 不跟着关卡走的播放器：换关卡时接着放
			Audio[i] = UGameplayStatics::CreateSound2D(World, Sounds[i], 0.0f, 1.0f, 0.0f, nullptr, /*bPersistAcrossLevelTransition*/ true, /*bAutoDestroy*/ false);
			if (Audio[i]) Audio[i]->bIsUISound = true;   // 游戏暂停（设置页）时照样放：在设置页里拖音量听得见
		}
		UAudioComponent* A = Audio[i];
		if (!A) continue;
		if (bWant)
		{
			// 平方一下：音量听起来是匀速变的
			A->SetVolumeMultiplier(FMath::Max(bMuted ? 0.0f : Volume * DysisUserVolume::Music() * Gain[i] * Gain[i] * Duck, 0.0001f));
			if (!A->IsPlaying()) A->Play(0.0f);
		}
		else if (A->IsPlaying()) A->Stop();
	}
}

void UDysisMusicSubsystem::Deinitialize()
{
	for (int32 i = 0; i < NumTracks; ++i)
	{
		if (Audio[i]) Audio[i]->Stop();
		Audio[i] = nullptr;
	}
	Super::Deinitialize();
}
