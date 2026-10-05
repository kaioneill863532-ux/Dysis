#include "DysisMoonPath.h"
#include "Surfaces/DysisMoonSurface.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

ADysisMoonPath::ADysisMoonPath()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ADysisSkyActor* ADysisMoonPath::ResolveSky() const
{
	ADysisSkyActor* Sky = CachedSky.Get();
	if (!Sky && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) { CachedSky = *It; Sky = *It; }
	}
	return Sky;
}

UDysisTimeComponent* ADysisMoonPath::ResolveTime() const
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}

void ADysisMoonPath::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RebuildCountdown -= DeltaTime;
	if (RebuildCountdown > 0.0) return;
	RebuildCountdown = RebuildInterval;
	Relayout();

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
	// 调试可视化：一片片踏面画绿框（寿命=重铺间隔）。正式视觉由水面材质的 sparkle 光带承担。
	for (const FTile& T : Tiles)
		DrawDebugBox(GetWorld(), T.CenterCm, FVector(StepCm * 0.5, WidthCm * 0.5, 5.0),
			FRotationMatrix::MakeFromZX(FVector::UpVector, T.Dir).ToQuat(), FColor::Green, false, float(RebuildInterval), 0, 2.0f);
#endif
}

void ADysisMoonPath::Relayout()
{
	Tiles.Reset();
	if (!TargetWater || !GetWorld()) return;

	// 生效条件（灰盒）：夜里 + 水面波纹态 + 面向月亮。
	UDysisTimeComponent* Time = ResolveTime();
	ADysisSkyActor* Sky = ResolveSky();
	if (!Time || !Sky || !Time->bNight) return;
	if (TargetWater->State != EDysisWaterState::Ripple) return;
	const FVector MoonDir = UDysisSkyLibrary::DysisMoonDir(Sky->GetTime());
	if (MoonDir.Z <= 0.0) return;
	FVector Dir = FVector(MoonDir.X, MoonDir.Y, 0.0).GetSafeNormal();
	if (Dir.SizeSquared() < 0.5) return;

	APawn* Pawn = nullptr;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) Pawn = PC->GetPawn();
	if (!Pawn) return;
	// 视线面向月亮（相机朝向，与踏片同口径）。
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ViewDir = Cam->GetCameraRotation().Vector();
		const double AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ViewDir.GetSafeNormal(), MoonDir), -1.0, 1.0)));
		if (AngleDeg > ViewMoonMaxDeg) return;
	}

	// 起点：玩家立足点（脚底）钳进水面盒——在水里从脚下铺，在岸上从最近的水边铺。
	FVector Foot = Pawn->GetActorLocation();
	Foot.Z = SurfaceZCm;
	const FBox Bounds(FVector(TargetWater->BoxMinCm.X, TargetWater->BoxMinCm.Y, SurfaceZCm - 50.0),
	                  FVector(TargetWater->BoxMaxCm.X, TargetWater->BoxMaxCm.Y, SurfaceZCm + 50.0));
	FVector Start = FVector(FMath::Clamp(Foot.X, Bounds.Min.X, Bounds.Max.X),
	                        FMath::Clamp(Foot.Y, Bounds.Min.Y, Bounds.Max.Y), SurfaceZCm);

	// 铺：沿月亮方位一步步，直到出盒。
	for (int32 i = 0; i < MaxSteps; ++i)
	{
		const FVector Center = Start + Dir * (StepCm * double(i));
		if (Center.X < Bounds.Min.X || Center.X > Bounds.Max.X || Center.Y < Bounds.Min.Y || Center.Y > Bounds.Max.Y) break;
		FTile T;
		T.CenterCm = Center;
		T.Dir = Dir;
		Tiles.Add(T);
	}
}

bool ADysisMoonPath::IsWalkableAt(const FVector& FootCm) const
{
	// 点在任一踏面的旋转矩形内（2D 判定，Z 只做±50cm 的贴近检查——人踩在带面上）。
	if (FMath::Abs(FootCm.Z - SurfaceZCm) > 50.0) return false;
	for (const FTile& T : Tiles)
	{
		if (T.Dir.SizeSquared() < 0.5) continue;
		const FVector D = T.Dir;
		const FVector R = FVector::CrossProduct(FVector::UpVector, D);   // 垂直走向（带宽方向）
		const FVector Delta = FootCm - T.CenterCm;
		if (FMath::Abs(FVector::DotProduct(Delta, D)) <= StepCm * 0.5 &&
		    FMath::Abs(FVector::DotProduct(Delta, R)) <= WidthCm * 0.5)
		{
			bHasWalked = true;   // 文案表·提示条件：走过月桥（下月桥后与女神交互时显示提示）
			return true;
		}
	}
	return false;
}

bool ADysisMoonPath::GetZoneName(FName& OutZone) const
{
	OutZone = ZoneName;
	return true;
}
