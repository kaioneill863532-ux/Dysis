// 日落回廊 · 时辰龛（M7 接入件，game-design §8）：主线旁的可选收集——日之龛（光阶第三束窗里）、
// 虹之龛（虹桥到窗台后）、月之龛（子夜跳井底）。摸一下 = 拿碎片：写存档（MarkNiche + 手动档 SaveNow）
// 并广播 OnCollected（将来驱动开盖动画/碎片 UI/黎明结局判定 AllNichesCollected）。
// 已拿过的龛再摸只提示不重复写（单调原则）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mechanisms/DysisInteractable.h"
#include "Save/DysisSaveGame.h"
#include "DysisNicheActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDysisNicheCollected, EDysisNiche, Niche);

UCLASS()
class DYSIS_API ADysisNicheActor : public AActor, public IDysisInteractable
{
	GENERATED_BODY()

public:
	ADysisNicheActor();

	/** 这是哪个龛。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Niche")
	EDysisNiche Niche = EDysisNiche::Sun;

	/** 拿到时是否写手动档（默认是——龛是重要节点）；false 则只写内存+自动档由调用方控。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Niche")
	bool bSaveManualOnCollect = true;

	/** 拿到碎片那一刻广播（开盖动画/UI/音效挂这）。 */
	UPROPERTY(BlueprintAssignable, Category = "Dysis|Niche")
	FDysisNicheCollected OnCollected;

	// ── IDysisInteractable ──
	virtual void Interact(APawn* Player, bool bFromFront = true) override;
	virtual FText GetInteractPrompt() const override;

	/** 这龛是否已收进存档。 */
	bool IsCollected() const;

protected:
	virtual void BeginPlay() override;

	bool bCollectedThisSession = false;   // 本局已拿（存档为真的超集；重开后由存档压回来）

private:
	/** 本工程的存档子系统（GameInstance 上，任何关卡都拿得到）。 */
	class UDysisSaveSubsystem* GetGameInstanceSave() const;
};
