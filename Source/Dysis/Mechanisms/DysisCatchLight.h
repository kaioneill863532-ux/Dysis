// 日落回廊 · 接光触发器（日5 高潮，调研 §2.6）：玩家站上接光台（最高一级，CATCH_AZ 方位），
// 太阳落到苹果高度以下（SlopeDeg 近零=光路放平、岛影漫顶）的那一刻 → SetNight(true)：天空交给月亮。
// 设计 6·日5："举起来的苹果还在光里。接住最后一缕阳光"。触发后的编排（桥门关、下行梯解锁）
// 广播 OnCaught，由 RoofSteps（已订阅 bNight）等系统各自响应。
// v1 判定：玩家在接光台半径内 && 太阳高度角 < CatchAltDeg（默认 0.5°，调参口）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisCatchLight.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDysisLightCaught);

UCLASS()
class DYSIS_API ADysisCatchLight : public AActor
{
	GENERATED_BODY()

public:
	ADysisCatchLight();

	/** 接光台中心（UE 厘米；机关清单：屋顶浑天仪 SM_Mech_Armillary_Top 摆在最高一级上，跟随踏步动——取开局位置即可，判定只看 XY）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Catch")
	FVector PlatformCm = FVector::ZeroVector;

	/** 站上接光台的判定半径（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Catch")
	double StandRadiusCm = 200.0;

	/** 太阳高度角低于此值才可接光（度；设计：岛影刚好漫过苹果=最后一缕。默认 0.5°=贴海平线，调参口）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Catch")
	double CatchAltDeg = 0.5;

	/** 接住光的那一刻广播（桥门关/和声切夜组挂这；下行梯由 RoofSteps 的 bNight 自动接）。 */
	UPROPERTY(BlueprintAssignable, Category = "Dysis|Catch")
	FDysisLightCaught OnCaught;

	/** 关卡里的金苹果（接光那刻自动 PickUp——苹果的光从这一刻开始陪你走夜路）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Catch")
	TObjectPtr<class ADysisGoldenApple> Apple;

	/** 桥门网格 Actor 名（机关清单：SM_Mech_RoofBridgeDoor_Leaf，"接住最后一缕光以后关上"；空=不找）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Catch")
	FString BridgeDoorName = TEXT("RoofBridgeDoor_Leaf");

protected:
	virtual void Tick(float DeltaTime) override;

	class ADysisSkyActor* ResolveSky() const;
	class UDysisTimeComponent* ResolveTime() const;

	bool bCaught = false;   // 只接一次（§14.2 单调）
	mutable TWeakObjectPtr<class ADysisSkyActor> CachedSky;
	mutable TWeakObjectPtr<class UDysisTimeComponent> CachedTime;
};
