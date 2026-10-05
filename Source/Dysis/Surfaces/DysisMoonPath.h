// 日落回廊 · 月光大道（M5，灰盒 2.3/13.3）：水面起波纹时，月亮的倒影被拉成一条从脚下伸向月亮的光带。
// 铺法（灰盒原值）：以 0.3 m 为一步、从玩家水边立足点沿"那一刻月亮的方位"铺 1.3 m 宽的踏面，
// 直到月光照不到（出水面范围）或到对岸；走的过程中月亮挪、光带每 0.1 s 重铺跟着拐弯。
// 判定与渲染分离：本 Actor 只管"哪些点踩得住"（IDysisVirtualSurface）+ 调试图形；
// 视觉光带由水面材质响应月亮方向自动呈现（调研 §12.4，物理依据 NOAA：带宽随月高自动变）。
// 站上光带时时间系统按 ZoneName 改判区域（默认 gbridge=H_BR；按施工图逐段查表填）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Optics/DysisVirtualSurface.h"
#include "DysisMoonPath.generated.h"

class ADysisMoonSurface;
class ADysisSkyActor;
class UDysisTimeComponent;

UCLASS()
class DYSIS_API ADysisMoonPath : public AActor, public IDysisVirtualSurface
{
	GENERATED_BODY()

public:
	ADysisMoonPath();

	/** 监听的水面（波纹态才有大道；静水走踏片）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	TObjectPtr<ADysisMoonSurface> TargetWater;

	/** 光带的面高（UE 厘米；水庭水面 y=-0.45 m → -45，随水面 Actor 定）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	double SurfaceZCm = -45.0;

	/** 一步的长度与带宽（灰盒：0.3 m 步、1.3 m 宽）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	double StepCm = 30.0;
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	double WidthCm = 130.0;

	/** 最多铺多少步（出水面自动停）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	int32 MaxSteps = 150;

	/** 站上光带时报给时间系统的区域名（默认细桥段；天池/潮沟按施工图改）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	FName ZoneName = TEXT("gbridge");

	/** 面向月亮才铺（灰盒：站在水边面向月亮，它就铺出来）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MoonPath")
	double ViewMoonMaxDeg = 80.0;

	// ── IDysisVirtualSurface ──
	virtual bool IsWalkableAt(const FVector& FootCm) const override;
	virtual bool GetZoneName(FName& OutZone) const override;

	/** 文案表·提示条件：玩家是否走过月桥（IsWalkableAt 返回过 true 即置位——"下了月桥走到一层"判定用）。 */
	bool HasWalked() const { return bHasWalked; }

protected:
	virtual void Tick(float DeltaTime) override;

	/** 每 RebuildInterval 重铺一次（月亮在挪，光带跟着拐弯）。 */
	void Relayout();

	ADysisSkyActor* ResolveSky() const;
	UDysisTimeComponent* ResolveTime() const;

	/** 一块踏面：中心 + 走向（月亮方向的水平投影）。 */
	struct FTile
	{
		FVector CenterCm = FVector::ZeroVector;
		FVector Dir = FVector::ForwardVector;   // 单位、水平
	};
	TArray<FTile> Tiles;

	double RebuildInterval = 0.1;
	double RebuildCountdown = 0.0;

	mutable TWeakObjectPtr<ADysisSkyActor> CachedSky;
	mutable TWeakObjectPtr<UDysisTimeComponent> CachedTime;
	mutable bool bHasWalked = false;   // 走过月桥标记（const 判定路径里写入，故 mutable）
};
