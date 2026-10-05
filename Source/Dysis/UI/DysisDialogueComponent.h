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

	/** 每句停留秒数（0 = 只能按 E 翻页）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Dialogue")
	float SecondsPerLine = 4.0f;

	/** 播放期间钉住的时刻（<0 不钉；设计：虹之龛对话发生在 RELIEF_H）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Dialogue")
	float ForcedH = -1.0f;

	/** 开始播放（重入安全：正在播则重启）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Dialogue")
	void Play();

	/** 立即翻下一句（E 键；HUD 提示里写）。 */
	void Advance();

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
	UDysisTimeComponent* ResolveTime();
	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
};
