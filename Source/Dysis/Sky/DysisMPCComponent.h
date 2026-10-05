// 日落回廊 · 时间→材质全局参数桥（M6 先行件，调研 §2.5/§8.6/§14.1）：
// 订阅玩家时间组件的 OnTimeChanged，把 H/昼夜/主光强度/日月方向写进 Material Parameter Collection
// （美术建 MPC_Dysis 资产后指给 Collection 即生效）——踏片亮灭、水面、月光大道 sparkle、
// 夜光石磷光、彩虹显隐全场景材质共用这一个入口，替代逐 MID 传参。
// 没指 Collection 时静默禁用（一次性 Warning），纯逻辑玩法不受影响。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DysisMPCComponent.generated.h"

class UMaterialParameterCollection;
class ADysisSkyActor;
class UDysisTimeComponent;
class ADysisLuminousStone;

UCLASS(ClassGroup = (Dysis), meta = (BlueprintSpawnableComponent))
class DYSIS_API UDysisMPCComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDysisMPCComponent();

	/** 美术建的 MPC 资产（建议名 MPC_Dysis）。参数名见下方各 UPROPERTY。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	TObjectPtr<UMaterialParameterCollection> Collection;

	/** 参数名（和 MPC 资产里的名字一致，改一处要同步另一处）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	FName HParam = TEXT("H");
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	FName NightParam = TEXT("bNight");
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	FName MainLightParam = TEXT("MainLightIntensity");   // 灰盒亮度（未乘换算系数），调试/材质共用
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	FName SunDirParam = TEXT("SunDir");                  // 向量：水面 sparkle/彩虹圆心共用（§12.4）
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	FName MoonDirParam = TEXT("MoonDir");
	UPROPERTY(EditAnywhere, Category = "Dysis|MPC")
	FName MistParam = TEXT("Mist");                      // 水闸状态 → 光束可见度（§15.1 注）

	/** 雾浓度 0–1（日1 开水闸→1；夜4 关闸→0）。程序/机关侧随时 SetMist。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|MPC")
	void SetMist(float InMist);

	virtual void BeginPlay() override;

private:
	/** OnTimeChanged 的处理器（AddDynamic 要求 UFUNCTION）。 */
	UFUNCTION()
	void HandleTimeChanged(float H);

	/** 找玩家时间组件 + 关卡天（懒缓存）。 */
	UDysisTimeComponent* ResolveTime();
	ADysisSkyActor* ResolveSky();

	void WriteAll(float H);

	TWeakObjectPtr<UDysisTimeComponent> CachedTime;
	TWeakObjectPtr<ADysisSkyActor> CachedSky;
	float Mist = 0.0f;
	bool bWarnedNoCollection = false;
};
