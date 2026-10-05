// 日落回廊 · 移动组件三合一（调研 §15.2）：光路携带（平台写人，不走引擎 Base 推挤）、
// 虚拟地面（水面/夜光石亮格才有地）、coyote 的"还站得住"半边。替换方式只能构造期
// （DysisCharacter 构造函数 SetDefaultSubobjectClass，BeginPlay 换会留下不一致的移动子系统，§8.1）。
// coyote 的"还能跳"半边在 ADysisCharacter 上（Falling/CanJumpInternal 是 ACharacter 的虚函数，§8.1）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Beams/DysisBeamActor.h"
#include "DysisCharacterMovement.generated.h"

class IDysisVirtualSurface;
class UDysisTimeComponent;

UCLASS()
class DYSIS_API UDysisCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDysisCharacterMovement();

	/** 光路塌缩后碰撞再保留一会（灰盒：刚踏空 0.14 s 里还站得住）。跳的宽容在 Character 的 coyote 里。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BeamCoyoteSeconds = 0.14f;

	/** §7 一次掉下超过这个高度（厘米）→ 回上一个安全落脚点（灰盒 6.5 m = 650 cm）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Movement")
	float FallRespawnThresholdCm = 650.0f;

	/** §7 水面高度（厘米）：掉落中脚下 Z < 此值且无虚拟面 → 立即回档（"掉进水池、海里就回到上一个安全落脚点"）。
	 *  默认 -45 = 水庭水面 y=-0.45m；海面在此以下（-16m），同一个门就够。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Movement")
	float WaterLevelCm = -45.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** 脚底世界坐标（胶囊中心 − 半高；和 DysisTimeComponent::ComputeFoot 同口径）。 */
	FVector FootCm() const;

	/** 站上/离开光时由 Tick 的地面判定驱动；调用方不需要管。 */
	void PinToBeam(ADysisBeamActor* Beam);
	void UnpinBeam();

	/** §4 光被挡住时人退回原位（规格书原话）：PinToBeam 那一刻记录脚下位置；
	 *  beam 变不可走（IsWalkableNow()=false）时传送回去。 */
	void RetreatFromBeam();

	/** §7 记录安全落脚点（在稳固地面上站稳时调）；光上/月石/桥上不记（规格书 §7 原话）。 */
	void RecordFoothold();
	/** §7 掉落超限 → 传送回上一个安全落脚点。 */
	void RespawnAtFoothold();

private:
	/** 本帧 Super 之后处理：①地面是虚拟面且不亮 → Falling；②虚拟面回写；③光路钉人携带；④回档。 */
	void HandleDysisFloors(float DeltaTime);

	UDysisTimeComponent* ResolveTime();

	TWeakObjectPtr<ADysisBeamActor> StandingBeam;
	FDysisBeamParam BeamPin;
	FVector PrePinFootCm = FVector::ZeroVector;   // §4 站上光那一刻的脚位（退回原位用）
	TWeakObjectPtr<AActor> StandingSurface;
	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
	bool bZoneFromSurface = false;

	// ── §7 回档 ──
	FVector LastSafeFootholdCm = FVector::ZeroVector;   // 上一个安全落脚点
	float FallStartZ = 0.0f;                             // 本次掉落的起始 Z
	bool bFalling = false;                               // 是否在掉落中
	int32 FramesOnGround = 0;                            // 稳固落地帧计数（≥3 帧才记落脚点）
};
