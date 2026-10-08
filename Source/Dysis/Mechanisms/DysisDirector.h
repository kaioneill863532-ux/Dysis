// 日落回廊 · 机关总管：把灰盒 v0.12 里各个机关的逻辑按原样搬过来（一个机关对应灰盒里的一个 update 函数），
// 关卡里的机关部件（模型里 SM_Mech_… 那些）由它来挪。GameMode 开局生成一个。
// 现在管着：水闸和瀑布、L1 的机关（原来 A、B 两个，已经合成一个）和两块推拉石板。别的机关做到哪搬到哪。
//
// 互动也在这里（灰盒 INTERACTS）：每个互动点有位置、够得着的水平距离、脚的高度范围、什么时候能用、提示文字、按下去做什么；
// 人走到旁边按 E，同时够得着几个时取最近的。提示文字和机关反馈按文案表（UI/DysisCopy.h）。
// 找部件靠 Tag：关卡里每个模型部件都带一个和自己名字一样的 Tag（Art/Models/temple-v0.12/ue_prepare_mechanisms.py 打的）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisDirector.generated.h"

class UStaticMeshComponent;

/** 一个互动点（灰盒 INTERACTS 的一行）。 */
struct FDysisInteract
{
	FName Id;
	FVector PosCm = FVector::ZeroVector;   // 只用水平位置
	float Z0 = 0.0f, Z1 = 0.0f;            // 脚的高度范围（厘米）
	float RadiusCm = 160.0f;               // 水平距离
	TFunction<bool()> When;                // 空 = 一直能用
	TFunction<FText()> Label;
	TFunction<void()> Act;
};

UCLASS()
class DYSIS_API ADysisDirector : public AActor
{
	GENERATED_BODY()

public:
	ADysisDirector();

	static ADysisDirector* Get(const UObject* WorldContext);

	/** 关卡里叫这个名字的模型部件（没有就是空）。 */
	AActor* Piece(FName Label) const;

	// ───── 互动 ─────

	/** 脚站在这里时够得着的互动点里最近的一个（没有就是空）。 */
	const FDysisInteract* NearestInteract(const FVector& FootCm) const;

	/** 按 E：做最近那个互动点的事。返回有没有做。 */
	bool Interact(const FVector& FootCm);

	// ───── 状态（灰盒里同名的量） ─────

	/** 两块推拉石板：0 = 开局（西边“上升的窗”关着、南边“虹的窗”开着），1 = 反过来。1.6 秒滑到位。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float SliderPos = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	float SliderTarget = 0.0f;

	/** 机关拉过几次（文案：单数次、双数次的反馈不一样）。 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dysis|Director")
	int32 LeverPulls = 0;

	/** 测试用：按名字触发一个互动点（"sluice"、"lever"……），不管人站在哪。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Director")
	bool DebugInteract(FName Id);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	void CollectPieces();
	void BuildInteracts();

	// 水闸、瀑布
	void ToggleSluice();
	void UpdateWaterfall();
	// 机关和推拉石板（灰盒 pullLever / updateSlider / placeSlider）
	void PullLever();
	void UpdateSlider(float Dt);
	void PlaceSlider();

	TMap<FName, TWeakObjectPtr<AActor>> Pieces;
	TArray<FDysisInteract> Interacts;

	struct FSliderPanel { TWeakObjectPtr<AActor> Actor; float Az = 0.0f, Shift = 0.0f, Z = 0.0f; bool bOpenAt = false; float Yaw0 = 0.0f, Az0 = 0.0f; };
	TArray<FSliderPanel> Panels;
	TWeakObjectPtr<AActor> LeverArm;
	FQuat LeverArmBase = FQuat::Identity;
	FVector LeverAxis = FVector::RightVector;

	/** 水闸那里现在没有模型，先立一根小柱子标出位置。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Director")
	TObjectPtr<UStaticMeshComponent> SluiceMarker;
};
