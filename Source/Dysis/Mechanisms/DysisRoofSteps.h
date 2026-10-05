// 日落回廊 · 屋顶升降踏步驱动（M4，机关清单"屋顶 升降踏步"全节）：
//   上行 16 级：人从细桥落脚（227.65°）顺时针走，16 级一起往上升——第 k 级面高 30.3+0.45k（米），
//              升起比例 t = smoothstep(0, 162.178°(UP_SPAN), 人顺时针走过的角度)；柱身从天花(29.5)接到踏面底，
//              踏面升多少它就沿 Z 拉长多少（轴心在底面，缩放即长高）。
//   下行 16 级：入夜后整块降下来成为回 L3 的阶梯，逐级目标面高来自机关清单（29.85 … 23.00）。
//   注意：导入姿态是"升起的"——BeginPlay 先把上行 16 级归零到 30.30 平面（UE实现说明 已知坑①）。
// 找部件：按关卡里 Actor 名含 SM_Mech_RoofSteps_UpNN/DnNN 匹配（找不到打日志，别静默）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisRoofSteps.generated.h"

class AStaticMeshActor;
class UDysisTimeComponent;

UCLASS()
class DYSIS_API ADysisRoofSteps : public AActor
{
	GENERATED_BODY()

public:
	ADysisRoofSteps();

	/** 细桥落脚方位角（度，generated.h LAND[1]——ConstTable 未暴露数组，取常数并注释来源）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Roof")
	double LandAzDeg = 227.654996;

	/** 升降速度（面高 cm/s，降/升都恒速过渡，手感同机关的"石头感"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Roof")
	double MoveSpeedCmPerSec = 60.0;

	/** 入夜后多久开始降下行梯（秒；"接光后回到桥头"的宽限近似，v1 用固定延迟）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Roof")
	double NightDropDelaySeconds = 2.0;

	/** 下行逐级目标面高（米，机关清单原值：Dn01…Dn16）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Roof")
	TArray<double> DnTargetY = { 29.85, 29.39, 28.94, 28.49, 28.03, 27.58, 27.13, 26.68,
	                            26.22, 25.77, 25.32, 24.86, 24.41, 23.95, 23.48, 23.00 };

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/** 收集关卡里的 32 级踏面 + 16 根上行柱身，记录原始变换。 */
	void GatherParts();

	/** 上行联动：白天按玩家方位角给目标；夜里锁全开。 */
	void DriveUp(float DeltaTime);
	/** 下行：入夜（延迟后）向逐级目标面高降。 */
	void DriveDown(float DeltaTime);

	/** 把一个部件的面高平滑挪到目标（只动 Z；记 origin 供柱身换算）。 */
	void MoveStepTo(AStaticMeshActor* Step, double TargetY_M, float DeltaTime, double& CurrentY_M);

	struct FPart
	{
		TWeakObjectPtr<AStaticMeshActor> Actor;
		FTransform Orig;          // BeginPlay 原始（导入=升起姿态）
		double CurrentY_M = 30.3; // 当前面高（米）——平滑的"现在值"
	};

	UDysisTimeComponent* ResolveTime();

	TArray<FPart> UpSteps;    // 16
	TArray<FPart> UpShafts;   // 16（柱身，与 UpSteps 同下标）
	TArray<FPart> DnSteps;    // 16
	double NightTimer = -1.0; // 入夜倒计时（<0 = 未触发）
};
