#include "DysisSliderActor.h"
#include "Mechanisms/DysisLeverActor.h"
#include "Optics/DysisOpticsLibrary.h"

ADysisSliderActor::ADysisSliderActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;   // 滑动动画期间才 Tick（§18.1 tick 纪律）
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisSliderActor::BeginPlay()
{
	Super::BeginPlay();
	CurrentAz = AzStart;                                  // 灰盒开局：石板在"挡住窗"的一头
	TargetAz = AzStart;
	if (TargetMesh) TargetMesh->SetActorLocation(UDysisOpticsLibrary::AzRyToCm(CurrentAz, RadiusM, HeightM));
	if (Lever) Lever->OnToggled.AddDynamic(this, &ADysisSliderActor::OnLeverToggled);
}

void ADysisSliderActor::OnLeverToggled(bool bPulled)
{
	const bool bGoEnd = bPulled ? bGoToEndWhenPulled : !bGoToEndWhenPulled;
	TargetAz = bGoEnd ? AzEnd : AzStart;
	SetActorTickEnabled(true);
}

void ADysisSliderActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!TargetMesh) { SetActorTickEnabled(false); return; }
	// 沿方位角恒速滑（弧线贴外墙切向——机关清单"贴着外墙沿切向滑"）。朝向保持导入时的 Yaw 不变（v1）。
	CurrentAz = FMath::FInterpConstantTo(CurrentAz, TargetAz, DeltaTime, DegPerSec);
	TargetMesh->SetActorLocation(UDysisOpticsLibrary::AzRyToCm(CurrentAz, RadiusM, HeightM));
	if (FMath::IsNearlyEqual(CurrentAz, TargetAz, 1e-3))
		SetActorTickEnabled(false);
}
