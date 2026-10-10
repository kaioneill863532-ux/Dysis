// 狄西斯的日落回廊 · 背景音乐管理器（来自美术/音频侧 zip 包 2026-10-06；2026-10-10 改成只发号施令）
// 关卡里放一个（导入脚本会自动放好并填上曲子），其他代码用 GetDysisMusicManager() 找到它。
// 曲子真正在放的地方是 UDysisMusicSubsystem（挂在游戏实例上，换关卡不断）。
// 音乐的走法（2026-10-10 用户定的）：
//   · 主界面放白天的曲子；
//   · 点“开始游戏”以后先淡出，只留海浪声；人踩上通往神殿的那道光，曲子才慢慢响起来；
//   · 靠近机关、过剧情时压低一些，离开、剧情结束再放回来——像呼吸一样有起伏；
//   · 接住最后一缕光入夜：白天淡出 → 安静几秒 → 夜晚淡入；
//   · 结尾把金苹果放上月托：夜晚的曲子淡下去、换回白天的曲子，剧情放完回到主界面时接着放，不断。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisMusicManager.generated.h"

class USoundBase;
class UDysisMusicSubsystem;

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
	float MusicVolume = 1.4f;

	/** 开始播放时淡入的秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float FadeInSeconds = 3.0f;

	/** 入夜时白天音乐淡出的秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float FadeOutSeconds = 3.0f;

	/** 白天淡出之后、夜晚淡入之前的安静秒数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float SilenceSeconds = 1.5f;

	/** 进关卡时自动开始播白天音乐（主界面的音乐） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music")
	bool bAutoPlayDay = true;

	/** 人踩上通往神殿的那道光以后，曲子用多少秒慢慢响起来。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float RiseSeconds = 9.0f;

	/** 靠近机关、过剧情时压到原来的多少；压下去、放回来各用几秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DuckLevel = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float DuckDownSeconds = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float DuckUpSeconds = 3.0f;

	/** 结尾从夜晚的曲子换回白天的曲子：夜晚淡出、白天淡入各用几秒（两段叠着走）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Music", meta = (ClampMin = "0.0"))
	float EndingCrossSeconds = 6.0f;

	/** 播白天音乐（淡入）。夜晚音乐若在播会淡出。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void PlayDay();

	/** 白天的曲子慢慢响起来（人踩上通往神殿的那道光时叫）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void RiseDay();

	/** 入夜：白天淡出 → 安静 SilenceSeconds 秒 → 夜晚淡入。重复调用无副作用。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void SwitchToNight();

	/** 结尾：夜晚的曲子淡下去，白天的曲子叠着淡进来。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void CrossToDay();

	/** 全部淡出停止 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void StopMusic(float FadeSeconds = 2.0f);

	/** 靠近机关 / 过剧情：压低；离开：放回来。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void SetDucked(bool bDucked);

	/** 测试用：整个静音（测试跑的时候不出声）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void SetMuted(bool bMuted);

	/** 改总音量，立即生效（设置菜单里的音乐音量可以接这个） */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	void SetMusicVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Dysis|Music")
	bool IsNight() const;

	/** 测试用：现在想放哪一首（day / night / none）、各自多响、压低到多少。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Music")
	FString DescribeMusic() const;

	/** 在当前关卡里找到音乐管理器（找不到返回空） */
	UFUNCTION(BlueprintPure, Category = "Dysis|Music", meta = (WorldContext = "WorldContextObject"))
	static ADysisMusicManager* GetDysisMusicManager(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;

private:
	UDysisMusicSubsystem* Sub() const;
};
