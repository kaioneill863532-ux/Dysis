#include "DysisMPCFactory.h"
#include "Materials/MaterialParameterCollection.h"

UMaterialParameterCollection* UDysisMPCFactory::CreateDysisMPC(UObject* Outer)
{
	if (!Outer) return nullptr;

	UMaterialParameterCollection* MPC = NewObject<UMaterialParameterCollection>(Outer, TEXT("MPC_Dysis_Runtime"));
	if (!MPC) return nullptr;

	// 标量参数——名字和 DysisMPCComponent 的 UPROPERTY 一一对应，材质图按名引用。
	// 反射注册：FCollectionScalarParameter 直接 push 到数组（引擎内部格式，不需要编辑器序列化）。
	auto AddScalar = [&MPC](const FName& Name, float DefaultValue = 0.0f)
	{
		FCollectionScalarParameter P;
		P.ParameterName = Name;
		P.DefaultValue = DefaultValue;
		MPC->ScalarParameters.Add(P);
	};
	auto AddVector = [&MPC](const FName& Name, const FLinearColor& DefaultValue = FLinearColor::Black)
	{
		FCollectionVectorParameter P;
		P.ParameterName = Name;
		P.DefaultValue = DefaultValue;
		MPC->VectorParameters.Add(P);
	};

	// ── 时间/天象（DysisMPCComponent 每帧写）──
	AddScalar(TEXT("H"), 22.369624f);              // 当前时角（度）——钟点=12:00+H/15
	AddScalar(TEXT("bNight"), 0.0f);                // 0=白天 / 1=夜里
	AddScalar(TEXT("MainLightIntensity"), 3.0f);    // 灰盒亮度（未乘 Lux 系数）

	// ── 水系统（LeverActor 拉闸时写）──
	AddScalar(TEXT("Mist"), 0.0f);                  // 雾浓度 0-1（水闸开着=1）
	AddScalar(TEXT("Ripple"), 0.0f);                // 水面波纹态 0-1
	AddScalar(TEXT("Flow"), 0.0f);                  // 水面流动/瀑布态 0-1

	// ── 屋顶光圈（BeamActor 圆眼检测时写 / RoofSteps 张开联动）──
	AddScalar(TEXT("OpenT"), 0.0f);                 // 光圈张开度 0-1（0=合拢 / 1=全开）

	// ── 光学表现（RainbowAlign/PrismActor 触发时写）──
	AddScalar(TEXT("RainbowOn"), 0.0f);             // 彩虹显示 0-1（虹台对齐后=1）

	// ── 道具（LuminousStone / GoldenApple 写）──
	AddScalar(TEXT("Glow"), 0.0f);                  // 夜光石磷光亮度
	AddScalar(TEXT("AppleGlow"), 0.0f);             // 金苹果光照半径因子

	// ── 方向向量（DysisMPCComponent 每帧写）──
	AddVector(TEXT("SunDir"), FLinearColor(0, 0, 1, 0));    // 太阳方向（XYZ 单位向量）
	AddVector(TEXT("MoonDir"), FLinearColor(0, 0, -1, 0));  // 月亮方向

	UE_LOG(LogTemp, Display, TEXT("DysisMPCFactory：运行时 MPC 创建成功（%d 个标量 + %d 个向量参数）"),
		MPC->ScalarParameters.Num(), MPC->VectorParameters.Num());
	return MPC;
}
