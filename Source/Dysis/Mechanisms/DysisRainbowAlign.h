// 日落回廊 · 虹之龛对齐判定（M6 判定层，game-design 9.2）：人背对太阳站在离水帘一米多处，
// 影子的头住进浮雕上空着的人形里 → 虹醒过来（虹桥延伸、龛开）。渲染（水帘里的整圈虹）走材质/MPC，
// 本 Actor 只做 CPU 判定：影头点（Optics::ShadowHeadPoint，§8.8）vs 人形中心的距离阈值。
// 阈值/站位/平面全部 UPROPERTY——数值等关卡调好再定（设计原话），先给合理默认。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisRainbowAlign.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDysisRainbowAligned);

UCLASS()
class DYSIS_API ADysisRainbowAlign : public AActor
{
	GENERATED_BODY()

public:
	ADysisRainbowAlign();

	/** 玩家该站的位置（虹台上、离水帘 ~1.1 m，设计 13.3）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Rainbow")
	FVector StandPointCm = FVector::ZeroVector;

	/** 人形中心（浮雕上空着的那个人形；设计：头部离地 1.4 m——以施工图为准）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Rainbow")
	FVector ReliefHeadCm = FVector::ZeroVector;

	/** 水帘/浮雕平面法线（世界系，归一；竖井朝中庭一面=80° 方位）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Rainbow")
	FVector PlaneNormal = FVector(1, 0, 0);

	/** 影头住进人形的判定半径（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Rainbow")
	double AlignToleranceCm = 40.0;

	/** 站位生效半径（玩家离 StandPoint 多少内才算"站在虹台上"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Rainbow")
	double StandRadiusCm = 120.0;

	/** 触发过一次后还能再触发吗（默认锁存——虹醒了就一直醒到龛开；调试可开重触发）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Rainbow")
	bool bReArmable = false;

	/** 影头住进人形的那一刻广播（醒虹：虹桥铺设、虹之龛解锁挂这）。 */
	UPROPERTY(BlueprintAssignable, Category = "Dysis|Rainbow")
	FDysisRainbowAligned OnAligned;

	/** 当前影头位置（调试/材质圆心共用——彩虹渲染的圆心正是它，§8.8）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Rainbow")
	FVector GetShadowHeadCm() const { return ShadowHeadCm; }

	/** 当前是否对齐中。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Rainbow")
	bool IsAligned() const { return bAligned; }

protected:
	virtual void Tick(float DeltaTime) override;

	FVector ShadowHeadCm = FVector::ZeroVector;
	bool bAligned = false;
	bool bFired = false;   // 锁存（bReArmable=false 时触发一次即止）

	class ADysisSkyActor* ResolveSky() const;
	mutable TWeakObjectPtr<class ADysisSkyActor> CachedSky;
};
