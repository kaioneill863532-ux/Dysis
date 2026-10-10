// 日落回廊 · 众神对话播放器（M7 演出先行，零资产）：把台词逐句打到 HUD 底部（每句 N 秒，
// 或玩家按 E 下一句），期间可选钉住时刻（SetForcedTime，过场语义）。正式版换 Sequencer+美术演出，
// 台词与节奏先用这个调。台词样例=设计 9.3 的六句（伊里斯/赫利俄斯/塞勒涅）。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DysisDialogueComponent.generated.h"

class UDysisTimeComponent;

UCLASS(ClassGroup = (Dysis), meta = (BlueprintSpawnableComponent))
class DYSIS_API UDysisDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDysisDialogueComponent();

	/** 台词（顺序播）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Dialogue")
	TArray<FText> Lines;

	/** 每句自己翻页的秒数。0 = 不自己翻，等玩家点（鼠标左键 / E / 空格）。
	 *  2026-10-10 用户定：自动翻页关掉——句子正要自己翻的时候玩家点一下，会一下跳两句。
	 *  自动测试里（HUD 的测试模式）没人点，按 4 秒一句自己翻。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Dialogue")
	float SecondsPerLine = 0.0f;

	/** 播放期间钉住的时刻（<0 不钉；设计：虹之龛对话发生在 RELIEF_H）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Dialogue")
	float ForcedH = -1.0f;

	/** 开始播放（重入安全：正在播则重启）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Dialogue")
	void Play();

	/** 立即翻下一句。 */
	void Advance();

	/** 玩家点了一下：这一句刚出来不到 0.25 秒的话不算（连点两下不会连翻两句）。翻了返回 true。 */
	bool AdvanceByPlayer();

	/** 正在播吗。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Dialogue")
	bool IsPlaying() const { return CurrentLine >= 0; }

	/** 当前句（-1 = 没在播；HUD 画它）。 */
	UPROPERTY(VisibleInstanceOnly, Category = "Dysis|Dialogue")
	int32 CurrentLine = -1;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float LineTimer = 0.0f;
	float LineShownSeconds = 0.0f;
	float AutoSeconds() const;
	TWeakObjectPtr<class ADysisHUD> Hud;
	UDysisTimeComponent* ResolveTime();
	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
};
