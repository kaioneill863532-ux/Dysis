// 狄西斯的日落回廊 · 背景音乐管理器（来自美术/音频侧 zip 包 2026-10-06）
// 白天播一首循环曲；入夜时：白天淡出 → 安静几秒 → 夜晚淡入。
// 关卡里放一个（导入脚本会自动放好并填上曲子），其他代码用 GetDysisMusicManager() 找到它。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisMusicManager.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS(Blueprintable, ClassGroup = (Dysis), meta = (DisplayName = "Dysis Music Manager"))
class DYSIS_API ADysisMusicManager : public AActor
{
	GENERATED_BODY()

public:
	ADysisMusicManager();

	/** 白天的循环曲（导入脚本默认填 M_Day_Sicilienne_Long） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music")
	TObjectPtr<USoundBase> DayMusic;

	/** 夜晚的循环曲（导入脚本默认填 M_Night_ClairDeLune_Full） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music")
	TObjectPtr<USoundBase> NightMusic;

	/** 音乐总音量（0 = 静音，1 = 原始响度 -20 LUFS） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MusicVolume = 0.8f;

	/** 开始播放时淡入的秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 3.0f;

	/** 入夜时白天音乐淡出的秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 3.0f;

	/** 白天淡出之后、夜晚淡入之前的安静秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float SilenceSeconds = 1.5f;

	/** 进关卡时自动开始播白天音乐 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music")
	bool bAutoPlayDay = true;

	/** 播白天音乐（淡入）。夜晚音乐若在播会先停掉。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void PlayDay();

	/** 入夜：白天淡出 → 安静 SilenceSeconds 秒 → 夜晚淡入。重复调用无副作用。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void SwitchToNight();

	/** 全部淡出停止（比如结局黑屏前） */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void StopMusic(float FadeSeconds = 2.0f);

	/** 改总音量，立即生效（设置菜单里的音乐音量可以接这个） */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void SetMusicVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Dysis|Music")
	bool IsNight() const { return bNight; }

	/** 在当前关卡里找到音乐管理器（找不到返回空） */
	UFUNCTION(BlueprintPure, Category = "Dysis|Music", meta = (WorldContext = "WorldContextObject"))
	static ADysisMusicManager* GetDysisMusicManager(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Music")
	TObjectPtr<UAudioComponent> DayAudio;

	UPROPERTY(VisibleAnywhere, Category = "Dysis|Music")
	TObjectPtr<UAudioComponent> NightAudio;

	FTimerHandle NightTimer;
	bool bNight = false;

	void StartNightTrack();
	static void SetupAudio(UAudioComponent* Audio);
};
