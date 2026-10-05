#include "DysisLuminousStone.h"
#include "Interaction/DysisGoldenApple.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

ADysisLuminousStone::ADysisLuminousStone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.05f;   // 20 Hz 足够（磷光是慢过程；§18.1 tick 纪律）
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisLuminousStone::BeginPlay()
{
	Super::BeginPlay();
	Glow = 0.0f;
	bLitNow = false;
}

void ADysisLuminousStone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 触发源优先级：手填 AppleActor > 关卡里正被托着的金苹果（自动找，免得潮沟 N 块石头逐块指）> 玩家（测试）。
	const AActor* Source = AppleActor;
	if (!Source && GetWorld())
	{
		TActorIterator<ADysisGoldenApple> It(GetWorld());
		if (It && It->IsCarried()) Source = *It;   // 只认被托着的苹果（还在架上/已归亭的不照亮）
	}
	if (!Source && bUsePlayerAsApple && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			Source = PC->GetPawn();
	bLitNow = Source && FVector::DistSquared(Source->GetActorLocation(), GetActorLocation()) < FMath::Square(IlluminateRadiusCm);

	if (bLitNow) Glow = 1.0f;
	else if (DecaySeconds > 0.0f) Glow = FMath::Max(0.0f, Glow - DeltaTime / DecaySeconds);
	else Glow = 0.0f;

	// 材质口（青白磷光；美术把 emissive 曲线挂在 Glow 上）。参数打在目标网格的根 Mesh 上。
	if (TargetMesh)
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(TargetMesh->GetRootComponent()))
			Mesh->SetScalarParameterValueOnMaterials(TEXT("Glow"), Glow);

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
	// 调试：亮度映射蓝→青；可踩阈值以下描红边提示"快暗了"。
	const FColor Color = FMath::Lerp(FLinearColor(0.0f, 0.0f, 1.0f), FLinearColor(0.0f, 1.0f, 1.0f), Glow).ToFColor(false);
	DrawDebugSphere(GetWorld(), GetActorLocation(), 30.0f, 10, Color, false, PrimaryActorTick.TickInterval, 0, 1.5f);
#endif
}

bool ADysisLuminousStone::IsWalkableAt(const FVector& FootCm) const
{
	// 亮着才踩得住（灰盒：夜光石被照亮发光，亮着的时候踩得住）。石墩自身位置即判定点，
	// 脚位只用来确认"踩的是这块"（半径 60 cm 的石墩）。
	return Glow > WalkableGlow
	    && FVector::DistSquared2D(FootCm, GetActorLocation()) < FMath::Square(60.0)
	    && FMath::Abs(FootCm.Z - GetActorLocation().Z) < 200.0;
}
