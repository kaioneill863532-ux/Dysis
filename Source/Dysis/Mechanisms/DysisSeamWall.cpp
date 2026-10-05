#include "DysisSeamWall.h"
#include "Components/BoxComponent.h"
#include "Optics/DysisOpticsLibrary.h"

ADysisSeamWall::ADysisSeamWall()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Wall = CreateDefaultSubobject<UBoxComponent>(TEXT("Wall"));
	Wall->SetupAttachment(RootComponent);
	Wall->SetCollisionProfileName(TEXT("InvisibleWall"));   // 挡人不挡视线不挡光（调研 §15.6）
	Wall->SetGenerateOverlapEvents(false);
}

void ADysisSeamWall::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LayoutWall();   // 编辑器里改参数即时可见位置（构造期只做摆放，无重活）
}

void ADysisSeamWall::BeginPlay()
{
	Super::BeginPlay();
	LayoutWall();
	Wall->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ADysisSeamWall::LayoutWall()
{
	// 弧带中点近似成一块平板：中心=中方位角×中半径×中高度；宽=弧长；厚=径向带宽；高=墙高。
	// 24° 的弧用弦近似，误差对"挡人"无感（两端略盖进墙里正好）。
	const double MidAz = (AzStart + AzEnd) * 0.5;
	const double MidR = (RadiusInnerM + RadiusOuterM) * 0.5;
	const double SpanDeg = FMath::Abs(AzEnd - AzStart);
	const double WidthCm = MidR * SpanDeg * UE_DOUBLE_PI / 180.0 * 100.0;   // 弧长≈弦长
	const double ThickCm = (RadiusOuterM - RadiusInnerM) * 100.0;
	const double HeightCm = (TopY - BottomY) * 100.0;

	Wall->SetWorldLocation(UDysisOpticsLibrary::AzRyToCm(MidAz, MidR, (BottomY + TopY) * 0.5));
	Wall->SetWorldRotation(FRotator(0.0, MidAz, 0.0));      // 盒本地 X 沿半径 → 厚度朝径向、宽度朝切向
	Wall->SetBoxExtent(FVector(ThickCm * 0.5f, WidthCm * 0.5f, HeightCm * 0.5f));
}
