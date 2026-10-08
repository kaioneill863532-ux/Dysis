// 日落回廊 · 互动：挂在玩家身上。按灰盒的做法，不用准星对着——走到机关旁边就能按 E，
// 同时够得着几个时取最近的一个（灰盒 nearestInteract：水平距离 < 半径，且脚的高度在范围里）。
// 先问机关总管（ADysisDirector，照灰盒重做过的机关都在那）；它那里没有，再看关卡里还没重做的旧机关
// （实现了 IDysisInteractable 的 Actor，半径和高度范围用下面的默认值）。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DysisInteractComponent.generated.h"

class APawn;
class AActor;
class ADysisDirector;

UCLASS(ClassGroup = (Dysis), meta = (BlueprintSpawnableComponent))
class DYSIS_API UDysisInteractComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDysisInteractComponent();

	/** 旧机关：够得着的水平距离（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float ReachCm = 180.0f;

	/** 旧机关：脚比机关的原点低多少、高多少以内算同一层（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float BelowCm = 230.0f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Interact")
	float AboveCm = 80.0f;

	/** E 键按下时由 Character 调。返回是否真的做了什么。 */
	bool TryInteract();

	/** 现在够得着的互动点的提示文字（界面上“E　……”那一行用）。没有就返回 false。 */
	bool GetPrompt(FText& OutPrompt) const;

	/** 同上，再给出提示该浮在世界里的哪一点（互动点旁边）。 */
	bool GetPromptAt(FText& OutPrompt, FVector& OutAnchorCm) const;

	/** 现在够得着哪个旧机关。 */
	bool CanInteractNow(AActor*& OutTarget) const;

private:
	void RefreshCandidates() const;
	FVector FootCm() const;
	ADysisDirector* Director() const;

	mutable TArray<TWeakObjectPtr<AActor>> Candidates;
	mutable double NextRefreshTime = -1.0;
	mutable TWeakObjectPtr<ADysisDirector> CachedDirector;
};
