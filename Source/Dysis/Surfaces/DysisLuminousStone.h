// 日落回廊 · 夜光石（月4 潮沟，调研 §2.3/§15.3）：被金苹果照亮时发青白光，苹果离开后 ~14 s 慢慢暗下去
// （灰盒 13.3：约 14 秒衰减）；亮着才踩得住（Glow > WalkableGlow），暗到线下即"没有地"——
// 这正是"一块一块踩过去"的紧迫感来源（设计 6·月4：这是金苹果自己的光第一次派上用场）。
// 本 Actor 为逻辑件：摆到每块石墩位置；TargetMesh 可选（指关卡里的石墩网格，把 Glow 参数打进材质）。
// v1 触发源：AppleActor（指金苹果 Actor）或用玩家 Pawn 代替（bUsePlayerAsApple，测试用）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Optics/DysisVirtualSurface.h"
#include "DysisLuminousStone.generated.h"

class UStaticMeshComponent;

UCLASS()
class DYSIS_API ADysisLuminousStone : public AActor, public IDysisVirtualSurface
{
	GENERATED_BODY()

public:
	ADysisLuminousStone();

	/** 触发源：金苹果 Actor（拿着它的角色也行——按 Actor 位置算距离）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Stone")
	TObjectPtr<AActor> AppleActor;

	/** 没有金苹果时用玩家 Pawn 当触发源（测试）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Stone")
	bool bUsePlayerAsApple = true;

	/** 照亮半径（厘米；设计：捧着金苹果走近即亮）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Stone")
	double IlluminateRadiusCm = 300.0;

	/** 暗下去的总时长（灰盒 ~14 s）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Stone")
	float DecaySeconds = 14.0f;

	/** 踩得住的亮度阈值（0–1）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Stone")
	float WalkableGlow = 0.2f;

	/** 可选：指关卡里的石墩网格，把标量参数 Glow（0–1）打进它的材质。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Stone")
	TObjectPtr<AActor> TargetMesh;

	/** 当前亮度 0–1（调试图形/材质共用）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Stone")
	float GetGlow() const { return Glow; }

	/** 手动照亮（设 1 并开始维持；离开后自然衰减）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Stone")
	void Illuminate() { Glow = 1.0f; bLitNow = true; }

	// ── IDysisVirtualSurface ──
	virtual bool IsWalkableAt(const FVector& FootCm) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	float Glow = 0.0f;
	bool bLitNow = false;    // 触发源这一帧在半径内（维持满亮）
};
