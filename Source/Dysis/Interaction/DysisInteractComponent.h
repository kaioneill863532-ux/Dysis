// 日落回廊 · 摸一下交互组件：挂在玩家 Character 上，E 键时从相机做一条屏幕中心射线（≤2.5 m），
// 打到实现 IDysisInteractable 的 Actor 就调它的 Interact（调研 §2.6/§16.3-①，输入零资产）。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DysisInteractComponent.generated.h"

class APawn;
class AActor;

UCLASS(ClassGroup = (Dysis), meta = (BlueprintSpawnableComponent))
class DYSIS_API UDysisInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDysisInteractComponent();

	/** 射线最远距离（厘米）。"摸一下"的手长。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float ReachCm = 250.0f;

	/** E 键按下时由 Character 调（绑定见 DysisCharacter.cpp）。返回是否真的摸到了东西。 */
	bool TryInteract();

	/** 当前是否正对着可交互物（给将来的指尖光点提示用；v1 调试）。 */
	bool CanInteractNow(AActor*& OutTarget) const;

protected:
	/** 从玩家相机做屏幕中心射线。 */
	bool TraceFromCamera(FHitResult& OutHit) const;
};
