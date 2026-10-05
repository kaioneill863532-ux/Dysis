// 日落回廊 · 推拉石板（日2）：贴着外墙沿切向滑的弧线滑动，订阅拉杆的 OnToggled 换向。
// 机关清单规格（b2 石板）：中心在半径 17.20、高 19.85，方位 245.72°（挡住窗）↔ 254.27°（让开窗），
// 开局在 245.72°；拉杆 A 拉下后滑开，拉杆 B 推回。iris 石板同理（171.43°↔179.78°，方向相反）。
// 用法：本 Actor 不用网格，摆进关卡后把 TargetMesh 指到面板的 StaticMeshActor（须 Movable——导入脚本已设），
// Lever 指到 ADysisLeverActor；bReversePull = 拉杆"拉下"时滑向哪一头。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisSliderActor.generated.h"

class ADysisLeverActor;

UCLASS()
class DYSIS_API ADysisSliderActor : public AActor
{
	GENERATED_BODY()

public:
	ADysisSliderActor();

	/** 要滑动的那块面板（关卡里的 StaticMeshActor，须 Movable）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	TObjectPtr<AActor> TargetMesh;

	/** 监听的拉杆。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	TObjectPtr<ADysisLeverActor> Lever;

	/** 滑动弧线：起始方位角（度，北 0 顺时针）——"挡住窗"的一头（机关清单）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double AzStart = 245.72;

	/** 滑动弧线：终点方位角——"让开窗"的一头。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double AzEnd = 254.27;

	/** 弧线半径与高度（米，施工图口径；换算见 Optics::AzRyToCm）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double RadiusM = 17.20;
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double HeightM = 19.85;

	/** 滑动角速度（度/秒，石板的重手感）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double DegPerSec = 6.0;

	/** true = 拉杆拉下时滑到 AzEnd（b2 的语义）；false = 反向（iris 石板：拉杆 A 拉下反而滑来挡窗）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	bool bGoToEndWhenPulled = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 拉杆扳动的回调（BeginPlay 里 AddDynamic 绑定，须 UFUNCTION）。 */
	UFUNCTION()
	void OnLeverToggled(bool bPulled);

private:
	/** 当前方位角（度）。 */
	double CurrentAz = 0.0;
	double TargetAz = 0.0;
};
