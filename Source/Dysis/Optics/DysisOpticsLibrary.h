// 日落回廊 · 光学纯函数：反射/折射/彩虹判定 + 施工图坐标换算（调研 §8.8）。全部无状态，配单测（DysisOpticsTests.cpp）。
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DysisOpticsLibrary.generated.h"

/**
 * 灰盒对应：
 *  - 反射 = docs 光路的 mirror 反射（R = D − 2(D·N)N，引擎 FVector::MirrorByVector 同式）；
 *  - 折射 = 棱镜两侧各折一次（Snell 向量式，n 红 1.600 → 紫 1.642，铅水晶）；
 *  - 影头点 = 彩虹 42° 的圆心（玩家影子的头 = 相机沿 −SunDir 投到水帘平面）。
 */
UCLASS()
class DYSIS_API UDysisOpticsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 施工图 (方位角 az°北0顺时针, 半径 r, 高 y) 米 → UE 厘米（机关清单坐标换算原话：X=100·r·cos az…）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static FVector AzRyToCm(double AzDeg, double R, double Y);

	/** 镜面反射方向：R = D − 2(D·N)N。N 必须归一；D、N、R 都是"光前进方向"。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static FVector ReflectDir(FVector D, FVector N);

	/**
	 * Snell 折射（GLSL refract 同式）：D 为入射光前进方向（指向界面、归一），N 为界面法线（与 D 相向、归一），
	 * Eta = n入射/n折射（空气→玻璃 0.625）。发生全内反射返回 false。OutT 为折射后的前进方向（归一）。
	 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static bool RefractDir(FVector D, FVector N, double Eta, FVector& OutT);

	/** 影头点：相机沿 −SunDir 投到平面（PlanePoint/PlaneNormal，法线归一）。平面接近平行视线时返回相机位置。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static FVector ShadowHeadPoint(FVector CameraCm, FVector SunDir, FVector PlanePoint, FVector PlaneNormal);

	/** 彩虹判定：FromHeadCm（影头）看向 ToPointCm 的方向，与太阳反方向的夹角是否落在 42°±带宽。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static bool IsRainbowDir(FVector FromHeadCm, FVector ToPointCm, FVector SunDir, double BandDeg = 4.0);

	/** 棱镜七色折射率 n(λ)：ColorIndex 0=红 … 6=紫，红 1.600 → 紫 1.642 线性（设计 9.1）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static double PrismIor(int32 ColorIndex);

	/** 方位角+俯仰 → 单位向量（北=+X、东=+Y、上=+Z；az 北 0° 顺时针，pitch 向上为正）。镜面法线/日月方向同口径。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Optics")
	static FVector DirFromAzPitch(double AzDeg, double PitchDeg);
};
