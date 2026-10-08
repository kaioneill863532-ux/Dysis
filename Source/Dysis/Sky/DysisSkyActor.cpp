#include "DysisSkyActor.h"
#include "DysisSkyLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
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
	MoonLight->SetAtmosphereSunLight(true);
	MoonLight->SetAtmosphereSunLightIndex(1);

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

	PreviewH = UDysisSkyLibrary::DysisConst(TEXT("H_I"));
}

void ADysisSkyActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetTime(PreviewH);
}

void ADysisSkyActor::BeginPlay()
{
	Super::BeginPlay();
	SetTime(PreviewH);
}

FRotator ADysisSkyActor::GetMainLightRotation() const
{
	return UDysisSkyLibrary::DysisLightRotation(bSunMain ? SunDir : MoonDir);
}

void ADysisSkyActor::SetTime(float H)
{
	// P1-9（调研 §15.8）：H 没变就别碰灯——时间组件每帧都调本函数，但只在 H 变化时广播，
	// 早退不漏任何真变化；省掉的是每帧灯光全量重设（VSM 重建的触发器）。PreviewH 拖动值每帧不同，不受影响。
	if (FMath::IsNearlyEqual(CurrentH, H, 1e-6f))
	{
		return;
	}

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
	MoonDiscOpacity = Smooth(-1.5f, 1.f, MoonAlt) * Smooth(4.f, -2.f, SunAlt);
	ApplyMoonDisc();
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