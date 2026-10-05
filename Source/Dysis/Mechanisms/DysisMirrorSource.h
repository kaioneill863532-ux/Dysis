// 日落回廊 · 铜镜反射源（日3 三相像，调研 M3）：给 bFromMirror 的光柱提供镜面法线。
// 机关清单（三相像）：像+铜镜绕台面竖轴转，停在这几格（镜面朝向方位角）：
//   68.90 / 122.90 / 153.19 / 206.19 / 259.19 / 312.19；开局白天格 153.19；日相镜俯仰 30.90°。
// v1：摸一下顺时针进一格（转台/绞盘 1:2 的手感 M3 后接）。灰盒对应：第五格（206.19°）把光送到 L3 东南。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mechanisms/DysisInteractable.h"
#include "DysisMirrorSource.generated.h"

UCLASS()
class DYSIS_API ADysisMirrorSource : public AActor, public IDysisInteractable
{
	GENERATED_BODY()

public:
	ADysisMirrorSource();

	/** 六个卡槽的镜面朝向（方位角，度）。默认=机关清单三相像的六格。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	TArray<double> SlotAzDeg = { 68.90, 122.90, 153.19, 206.19, 259.19, 312.19 };

	/** 开局在第几格（0 起）。机关清单：白天那一格 = 153.19°，即下标 2。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	int32 StartSlot = 2;

	/** 镜面俯仰（度，日相 30.90；月相 21.29 由机关侧切，v1 手调）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double MirrorPitchDeg = 30.90;

	/** 转格的恒速角速度（度/秒）——"石头底座"手感；绞盘 1:2 的美术耦合后接（TODO M3）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double RotateDegPerSec = 45.0;

	// ── IDysisInteractable ──
	virtual void Interact(APawn* Player, bool bFromFront = true) override;
	virtual FText GetInteractPrompt() const override;   // 文案表：三相机关→"转动雕像"

	/** 当前镜面法线（世界系，归一）——光柱每帧来取。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Mech")
	FVector GetMirrorNormal() const;

	/** 当前格的朝向（度）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Mech")
	double GetCurrentSlotAz() const;

	/** 当前格下标（0 起；Dysis.MirrorInfo 调试用）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Mech")
	int32 GetCurrentSlot() const { return CurrentSlot; }

	/** §4 镜面照亮比例（0–1）：3×3=9 个采样点沿太阳方向打 trace，≥50% 被照亮才允许镜光 beam 可走。
	 *  规格书原话"镜面照亮一半以上，镜光才能走"。每帧重算（beam 的 UpdateFor 调）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Mech")
	double GetMirrorIllumination() const { return MirrorIllumination; }

	/** §4 镜光可走判定：照亮 ≥50% 且形态为日相（规格书"镜心被阳光照到、形态是'日'时"）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Mech")
	bool CanMirrorBeamWalk() const { return MirrorIllumination >= 0.5; }

	/** 采样半径（厘米；镜面 2.2×1.4 m 取内切圆 ≈ 70cm）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double MirrorSampleRadiusCm = 70.0;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	int32 CurrentSlot = 0;
	double CurrentAz = 153.19;   // 平滑的当前朝向（度）——反射光随它扫动
	bool bAzInit = false;
	mutable double MirrorIllumination = 1.0;   // §4 每帧更新的镜面照亮比例

	/** §4 镜面 3×3 采样：在镜面法线平面上取 9 个点，逐点沿 SunDir 打 trace。 */
	void SampleMirrorIllumination();
	class ADysisSkyActor* ResolveSkyForSample() const;
	mutable TWeakObjectPtr<class ADysisSkyActor> CachedSampleSky;
};
