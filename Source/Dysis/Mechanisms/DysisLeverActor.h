// 日落回廊 · 拉杆（日2）：扳动后广播 OnToggled，推拉石板等机关订阅。轴心/角度来自机关清单：
//   SM_Mech_LeverA_Arm 绕底座上沿的水平轴（本地 X）扳 −0.5 → +0.5 弧度；拉杆 A 拉下 = 上升的窗打开、虹的窗关上。
// 摆放：本 Actor 放到拉杆臂的轴心（机关清单"轴心 UE"列），扳动转的是本 Actor 的根——
// 关卡里已有独立网格时把网格设 Movable 并由美术改为挂在本 Actor 下，或直接把本 Actor 当容器。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mechanisms/DysisInteractable.h"
#include "DysisLeverActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDysisLeverToggled, bool, bPulled);

UCLASS()
class DYSIS_API ADysisLeverActor : public AActor, public IDysisInteractable
{
	GENERATED_BODY()

public:
	ADysisLeverActor();

	/** 扳动角度（弧度，机关清单：±0.5）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	float SwingAngleRad = 0.5f;

	/** 扳动角速度（度/秒，石头机关的"重手感"）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	float SwingDegPerSec = 180.0f;

	/** 扳动绕的本地轴（机关清单：物体本地 X）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	FVector SwingAxis = FVector(1, 0, 0);

	/** 开局是否已拉下。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	bool bStartPulled = false;

	/** 拉下/推回的瞬间广播（石板等订阅；灰盒：拉杆 A 拉下 → b2 滑开、iris 滑来）。 */
	UPROPERTY(BlueprintAssignable, Category = "Dysis|Mech")
	FDysisLeverToggled OnToggled;

	/** 水闸语义（可选）：非空 = 这根拉杆是水闸/进水口，拉下时把该名字写进存档（MarkSluice+自动档）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	FName SluiceName;

	/** 拉下时把水面切到这个态（设计 §5：日1 开水闸→流动起雾；月4 关水闸→静成镜）。None=不切水面。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	FName WaterStateOnChange;   // "FlowOut" / "Calm" / "Ripple"，留空不切

	/** 拉下时是否写自动档（水闸是单向节点，默认写——§14.2 单调原则）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Mech")
	bool bAutosaveOnPull = true;

	// ── IDysisInteractable ──
	virtual void Interact(APawn* Player, bool bFromFront = true) override;
	virtual FText GetInteractPrompt() const override;   // 文案表：水闸→"打开水闸"，机关→"转动雕像"

	/** 当前是否处于拉下状态。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Mech")
	bool IsPulled() const { return bPulled; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	bool bPulled = false;
	float CurrentAngleRad = 0.0f;    // 拉下方向为正
	FQuat BaseQuat = FQuat::Identity;
};
