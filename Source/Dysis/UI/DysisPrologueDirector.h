// 日落回廊 · 开场演出导演（M7 演出收尾件，文案表·游戏剧情·游戏开场）：
// 游戏启动 13:29（H_I）→ 播开场对话（赫利俄斯×狄西斯 9 句，文案表第 1-9 行）→ 对话结束 → 光路显现。
// 摆进关卡一个即可（GameMode 生成或手动摆都行）。
// 对话期间可设 ForcedH 钉住时刻（开场对话在 H_I 不动——光路要等对话完才显形）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisPrologueDirector.generated.h"

class UDysisDialogueComponent;

UCLASS()
class DYSIS_API ADysisPrologueDirector : public AActor
{
	GENERATED_BODY()

public:
	ADysisPrologueDirector();

	/** 开场对话（赫利俄斯×狄西斯，文案表第 1-9 句）——BeginPlay 延迟 2 秒后自动播。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prologue")
	bool bAutoStart = true;

	/** 延迟秒数（等玩家 Pawn / SkyActor 就位）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prologue")
	float StartDelaySeconds = 2.0f;

	/** 对话期间钉住时刻为 H_I（开场对话时光不动）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prologue")
	bool bFreezeTimeDuringDialogue = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Dysis|Prologue")
	TObjectPtr<UDysisDialogueComponent> Dialogue;

private:
	float Countdown = -1.0f;
	bool bStarted = false;
};
