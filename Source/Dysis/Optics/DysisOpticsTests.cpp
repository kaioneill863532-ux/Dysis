// 日落回廊 · 自动化测试（调研 §8.7/§15.7）：光学纯函数 + 光路时刻表。全部不依赖关卡资产，headless 可跑：
//   UnrealEditor-Cmd Dysis.uproject -ExecCmds="Automation RunTests Dysis" -TestExit="Automation Test Queue Empty" -unattended -nullrhi -nosound -log
#include "DysisOpticsLibrary.h"
#include "Sky/DysisSkyLibrary.h"
#include "Misc/AutomationTest.h"

// ───── 坐标换算（机关清单换算式：X=100·r·cos az，Y=100·r·sin az，Z=100·y）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisAzRyTest, "Dysis.Optics.AzRyToCm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisAzRyTest::RunTest(const FString&)
{
	TestTrue(TEXT("北 10m"), UDysisOpticsLibrary::AzRyToCm(0.0, 10.0, 2.0).Equals(FVector(1000, 0, 200), 0.01));
	TestTrue(TEXT("东 10m"), UDysisOpticsLibrary::AzRyToCm(90.0, 10.0, 2.0).Equals(FVector(0, 1000, 200), 0.01));
	return true;
}

// ───── 反射（R = D − 2(D·N)N）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisReflectTest, "Dysis.Optics.ReflectDir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisReflectTest::RunTest(const FString&)
{
	// 平行于镜面：不偏转。
	TestTrue(TEXT("掠射不偏"), UDysisOpticsLibrary::ReflectDir(FVector(1, 0, 0), FVector(0, 0, 1)).Equals(FVector(1, 0, 0), 1e-4));
	// 正入射：原路返回。
	TestTrue(TEXT("正入射返回"), UDysisOpticsLibrary::ReflectDir(FVector(0, 0, -1), FVector(0, 0, 1)).Equals(FVector(0, 0, 1), 1e-4));
	// 45° 入射水平镜 → 转成水平（论坛踩坑：入射方向必须已初始化，这里显式给全值）。
	TestTrue(TEXT("斜射折转"), UDysisOpticsLibrary::ReflectDir(FVector(1, 0, -1), FVector(0, 0, 1)).Equals(FVector(1, 0, 1).GetSafeNormal(), 1e-4));
	return true;
}

// ───── 折射（Snell 向量式，GLSL refract 同款）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisRefractTest, "Dysis.Optics.RefractDir",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisRefractTest::RunTest(const FString&)
{
	FVector T = FVector::ZeroVector;
	// 正入射不弯折。
	TestTrue(TEXT("正入射直穿"), UDysisOpticsLibrary::RefractDir(FVector(0, 0, -1), FVector(0, 0, 1), 0.5, T));
	TestTrue(TEXT("方向不变"), T.Equals(FVector(0, 0, -1), 1e-4));
	// 玻璃→空气超过临界角：全内反射（cosθi≈0.312，η=1.5 → 1−2.25·(1−0.097)<0）。
	TestFalse(TEXT("全内反射"), UDysisOpticsLibrary::RefractDir(FVector(0.95, 0, -0.3122).GetSafeNormal(), FVector(0, 0, 1), 1.5, T));
	return true;
}

// ───── 彩虹 42°（圆心=影头）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisRainbowTest, "Dysis.Optics.Rainbow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisRainbowTest::RunTest(const FString&)
{
	// 太阳在正东（+Y）水平方向，影头在原点：太阳正对面 42° 仰角处的那一点应判为虹。
	const FVector SunDir = FVector(0, 1, 0);
	const double Az = 42.0 * UE_DOUBLE_PI / 180.0;
	const FVector P(0, -100 * FMath::Cos(Az), 100 * FMath::Sin(Az));          // 太阳反方向水平 100、抬 42°
	TestTrue(TEXT("42° 环上判虹"), UDysisOpticsLibrary::IsRainbowDir(FVector::ZeroVector, P, SunDir, 4.0));
	TestFalse(TEXT("环外不判"), UDysisOpticsLibrary::IsRainbowDir(FVector::ZeroVector, FVector(0, -100, 5), SunDir, 4.0));
	return true;
}

// ───── 棱镜折射率（红 1.600 → 紫 1.642）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisPrismTest, "Dysis.Optics.PrismIor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisPrismTest::RunTest(const FString&)
{
	TestEqual(TEXT("红 1.600"), UDysisOpticsLibrary::PrismIor(0), 1.600, 1e-6);
	TestEqual(TEXT("紫 1.642"), UDysisOpticsLibrary::PrismIor(6), 1.642, 1e-6);
	TestEqual(TEXT("靛(第六格)"), UDysisOpticsLibrary::PrismIor(5), 1.600 + 0.042 * 5.0 / 6.0, 1e-6);
	return true;
}

// ───── 方位角+俯仰 → 方向（镜面法线/日月同口径）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisAzPitchTest, "Dysis.Optics.DirFromAzPitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisAzPitchTest::RunTest(const FString&)
{
	TestTrue(TEXT("北水平"), UDysisOpticsLibrary::DirFromAzPitch(0.0, 0.0).Equals(FVector(1, 0, 0), 1e-5));
	TestTrue(TEXT("东水平"), UDysisOpticsLibrary::DirFromAzPitch(90.0, 0.0).Equals(FVector(0, 1, 0), 1e-5));
	TestTrue(TEXT("天顶"), UDysisOpticsLibrary::DirFromAzPitch(0.0, 90.0).Equals(FVector(0, 0, 1), 1e-5));
	TestTrue(TEXT("单位向量"), FMath::Abs(UDysisOpticsLibrary::DirFromAzPitch(153.19, 30.90).Size() - 1.0) < 1e-5);
	return true;
}

// ───── 光路时刻表（beam:b1 脚 24.0 → 顶 24.6，generated.h BeamTimeY 同源）─────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisBeamTimeTest, "Dysis.Sky.BeamTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisBeamTimeTest::RunTest(const FString&)
{
	TestEqual(TEXT("b1 脚 H=24.0"), UDysisSkyLibrary::DysisTimeAt(TEXT("beam:b1"), FVector(0, 0, 0), false, -999.f), 24.0f, 0.01f);
	TestEqual(TEXT("b1 顶 H=24.6"), UDysisSkyLibrary::DysisTimeAt(TEXT("beam:b1"), FVector(0, 0, 940), false, -999.f), 24.6f, 0.01f);
	TestEqual(TEXT("h1 脚 H=32.5"), UDysisSkyLibrary::DysisTimeAt(TEXT("beam:h1"), FVector(0, 0, 1450), false, -999.f), 32.5f, 0.01f);
	return true;
}
