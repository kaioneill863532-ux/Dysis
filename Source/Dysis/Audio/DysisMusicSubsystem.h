// 日落回廊 · 背景音乐真正在放的地方：两首曲子（白天 / 夜晚）各一个播放器，挂在游戏实例上——
// 换关卡（结局以后重新载入回到主界面）时不会断，曲子接着往下放。
// 每首各有一个“现在多响”和“想要多响”，每帧朝目标走；另有一个整体的“压低”（靠近机关、过剧情时），也是慢慢走。
// 关卡里的 ADysisMusicManager 只管填曲子、定音量、发号施令；别的代码仍然找它。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "DysisMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS()
class DYSIS_API UDysisMusicSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	enum : int32 { Day = 0, Night = 1, NumTracks = 2 };

	static UDysisMusicSubsystem* Get(const UObject* WorldContextObject);

	void SetSounds(USoundBase* DaySound, USoundBase* NightSound);
	void SetVolume(float InVolume) { Volume = InVolume; }
	void SetMuted(bool bInMuted) { bMuted = bInMuted; }

	/** 换成放这一首（Track = -1：都不放）。别的曲子用 FadeOutSeconds 淡出；这一首等 DelaySeconds 以后用 FadeInSeconds 淡入。 */
	void Play(int32 Track, float FadeOutSeconds, float FadeInSeconds, float DelaySeconds = 0.0f);

	/** 整体压到多少（1 = 不压）；压下去用 DownSeconds，放回来用 UpSeconds。 */
	void SetDuck(float Level, float DownSeconds, float UpSeconds);

	/** 现在想放的是哪一首（-1 = 都不放）。 */
	int32 GetWanted() const { return Wanted; }
	/** 这一首现在有多响（0–1，不算压低和总音量）。 */
	float GetGain(int32 Track) const { return Track >= 0 && Track < NumTracks ? Gain[Track] : 0.0f; }
	float GetDuck() const { return Duck; }

	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDysisMusicSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> Audio[NumTracks];

	UPROPERTY()
	TObjectPtr<USoundBase> Sounds[NumTracks];

	float Gain[NumTracks] = { 0.0f, 0.0f }, Target[NumTracks] = { 0.0f, 0.0f }, Rate[NumTracks] = { 1.0f, 1.0f }, Delay[NumTracks] = { 0.0f, 0.0f };
	float Volume = 1.4f, Duck = 1.0f, DuckTarget = 1.0f, DuckDownRate = 1.0f, DuckUpRate = 0.5f;
	int32 Wanted = -1;
	bool bMuted = false;
};
