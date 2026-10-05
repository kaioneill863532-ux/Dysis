// 日落回廊 · 水面三态（灰盒：水动成雾 / 水静成镜 / 水皱成道）+ 踏片亮灭判定（调研 §15.3）。调研 §13。
// 本 Actor 只承担"逻辑水面"：关卡的物理水面网格保持 NoCollision，移动组件每帧来问 IsWalkableAt。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Optics/DysisVirtualSurface.h"
#include "DysisMoonSurface.generated.h"

class ADysisSkyActor;

/** 水面三态（game-design 2.4，日夜反转同一套水）。 */
UENUM()
enum class EDysisWaterState : uint8
{
	Calm    UMETA(DisplayName = "静（镜：踏片）"),
	Ripple  UMETA(DisplayName = "波纹（月光大道）"),
	FlowOut UMETA(DisplayName = "流动/瀑布（雾源，无踏片）"),
};

UCLASS()
class DYSIS_API ADysisMoonSurface : public AActor, public IDysisVirtualSurface
{
	GENERATED_BODY()

public:
	ADysisMoonSurface();

	/** 水面范围（UE 厘米，世界系盒子）；格点判定只在这盒子里做。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Water")
	FVector BoxMinCm = FVector(-1500, -1500, -100);

	UPROPERTY(EditAnywhere, Category = "Dysis|Water")
	FVector BoxMaxCm = FVector(1500, 1500, 100);

	/** 踏片格尺寸（灰盒 1 m）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Water")
	double TileSizeCm = 100.0;

	/** 视线与月亮的允许夹角（灰盒：面向月亮踏片才亮，80°）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Water")
	double ViewMoonMaxDeg = 80.0;

	/** 当前水态。切态由闸/进水口等离散事件调 SetState（调研 §14.2），不在这里自转。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Water")
	EDysisWaterState State = EDysisWaterState::Calm;

	/** 切换水态（离散事件入口）。有 MPC 资产时同步写 Ripple/Flow 标量驱动材质（§8.6），没有就纯逻辑。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Water")
	void SetState(EDysisWaterState NewState);

	// ── IDysisVirtualSurface：移动组件的 FindFloor/Tick 来问 ──
	virtual bool IsWalkableAt(const FVector& FootCm) const override;
	virtual void NoteWalkableAt(const FVector& FootCm) override;   // 记 KeepTile（灰盒：脚下那片保留到离开）
	virtual void NoteDeparted() override;                          // 离开水面：清 KeepTile

protected:
	/** 月亮方向（懒找关卡里第一个 DysisSky；找不到返回零向量=判定不亮）。 */
	FVector MoonDirNow() const;

	/** 这格是否被月光直接照到：月亮在地平线上 + 点在盒内 + **月照遮挡 trace**（从格点沿 MoonDir 打，被墙/屋檐挡住=不亮——月3"石格窗把月光切成一排踏片"的正确性来源）。 */
	bool IsMoonLitTile(const FVector& FootCm) const;

	/** 懒找的关卡天（const 判定路径里缓存，故 mutable）。 */
	mutable TWeakObjectPtr<ADysisSkyActor> CachedSky;

	/** "脚下那片"：站稳时回写的脚位（保留格；离开才清）。 */
	FVector KeepTileCm = FVector::ZeroVector;
	bool bHasKeepTile = false;
};
