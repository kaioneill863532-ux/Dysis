// 日落回廊 · 接缝空气墙（P1-7，调研 §15.6）：挡住水钟竖井筒段，不让玩家同层跨过时间接缝
// （灰盒：SEAM 方位 116°–140° 每层首尾相接，竖井正好截成"C"形）。规格来自 generated.h：
//   SEAM=[116°,140°]、墙内半径 R_IN=15.5、墙外 17.1（README 墙体规格）、墙底 −0.6 ~ 墙顶 31.3。
// 摆进关卡即生效（BeginPlay 自动算位置尺寸），要临时关掉用 bEnabled。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisSeamWall.generated.h"

class UBoxComponent;

UCLASS()
class DYSIS_API ADysisSeamWall : public AActor
{
	GENERATED_BODY()

public:
	ADysisSeamWall();

	/** 接缝起止方位角（度）。默认来自 generated.h 的 SEAM。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	double AzStart = 116.0;

	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	double AzEnd = 140.0;

	/** 墙带内外半径（米）。默认=墙体规格 15.5–17.1。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	double RadiusInnerM = 15.5;
	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	double RadiusOuterM = 17.1;

	/** 墙底/墙顶（米，施工图 y）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	double BottomY = -0.6;
	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	double TopY = 31.3;

	/** 摆不摆这堵墙（调试用）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Wall")
	bool bEnabled = true;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	/** 按当前参数把碰撞盒摆到弧带中点。 */
	void LayoutWall();

	UPROPERTY(VisibleAnywhere, Category = "Dysis|Wall")
	TObjectPtr<UBoxComponent> Wall;
};
