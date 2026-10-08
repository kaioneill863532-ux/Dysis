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

	/** 一次掉下超过这个高度（厘米），落地时回上一个落脚点（灰盒 6.5 m）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Movement")
	float FallRespawnThresholdCm = 650.0f;

	/** 最快下落速度（厘米 / 秒；灰盒 30 m/s）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Movement")
	float MaxFallSpeedCm = 3000.0f;

	/** 回落脚点时画面黑下去 / 亮起来各用多久（秒；灰盒 0.38）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Movement")
	float RespawnFadeSeconds = 0.38f;

	/** 回上一个落脚点（R 键，或者掉得太深、掉进水里海里时自动触发）：画面黑一下，人回去。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Movement")
	void RespawnNow();

	/** 正在回落脚点的黑场里（这期间人不动）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Movement")
	bool IsRespawning() const { return bRespawning; }

	/** 一共回过几次落脚点（测试用）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Movement")
	int32 RespawnCount = 0;

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

private:
	/** 落脚点和回档（灰盒 updatePlayer 的后半段 + respawn）。 */
	void UpdateRespawn(float DeltaTime);
	/** 脚下这块地能不能记成落脚点：光上、月石上、几座桥上不记。 */
	bool IsOnSafeGround();
	void FinishRespawn();

	/** 本帧 Super 之后处理：①地面是虚拟面且不亮 → Falling；②虚拟面回写；③光路钉人携带；④回档。 */
	void HandleDysisFloors(float DeltaTime);

	UDysisTimeComponent* ResolveTime();

	TWeakObjectPtr<ADysisBeamActor> StandingBeam;
	FDysisBeamParam BeamPin;
	FVector PrePinFootCm = FVector::ZeroVector;   // §4 站上光那一刻的脚位（退回原位用）
	TWeakObjectPtr<AActor> StandingSurface;
	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
	bool bZoneFromSurface = false;

	// ── 落脚点和回档 ──
	FVector LastSafeFootholdCm = FVector::ZeroVector;   // 上一个落脚点（脚底的位置）
	bool bHasFoothold = false;
	float SafeSeconds = 0.0f;                            // 站在地上累计的时间（灰盒 safeT：每满 0.4 s 记一次落脚点）
	bool bAirborne = false;                              // 离了地（跳起或踏空）
	float FallFromZ = 0.0f;                              // 离地那一刻脚的高度
	bool bRespawning = false;
	float RespawnTimer = 0.0f;
};
