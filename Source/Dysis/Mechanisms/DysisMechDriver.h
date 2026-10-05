// 日落回廊 · 通用机关驱动器：把 10 个缺失机关一次收进一个类（机关清单"怎么动"列全是同一套范式——
// 条件触发→恒速动画→到位休眠）。每个实例配一个机关，参数全 UPROPERTY。
// 机关清单逐机关映射（轴心/朝向/挂在 全在各机关行内，本类只管运动学）：
//   月2天鹅：像+天鹅绕竖轴 45°/格（同拉杆扳转但绕 Z）→ 门沉 2.6m
//   月2月石：反射月光照到 → 材质透明（不动物理网格，SetActorEnableCollision(false)+SetActorHiddenInGame(true)）
//   月2月锁：银月被反射月光照到 → 刹杆松（同月石，但目标是竖井铜链解锁）
//   月3闸轮：转 → 打开竖井闸（同拉杆）
//   月4双子：波吕丢刻斯拉出+推回（两段直线滑动）
//   月4月桥：双子合拢后显形（同月石但反向：从隐藏到显示+NoCollision→有碰撞）
//   月5半桥：女神亮后从池沿伸出（沿半径直线滑动）
//   封门石板×2：上门开后往下沉（Z 直线滑动）
//   外窗石块×5：沿半径往外推后消失（径向滑动+隐藏）
//   光圈叶片：跟踏步升降（由 RoofSteps 驱动——不用本类，叶片有专属 MPC 驱动，见 §15.4）
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mechanisms/DysisInteractable.h"
#include "DysisMechDriver.generated.h"

class ADysisLeverActor;
class ADysisMirrorSource;
class UDysisTimeComponent;

/** 运动类型（机关清单"怎么动"列的六类收敛为三）。 */
UENUM()
enum class EDysisMechMotion : uint8
{
	SlideWorld   UMETA(DisplayName = "直线滑动（世界坐标：起点→终点）"),
	RotateLocal  UMETA(DisplayName = "绕本地轴旋转（起始角→终止角）"),
	Reveal       UMETA(DisplayName = "显隐切换（隐藏→显示 / 显示→消失）"),
};

UCLASS()
class DYSIS_API ADysisMechDriver : public AActor, public IDysisInteractable
{
	GENERATED_BODY()

public:
	ADysisMechDriver();

	// ───── 运动参数（按 Motion 类型取用）─────

	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	EDysisMechMotion Motion = EDysisMechMotion::SlideWorld;

	/** 要驱动的网格 Actor（关卡里的 StaticMeshActor，须 Movable）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	TObjectPtr<AActor> TargetMesh;

	/** 直线滑动：起点/终点（世界坐标，UE 厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::SlideWorld"))
	FVector SlideFromCm = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::SlideWorld"))
	FVector SlideToCm = FVector::ZeroVector;

	/** 旋转：绕的本地轴 + 起始/终止角（度）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::RotateLocal"))
	FVector RotateAxis = FVector(0, 0, 1);
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::RotateLocal"))
	double RotateFromDeg = 0.0;
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::RotateLocal"))
	double RotateToDeg = 90.0;

	/** 恒速速度（单位/秒：滑动=厘米/秒，旋转=度/秒）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	double Speed = 30.0;

	/** 显隐：显示还是隐藏（true=从隐藏变显示；false=从显示变隐藏+无碰撞）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::Reveal"))
	bool bRevealToVisible = true;

	/** 显隐时是否启用碰撞（月桥=true 要踩；外窗石块=false 消失）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech", meta = (EditCondition = "Motion == EDysisMechMotion::Reveal"))
	bool bRevealCollision = true;

	// ───── 触发条件（三选一；全空 = 只能手动 Interact）─────

	/** 触发拉杆（拉下拉杆 → 本机关激活）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Trigger")
	TObjectPtr<class ADysisLeverActor> TriggerLever;

	/** 触发棱镜（转到此槽位 → 激活；机关清单月3"闸轮"和月2的月石/月锁用铜镜反射光触发，简化为棱镜格数）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Trigger")
	TObjectPtr<class ADysisMirrorSource> TriggerMirror;
	/** 匹配的铜镜格位（-1=任意格；月石/月锁只在铜镜转到特定格时才被"照到"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Trigger")
	int32 TriggerMirrorSlot = -1;

	/** 触发接光（入夜 → 激活；机关清单月2天鹅/月桥/半桥都挂在入夜后）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Trigger")
	bool bTriggerOnNight = false;

	// ───── §5 月石采样点（月石/月锁"被月光照到≥2/3 触发"）─────
	/** 采样半径（厘米）：在此半径内取 8 个点打月光遮挡 trace（§5"在它上面取一组采样点，逐点检查"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Moonstone")
	double SampleRadiusCm = 100.0;
	/** 月石门槛（§5"约 2/3"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Moonstone")
	double MoonstoneThreshold = 0.667;
	/** 离开月光会合回去（§5"有的离开月光会合回去"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech|Moonstone")
	bool bCloseWhenMoonlightLeaves = false;

	/** 已激活（单调——机关只进不退，§14.2）。 */
	UPROPERTY(VisibleInstanceOnly, Category = "Dysis|Mech")
	bool bActivated = false;

	// ── IDysisInteractable（手动触发兜底）──
	virtual void Interact(APawn* Player, bool bFromFront = true) override;
	virtual FText GetInteractPrompt() const override;   // 文案表：按运动类型给不同提示

	/** 拉杆回调（BeginPlay 里 AddDynamic；须 UFUNCTION）。 */
	UFUNCTION()
	void OnLeverToggled(bool bPulled);

	/** 外部直接触发（拉杆/接光编排调这个）。 */
	void Activate();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/** 运动进度 0→1（恒速）。 */
	double Alpha = 0.0;
	bool bTicked = false;   // 动画播完休眠
	FQuat BaseQuat = FQuat::Identity;   // TargetMesh 的 BeginPlay 姿态（旋转类机关从这起算）

	/** 应用当前 Alpha 到 TargetMesh。 */
	void ApplyAlpha();
	void CheckTriggers();

	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
	UDysisTimeComponent* ResolveTime();
};
