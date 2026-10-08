// 日落回廊 · 互动：挂在玩家身上。按灰盒的做法，不用准星对着——走到机关旁边就能按 E，
// 同时够得着几个时取最近的一个（灰盒 nearestInteract：水平距离 < 半径，且脚的高度在范围里）。
// 现在的候选是关卡里所有实现了 IDysisInteractable 的 Actor；机关逐个照灰盒重做以后，半径和高度范围改由各机关自己给。
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

	/** 够得着的水平距离（厘米）。灰盒各机关 0.9–2.8 m 不等，这里先取中间值。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float ReachCm = 180.0f;

	/** 脚比机关的原点低多少、高多少以内算同一层（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float BelowCm = 230.0f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float AboveCm = 80.0f;

	/** E 键按下时由 Character 调。返回是否真的摸到了东西。 */
	bool TryInteract();

	/** 现在够得着哪个机关（界面的互动提示用）。 */
	bool CanInteractNow(AActor*& OutTarget) const;

private:
	void RefreshCandidates() const;

	mutable TArray<TWeakObjectPtr<AActor>> Candidates;
	mutable double NextRefreshTime = -1.0;
};
