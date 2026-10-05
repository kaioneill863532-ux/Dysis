#include "DysisOpticsLibrary.h"

namespace
{
	const double Deg = UE_DOUBLE_PI / 180.0;
}

FVector UDysisOpticsLibrary::AzRyToCm(double AzDeg, double R, double Y)
{
	const double Az = AzDeg * Deg;
	// 机关清单坐标换算原话：X = 100·r·cos(az)，Y = 100·r·sin(az)，Z = 100·y；北=+X、东=+Y。
	return FVector(100.0 * R * FMath::Cos(Az), 100.0 * R * FMath::Sin(Az), 100.0 * Y);
}

FVector UDysisOpticsLibrary::ReflectDir(FVector D, FVector N)
{
	// 引擎自带同式反射（§9.2：论坛收敛结论，不必手写）；先归一（测试斜射用例抓过：非单位向量的
	// 反射结果长度会错——光方向约定单位，这里自防御）。用 double 版算完转存。
	const FVector3d Dn = FVector3d(D).GetSafeNormal();
	const FVector3d R = Dn.MirrorByVector(FVector3d(N).GetSafeNormal());
	return FVector(R);
}

bool UDysisOpticsLibrary::RefractDir(FVector D, FVector N, double Eta, FVector& OutT)
{
	// GLSL refract：T = Eta·D + (Eta·CosI − CosT)·N，CosI = −D·N，CosT = sqrt(1 − Eta²·(1 − CosI²))；负根号 = 全内反射。
	const FVector3d Dn = FVector3d(D).GetSafeNormal();
	const FVector3d Nn = FVector3d(N).GetSafeNormal();
	const double CosI = FMath::Clamp(-Dn.Dot(Nn), -1.0, 1.0);
	const double Sin2T = Eta * Eta * (1.0 - CosI * CosI);
	if (Sin2T > 1.0) return false;                     // 全内反射（光密→光疏超临界角）
	const double CosT = FMath::Sqrt(1.0 - Sin2T);
	const FVector3d T = Eta * Dn + (Eta * CosI - CosT) * Nn;
	OutT = FVector(T.GetSafeNormal());
	return true;
}

FVector UDysisOpticsLibrary::ShadowHeadPoint(FVector CameraCm, FVector SunDir, FVector PlanePoint, FVector PlaneNormal)
{
	// 头 = 相机沿太阳反方向走 t 步落在水帘/浮雕平面上：t = ((P0 − Cam)·n) / (−SunDir·n)。
	const FVector3d Dir = -FVector3d(SunDir).GetSafeNormal();
	const FVector3d Nn = FVector3d(PlaneNormal).GetSafeNormal();
	const double Denom = Dir.Dot(Nn);
	if (FMath::Abs(Denom) < 1e-6) return CameraCm;     // 视线几乎平行平面：拿不到投影，退化返回相机
	const double T = FVector3d(PlanePoint - CameraCm).Dot(Nn) / Denom;
	return CameraCm + FVector(T * Dir);
}

bool UDysisOpticsLibrary::IsRainbowDir(FVector FromHeadCm, FVector ToPointCm, FVector SunDir, double BandDeg)
{
	// 虹在"太阳正对面 42°"的圆上（设计 9.1）：影头 → 像素方向 与 −SunDir 的夹角落在带宽内即显虹。
	const FVector3d V = (FVector3d(ToPointCm) - FVector3d(FromHeadCm)).GetSafeNormal();
	const double CosA = FMath::Clamp(V.Dot(-FVector3d(SunDir).GetSafeNormal()), -1.0, 1.0);
	return FMath::Abs(FMath::Acos(CosA) / Deg - 42.0) <= BandDeg;
}

double UDysisOpticsLibrary::PrismIor(int32 ColorIndex)
{
	// 红 1.600 → 紫 1.642 线性（设计 9.1：铅水晶，顶角 45°，相邻两色在眼睛处相隔约 13 cm）。
	const int32 Idx = FMath::Clamp(ColorIndex, 0, 6);
	return 1.600 + (1.642 - 1.600) * double(Idx) / 6.0;
}

FVector UDysisOpticsLibrary::DirFromAzPitch(double AzDeg, double PitchDeg)
{
	const double Az = AzDeg * Deg, P = PitchDeg * Deg;
	return FVector(FMath::Cos(P) * FMath::Cos(Az), FMath::Cos(P) * FMath::Sin(Az), FMath::Sin(P));
}
