// 日落回廊 · 金苹果（设计 §2.6/§6/§7 日月联动表）：贯穿日5→月5 的核心道具。
//   接光前：摆放在屋顶浑天仪架上（静态，金光=点光占位）。
//   接光后（bNight）：托着走——漂浮跟随玩家（FInterpTo，"带来冷暖对比，给夜里的场景一点光"），
//                     靠近夜光石触发磷光（AppleActor 指向本 Actor 即可，距离判定已就绪）。
//   月5 归亭：玩家放到底层浑天仪月托 → 变白色小月亮（材质换色 TODO 美术）→ 子夜结局。
// 不可收起（设计：不进库存、一直在画面里）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mechanisms/DysisInteractable.h"
#include "DysisGoldenApple.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UDysisTimeComponent;
class IDysisInteractable;

/** 三态（设计 §6 日5/月5 流程）。 */
UENUM()
enum class EAppleState : uint8
{
	OnArmillary UMETA(DisplayName = "架上（接光前）"),
	Carried     UMETA(DisplayName = "托着走"),
	Placed      UMETA(DisplayName = "归亭（结局）"),
};

UCLASS()
class DYSIS_API ADysisGoldenApple : public AActor, public IDysisInteractable
{
	GENERATED_BODY()

public:
	ADysisGoldenApple();

	/** 三态（设计 §6 日5/月5 流程）。 */
	UPROPERTY(VisibleInstanceOnly, Category = "Dysis|Apple")
	EAppleState State = EAppleState::OnArmillary;

	/** 托着走时的目标偏移（相对玩家，厘米：头侧前方漂浮）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Apple")
	FVector CarryOffsetCm = FVector(50, 30, 60);

	/** 漂浮跟随速度（FInterpTo 速率；越高跟得越紧）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Apple")
	float FollowSpeed = 8.0f;

	/** 漂浮上下浮动幅度与周期（厘米/秒）——"浮在身边"的呼吸感。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Apple")
	float BobAmplitudeCm = 5.0f;
	UPROPERTY(EditAnywhere, Category = "Dysis|Apple")
	float BobPeriodSec = 3.0f;

	/** 金光/月光点光强度（占位视觉；正式版换材质+Niagara）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Apple")
	float LightIntensity = 3000.0f;

	/** 拿起（接光时刻调；也可由 CatchLight 广播接）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Apple")
	void PickUp();

	/** 放下（月5 放到底层浑天仪；调用方传月托位置）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Apple")
	void Place(FVector TargetCm);

	/** 是否在被托着（夜光石的 AppleActor 距离判定用）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Apple")
	bool IsCarried() const { return State == EAppleState::Carried; }

	// ── IDysisInteractable ──
	virtual void Interact(APawn* Player, bool bFromFront = true) override;
	virtual FText GetInteractPrompt() const override;

protected:
	virtual void Tick(float DeltaTime) override;

	UDysisTimeComponent* ResolveTime();

	UPROPERTY(VisibleAnywhere, Category = "Dysis|Apple")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Dysis|Apple")
	TObjectPtr<UPointLightComponent> Glow;

	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
	float BobPhase = 0.0f;
};
