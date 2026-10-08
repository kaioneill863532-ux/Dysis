// 狄西斯的日落回廊 · 玩家脚步、起跳、落地、掉落、回到落脚点的声音
// 不改玩家类：ADysisSfxDirector 开局把本组件挂到玩家 Pawn 上（自己的 Character 也能直接加这个组件）。
// 没有动画，所以按“走过的距离”计步：走一步 225 cm、快走一步 300 cm（数值在 Project Settings → Dysis 音效 → 脚步）。
// 只在玩家真的在走（有移动输入）时出声——站在光上被光带着走不会响脚步。
// 脚下材质的判定顺序：光柱/夜光石/水面这些 Dysis 自己的地面 → 区域名（beam:、moonbr、gbridge……）
//   → 地面的名字（MoonBridge、Bronze……，可在设置页加规则）→ 水面高度以下 → 瀑布流着时的近处石面 → 石头。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Audio/DysisSfxSettings.h"
#include "DysisFootstepComponent.generated.h"

class ACharacter;
class UPrimitiveComponent;
class UDysisTimeComponent;
class UDysisSfxSubsystem;

UCLASS(ClassGroup = (Dysis), meta = (BlueprintSpawnableComponent))
class DYSIS_API UDysisFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDysisFootstepComponent();

	/** 当前脚下材质（调试/别的系统用）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sfx")
	EDysisFootSurface GetSurface() const { return Surface; }

	/** 瀑布是否在流、在哪（Director 每次轮询写进来；湿石判定用）。 */
	void SetWetSource(bool bFlowing, const FVector& LocationCm);

	/** 关掉脚步声（过场时用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx")
	bool bEnabled = true;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	EDysisFootSurface ClassifySurface(const ACharacter* Character) const;
	FName StepKey(EDysisFootSurface InSurface, bool bRun) const;
	FName LandKey(EDysisFootSurface InSurface) const;
	void OnTeleported(UDysisSfxSubsystem* Sfx, const FVector& Location, double Now);
	UDysisTimeComponent* ResolveTime() const;
	FString DescribeFloor(const UPrimitiveComponent* Comp) const;

	TWeakObjectPtr<ACharacter> Character;
	mutable TWeakObjectPtr<UDysisTimeComponent> CachedTime;

	FVector PrevLocation = FVector::ZeroVector;
	bool bHavePrev = false;

	float StepDistance = 0.0f;
	bool bWasMoving = false;
	float LastMoveSpeed = 0.0f;
	bool bWasOnGround = true;
	int32 LastJumpCount = 0;

	float AirSeconds = 0.0f;
	float AirTopZ = 0.0f;
	bool bFallSoundPlayed = false;
	double SuppressLandUntil = 0.0;

	EDysisFootSurface Surface = EDysisFootSurface::Stone;
	bool bWetFlowing = false;
	FVector WetLocation = FVector::ZeroVector;

	// 地面名字缓存（同一个组件不重复拼字符串）。
	mutable TWeakObjectPtr<const UPrimitiveComponent> CachedFloorComp;
	mutable FString CachedFloorNames;
};
