// 日落回廊 · 棱镜转台（M6 判定层，game-design 9.2-6）：窗台石板托着的铅水晶三棱镜，转台有八个卡槽，
// 每转一格有一种颜色落到 L2 北墙塞勒涅浮雕的眼睛上，从红到紫；第六格=靛色落进青金石眼睛 → 她醒过来
// （众神借伊里斯传话的对话触发）。色散光束的渲染（七道细光）走 beam 框架 + Optics::RefractDir，
// 本 Actor 管转台状态与"靛色入眼"判定。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mechanisms/DysisInteractable.h"
#include "DysisPrismActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDysisPrismIndigo);

UCLASS()
class DYSIS_API ADysisPrismActor : public AActor, public IDysisInteractable
{
	GENERATED_BODY()

public:
	ADysisPrismActor();

	/** 卡槽数（设计：八个）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prism")
	int32 NumSlots = 8;

	/** 开局槽位（0=红）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prism")
	int32 StartSlot = 0;

	/** "靛色入眼"的槽位（0 起数：红橙黄绿蓝靛紫的第 6 = 下标 5——设计：她只认第六色）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prism")
	int32 IndigoSlot = 5;

	/** §4 色散光束：true = 每帧更新 7 道按 IOR 折射的细光（红→紫各不同角度）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prism")
	bool bSpawnDispersionBeams = true;

	/** 色散光束截面（厘米，设计 9.1"七道细细的彩色光线，不可以踩"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Prism")
	FVector BeamExtentCm = FVector(200, 15, 15);

	/** 靛色落进眼睛那一刻广播（塞勒涅醒来→众神对话挂这）。 */
	UPROPERTY(BlueprintAssignable, Category = "Dysis|Prism")
	FDysisPrismIndigo OnIndigoOnTarget;

	// ── IDysisInteractable ──
	virtual void Interact(APawn* Player, bool bFromFront = true) override;
	virtual FText GetInteractPrompt() const override;

	/** 当前槽位（0 起）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Prism")
	int32 GetCurrentSlot() const { return CurrentSlot; }

	/** 当前颜色的折射率（Optics::PrismIor 同表——色散光束渲染共用一个源）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Prism")
	double GetCurrentIor() const;

	/** 当前是不是靛色入眼格。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Prism")
	bool IsIndigoOnTarget() const { return CurrentSlot == IndigoSlot; }

protected:
	virtual void BeginPlay() override;

	int32 CurrentSlot = 0;
	bool bIndigoFired = false;   // 对话只触发一次（单调）

	/** 色散光束 Actor 数组（7 道，红→紫）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class ADysisBeamActor>> DispersionBeams;

	/** 每帧更新色散光束的方向（按当前 IOR 用 Snell 折射计算）。 */
	void UpdateDispersionBeams();

	/** 演出：靛色入眼自动播众神对话（设计 9.3 六句 + 钉住 RELIEF_H）。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Prism")
	TObjectPtr<class UDysisDialogueComponent> Dialogue;
};
