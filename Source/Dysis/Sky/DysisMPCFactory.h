// 日落回廊 · 运行时 MPC 工厂（调研 §2.5/§8.6/§14.1）：不依赖编辑器资产，在 BeginPlay 里
// 用 NewObject<UMaterialParameterCollection> + 反射注册参数（Scalar/Vector），赋给 DysisMPCComponent。
// 这样 MPC 的六个 TODO（水面材质/光圈叶片/彩虹显示/雾源/夜光石/苹果光）全部解除阻塞——
// 材质图用 Collection Parameter 节点按参数名引用（H/bNight/Mist/OpenT/Ripple/Flow/RainbowOn/Glow）。
// 美术以后在编辑器里建正式 MPC_Dysis 资产时，把 DysisMPCComponent 的 Collection 指过去即可无缝切换。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DysisMPCFactory.generated.h"

class UMaterialParameterCollection;

UCLASS()
class DYSIS_API UDysisMPCFactory : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 创建运行时 MPC，预注册本项目需要的全部标量/向量参数。
	 * 参数名 = DysisMPCComponent 的 UPROPERTY 名（H/bNight/MainLightIntensity/Mist/OpenT/Ripple/Flow/RainbowOn/Glow/AppleGlow + SunDir/MoonDir）。
	 * 调用一次即可（DysisMPCComponent 的 BeginPlay 里 Collection 为空时自动调）。
	 */
	static UMaterialParameterCollection* CreateDysisMPC(UObject* Outer);
};
