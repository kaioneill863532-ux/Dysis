#include "DysisSkyActor.h"
#include "DysisSkyLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/Scene.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 灰盒的 smoothstep(a, b, x)：a > b 时是反过来的台阶
	float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	float AltOf(const FVector& V) { return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(V.Z, -1.0, 1.0))); }
}

ADysisSkyActor::ADysisSkyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(RootComponent);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->SetAtmosphereSunLight(false);   // 照亮大气的是下面的 SunSkyGlow

	SunSkyGlow = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunSkyGlow"));
	SunSkyGlow->SetupAttachment(RootComponent);
	SunSkyGlow->SetMobility(EComponentMobility::Movable);
	SunSkyGlow->SetAtmosphereSunLight(true);
	SunSkyGlow->SetAtmosphereSunLightIndex(0);
	SunSkyGlow->SetCastShadows(false);
	SunSkyGlow->LightingChannels.bChannel0 = false;   // 不照任何物体
	SunSkyGlow->LightingChannels.bChannel1 = false;
	SunSkyGlow->LightingChannels.bChannel2 = false;

	MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
	MoonLight->SetupAttachment(RootComponent);
	MoonLight->SetMobility(EComponentMobility::Movable);
	MoonLight->SetAtmosphereSunLight(false);   // 夜空由 NightDome 画，见头文件

	for (int32 i = 0; i < 4; ++i)
	{
		UDirectionalLightComponent* Fill = CreateDefaultSubobject<UDirectionalLightComponent>(*FString::Printf(TEXT("NightFill%d"), i));
		Fill->SetupAttachment(RootComponent);
		Fill->SetMobility(EComponentMobility::Movable);
		Fill->SetAtmosphereSunLight(false);
		Fill->SetCastShadows(false);
		Fill->SetIntensity(0.0f);
		Fill->ForwardShadingPriority = 5 - i;   // 四盏各排一个名次（太阳、月亮都灭着的那一阵，免得引擎在屏幕上提示“几盏平行光在抢”）
		NightFill.Add(Fill);
	}
	// 半透明、水、体积雾只认一盏平行光：太阳优先，其次月亮（不排的话引擎会在屏幕上提示“几盏平行光在抢”）
	SunLight->ForwardShadingPriority = 10;
	MoonLight->ForwardShadingPriority = 9;
	SunSkyGlow->ForwardShadingPriority = 1;

	MoonDisc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MoonDisc"));
	MoonDisc->SetupAttachment(RootComponent);
	MoonDisc->SetMobility(EComponentMobility::Movable);
	MoonDisc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonDisc->SetCastShadow(false);
	MoonDisc->bUseAsOccluder = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) MoonDisc->SetStaticMesh(Sphere.Object);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DiscMat(TEXT("/Game/Dysis/Sky/M_DysisMoonDisc.M_DysisMoonDisc"));
	if (DiscMat.Succeeded()) MoonDiscMaterial = DiscMat.Object;

	NightDome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NightDome"));
	NightDome->SetupAttachment(RootComponent);
	NightDome->SetMobility(EComponentMobility::Movable);
	NightDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NightDome->SetCastShadow(false);
	NightDome->bUseAsOccluder = false;
	NightDome->SetTranslucentSortPriority(-100);   // 最先画：星星、月亮、别的半透明都盖在它上面
	if (Sphere.Succeeded()) NightDome->SetStaticMesh(Sphere.Object);

	Stars = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stars"));
	Stars->SetupAttachment(RootComponent);
	Stars->SetMobility(EComponentMobility::Movable);
	Stars->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Stars->SetCastShadow(false);
	Stars->bUseAsOccluder = false;
	Stars->SetTranslucentSortPriority(-90);
	Stars->NumCustomDataFloats = 3;   // 每颗星自己的颜色 × 亮度
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) Stars->SetStaticMesh(Plane.Object);

	PreviewH = UDysisSkyLibrary::DysisConst(TEXT("H_I"));
}

void ADysisSkyActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildStars();
	bTimeSet = false;
	SetTime(PreviewH);
}

void ADysisSkyActor::BeginPlay()
{
	Super::BeginPlay();
	BuildStars();
	bTimeSet = false;
	SetTime(PreviewH);
}

void ADysisSkyActor::SetAfterSunset(bool bAfter)
{
	if (bAfterSunset == bAfter) return;
	bAfterSunset = bAfter;
	RefreshSky();
}

float ADysisSkyActor::GetDuskLook() const
{
	// 太阳从 DuskStartAltDeg 落到 DuskFullAltDeg 越来越浓；落到地平线下以后跟着天黑退掉（到 −7° 没有）
	return Smooth(DuskStartAltDeg, DuskFullAltDeg, SunAlt) * (1.f - Smooth(-1.5f, -7.f, SunAlt));
}

float ADysisSkyActor::ApplyDuskLook(FPostProcessSettings& PP) const
{
	const float K = GetDuskLook();
	PP.bOverride_ColorGain = PP.bOverride_ColorSaturation = K > 0.001f;
	PP.ColorGain = FVector4(FMath::Lerp(1.f, DuskTint.R, K), FMath::Lerp(1.f, DuskTint.G, K), FMath::Lerp(1.f, DuskTint.B, K), 1.f);
	const float Sat = FMath::Lerp(1.f, DuskSaturation, K);
	PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	return DuskExposureBias * K;
}

void ADysisSkyActor::ApplyDuskAtmosphere()
{
	// 关卡里那一个大气：黄昏时把“散掉蓝光”和“尘雾”都调大，落日和天边就更红
	if (!Atmosphere.IsValid() && GetWorld())
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			if (USkyAtmosphereComponent* C = It->FindComponentByClass<USkyAtmosphereComponent>())
			{
				Atmosphere = C;
				BaseRayleigh = C->RayleighScatteringScale;
				BaseMie = C->MieScatteringScale;
				break;
			}
	USkyAtmosphereComponent* C = Atmosphere.Get();
	const float K = GetDuskLook();
	if (!C || FMath::IsNearlyEqual(K, AppliedDusk, 0.002f)) return;
	AppliedDusk = K;
	C->SetRayleighScatteringScale(BaseRayleigh * FMath::Lerp(1.f, DuskRayleighScale, K));
	C->SetMieScatteringScale(BaseMie * FMath::Lerp(1.f, DuskMieScale, K));
}

float ADysisSkyActor::AfterSunsetK() const
{
	return bAfterSunset ? 1.f : Smooth(-1.2f, -2.f, SunAlt);
}

void ADysisSkyActor::RefreshSky()
{
	bTimeSet = false;   // 让 SetTime 不走“没变就不动”的那条近路
	SetTime(CurrentH);
}

void ADysisSkyActor::BuildStars()
{
	if (!Stars || !Stars->GetStaticMesh()) return;
	if (StarsBuilt == StarCount && Stars->GetInstanceCount() == StarCount) return;
	Stars->ClearInstances();
	// 灰盒的那一套随机数（seed 7，每次 ×16807 对 2147483647 取余），每颗星依次取 u、th、亮度、颜色四个数
	int64 Seed = 7;
	auto Rnd = [&Seed]() { Seed = (Seed * 16807) % 2147483647; return double(Seed) / 2147483647.0; };
	const double Size = 2.0 * StarDistanceCm * FMath::Tan(FMath::DegreesToRadians(StarAngularSizeDeg * 0.5));
	TArray<FTransform> Xf; Xf.Reserve(StarCount);
	TArray<float> Data; Data.Reserve(StarCount * 3);
	for (int32 i = 0; i < StarCount; ++i)
	{
		const double U = Rnd() * 2.0 - 1.0, Th = Rnd() * UE_DOUBLE_TWO_PI, Rr = FMath::Sqrt(FMath::Max(0.0, 1.0 - U * U));
		const double B = 0.35 + FMath::Pow(Rnd(), 3.0) * 1.6, W = Rnd();
		// 灰盒（东、上、南）→ 这里（北、东、上）
		const FVector Dir(-Rr * FMath::Sin(Th), Rr * FMath::Cos(Th), U);
		Xf.Emplace(FRotationMatrix::MakeFromZ(-Dir).Rotator(), Dir * StarDistanceCm, FVector(Size / 100.0));   // 引擎的 Plane 是 100 cm 见方，面朝天空中心
		Data.Add(float(B * (0.85 + 0.15 * W))); Data.Add(float(B * 0.9)); Data.Add(float(B * (1.05 - 0.1 * W)));
	}
	Stars->AddInstances(Xf, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false);
	for (int32 i = 0; i < StarCount; ++i) Stars->SetCustomData(i, MakeArrayView(&Data[i * 3], 3), i == StarCount - 1);
	StarsBuilt = StarCount;
}

FRotator ADysisSkyActor::GetMainLightRotation() const
{
	return UDysisSkyLibrary::DysisLightRotation(bSunMain ? SunDir : MoonDir);
}

void ADysisSkyActor::SetTime(float H)
{
	// P1-9（调研 §15.8）：H 没变就别碰灯——时间组件每帧都调本函数，但只在 H 变化时广播，
	// 早退不漏任何真变化；省掉的是每帧灯光全量重设（VSM 重建的触发器）。PreviewH 拖动值每帧不同，不受影响。
	if (bTimeSet && FMath::IsNearlyEqual(CurrentH, H, 1e-6f))
	{
		return;
	}
	bTimeSet = true;

	CurrentH = H;
	SunDir = UDysisSkyLibrary::DysisSunDir(H);
	MoonDir = UDysisSkyLibrary::DysisMoonDir(H);
	SunAlt = AltOf(SunDir);
	MoonAlt = AltOf(MoonDir);
	const float K = Smooth(2.f, 25.f, SunAlt);

	// 两盏灯各自一直朝着自己的天体；谁是主光、亮度多少按灰盒 setTime
	SunLight->SetWorldRotation(UDysisSkyLibrary::DysisLightRotation(SunDir));
	MoonLight->SetWorldRotation(UDysisSkyLibrary::DysisLightRotation(MoonDir));
	bSunMain = SunAlt > -0.8f;
	if (bSunMain)
	{
		GreyboxIntensity = FMath::Lerp(2.4f, 3.6f, K) * Smooth(-0.8f, 1.2f, SunAlt);
		SunLight->SetIntensity(GreyboxIntensity * SunLuxPerGreyboxUnit);
		SunLight->SetLightColor(FMath::Lerp(SunColorLow, SunColorHigh, K));
		MoonLight->SetIntensity(0.f);
	}
	else
	{
		GreyboxIntensity = 0.55f * Smooth(0.f, 7.f, MoonAlt) * Smooth(-0.8f, -5.f, SunAlt);
		SunLight->SetIntensity(0.f);
		MoonLight->SetIntensity(GreyboxIntensity * MoonLuxPerGreyboxUnit);
		MoonLight->SetLightColor(MoonColor);
	}
	// 天空：白天和太阳光一样亮（太阳高度 1.2° 以上两条曲线重合）；再往下不是到 −0.8° 就灭，而是一直暗到 TwilightEndAltDeg
	if (SunSkyGlow)
	{
		SunSkyGlow->SetWorldRotation(UDysisSkyLibrary::DysisLightRotation(SunDir));
		SunSkyGlow->SetIntensity(FMath::Lerp(2.4f, 3.6f, K) * Smooth(TwilightEndAltDeg, 1.2f, SunAlt) * SunLuxPerGreyboxUnit);
		SunSkyGlow->SetLightColor(SunColorHigh);
	}
	// 夜里的环境光（灰盒 nightAmb.intensity = 0.45 × dusk）
	{
		const float Dusk = Smooth(1.5f, -9.f, SunAlt);
		for (int32 i = 0; i < NightFill.Num(); ++i)
			if (UDirectionalLightComponent* Fill = NightFill[i])
			{
				// 夜里的那一份 + 落日以后暮色的那一份（见头文件 DayFillLux）
				const FLinearColor Sum = NightFillColor * (NightFillLux * Dusk) + DayFillColor * (DayFillLux * (1.f - Dusk) * AfterSunsetK());
				const float Peak = FMath::Max3(Sum.R, Sum.G, Sum.B);
				Fill->SetWorldRotation(FRotator(-NightFillElevationDeg, 45.0f + 90.0f * i, 0.0f));
				Fill->SetIntensity(Peak);
				Fill->SetLightColor(Peak > 1.0e-6f ? FLinearColor(Sum.R / Peak, Sum.G / Peak, Sum.B / Peak) : NightFillColor);
			}
	}
	MoonDiscOpacity = Smooth(-1.5f, 1.f, MoonAlt) * Smooth(4.f, -2.f, SunAlt);
	ApplyMoonDisc();
	ApplyNightSky();
	if (GetWorld() && GetWorld()->IsGameWorld()) ApplyDuskAtmosphere();   // 只在游戏里动大气（编辑器里拖时间预览时不改关卡里的那个大气）
}

void ADysisSkyActor::ApplyNightSky()
{
	// 这两个材质用到的时候才去找（不在构造函数里找：那样引擎会一直攥着它们，材质脚本重跑时一动就崩；也免得刚建好材质还得重启编辑器）
	if (!NightDomeMaterial) NightDomeMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Sky/M_DysisNightDome.M_DysisNightDome"));
	if (!StarMaterial) StarMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Sky/M_DysisStar.M_DysisStar"));
	// 夜空的球（灰盒 nightDome.uO = smoothstep(-2, -10, 太阳高度)）。落日以后整片天都由它画：
	// 先是灰蒙蒙的暮色（跟着太阳往下走越来越暗），再按上面那个比例换成夜空
	if (NightDome)
	{
		const float NightK = Smooth(-2.f, -10.f, SunAlt);
		const float Opacity = FMath::Max(NightK, AfterSunsetK());
		const float Twilight = Smooth(-10.f, -1.f, SunAlt) * (1.f - NightK) * AfterSunsetK();
		NightDome->SetRelativeScale3D(FVector(2.f * NightDomeRadiusCm / 100.f));   // 引擎的 Sphere 直径 100 cm
		if (NightDomeMaterial && (!NightDomeMID || NightDomeMID->Parent != NightDomeMaterial))
		{
			NightDomeMID = UMaterialInstanceDynamic::Create(NightDomeMaterial, this);
			NightDome->SetMaterial(0, NightDomeMID);
		}
		if (NightDomeMID)
		{
			NightDomeMID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			NightDomeMID->SetScalarParameterValue(TEXT("Glow"), NightSkyGain);
			NightDomeMID->SetScalarParameterValue(TEXT("NightW"), Opacity > 1.0e-4f ? NightK / Opacity : 1.f);
			NightDomeMID->SetScalarParameterValue(TEXT("TwilightW"), Opacity > 1.0e-4f ? Twilight / Opacity : 0.f);
			NightDomeMID->SetVectorParameterValue(TEXT("Twilight"), TwilightSkyColor);
			NightDomeMID->SetScalarParameterValue(TEXT("Halo"), MoonDiscOpacity);
			NightDomeMID->SetScalarParameterValue(TEXT("MoonUp"), Smooth(-0.05f, 0.1f, float(MoonDir.Z)));
			NightDomeMID->SetVectorParameterValue(TEXT("MoonDir"), FLinearColor(float(MoonDir.X), float(MoonDir.Y), float(MoonDir.Z), 0.f));
		}
		NightDome->SetVisibility(NightDomeMID && Opacity > 0.001f);
	}
	// 星星（灰盒 stars.opacity = smoothstep(-4, -13, 太阳高度)；整片绕北天极转）
	if (Stars)
	{
		StarOpacity = Smooth(-4.f, -13.f, SunAlt);
		if (StarMaterial && (!StarMID || StarMID->Parent != StarMaterial))
		{
			StarMID = UMaterialInstanceDynamic::Create(StarMaterial, this);
			Stars->SetMaterial(0, StarMID);
		}
		if (StarMID)
		{
			StarMID->SetScalarParameterValue(TEXT("Opacity"), StarOpacity);
			StarMID->SetScalarParameterValue(TEXT("Glow"), StarGain);
		}
		const float Lat = FMath::DegreesToRadians(UDysisSkyLibrary::DysisConst(TEXT("LAT")));
		// 北天极：朝北、仰起一个纬度的角。灰盒是绕它转 −H；这里的坐标是左手的，同一个转法写出来是 +H
		Stars->SetRelativeRotation(FQuat(FVector(FMath::Cos(Lat), 0.0, FMath::Sin(Lat)), FMath::DegreesToRadians(CurrentH)));
		Stars->SetVisibility(StarMID && StarOpacity > 0.001f);
	}
}

void ADysisSkyActor::ApplyMoonDisc()
{
	if (!MoonDisc) return;
	const float Diameter = 2.f * MoonDiscDistanceCm * FMath::Tan(FMath::DegreesToRadians(MoonDiscAngularDiameterDeg * 0.5f));
	MoonDisc->SetRelativeLocation(MoonDir * MoonDiscDistanceCm);
	MoonDisc->SetRelativeScale3D(FVector(Diameter / 100.f));   // 引擎的 Sphere 直径 100 cm
	if (MoonDiscMaterial && (!MoonDiscMID || MoonDiscMID->Parent != MoonDiscMaterial))
	{
		MoonDiscMID = UMaterialInstanceDynamic::Create(MoonDiscMaterial, this);
		MoonDisc->SetMaterial(0, MoonDiscMID);
	}
	if (MoonDiscMID) MoonDiscMID->SetScalarParameterValue(TEXT("Opacity"), MoonDiscOpacity);
	MoonDisc->SetVisibility(MoonDiscOpacity > 0.001f);
}