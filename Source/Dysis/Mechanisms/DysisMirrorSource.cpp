#include "DysisMirrorSource.h"
#include "Optics/DysisOpticsLibrary.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "UI/DysisCopy.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADysisMirrorSource::ADysisMirrorSource()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;   // 需要每帧采样镜面照亮
	PrimaryActorTick.TickInterval = 0.1f;             // 10 Hz 采样够用
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ADysisSkyActor* ADysisMirrorSource::ResolveSkyForSample() const
{
	ADysisSkyActor* Sky = CachedSampleSky.Get();
	if (!Sky && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) { CachedSampleSky = *It; Sky = *It; }
	}
	return Sky;
}

void ADysisMirrorSource::SampleMirrorIllumination()
{
	// §4 规格书原话"镜心被阳光照到…镜面照亮一半以上，镜光才能走"：
	// 在镜面平面（法线 Normal 的垂直面）取 3×3 = 9 个采样点，逐点沿 SunDir 打 50m trace。
	// 打不到东西 = 阳光直射到这个点 = 被照亮。比例 ≥50% 才 CanMirrorBeamWalk()。
	ADysisSkyActor* Sky = ResolveSkyForSample();
	if (!Sky || !GetWorld()) { MirrorIllumination = 1.0; return; }

	const FVector SunDir = UDysisSkyLibrary::DysisSunDir(Sky->GetTime());
	const FVector Normal = GetMirrorNormal();
	// 镜面平面上的两个正交基（叉积构建）。
	const FVector Up = FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Normal, Up).GetSafeNormal();
	const FVector Center = GetActorLocation();

	constexpr int32 GridN = 3;
	int32 LitCount = 0;
	for (int32 iu = 0; iu < GridN; ++iu)
	{
		for (int32 ir = 0; ir < GridN; ++ir)
		{
			const double FU = (double(iu) / double(GridN - 1)) * 2.0 - 1.0;
			const double FR = (double(ir) / double(GridN - 1)) * 2.0 - 1.0;
			const FVector SamplePoint = Center + Up * (FU * MirrorSampleRadiusCm) + Right * (FR * MirrorSampleRadiusCm);
			FHitResult Hit;
			FCollisionQueryParams Params(TEXT("DysisMirrorSample"), false, this);
			if (!GetWorld()->LineTraceSingleByChannel(Hit, SamplePoint, SamplePoint + SunDir * 5000.0, ECC_Visibility, Params))
				++LitCount;   // 打不到 = 太阳直射
		}
	}
	MirrorIllumination = double(LitCount) / double(GridN * GridN);
}

void ADysisMirrorSource::BeginPlay()
{
	Super::BeginPlay();
	if (SlotAzDeg.Num() == 0) SlotAzDeg = { 68.90, 122.90, 153.19, 206.19, 259.19, 312.19 };
	CurrentSlot = FMath::Clamp(StartSlot, 0, SlotAzDeg.Num() - 1);
	CurrentAz = GetCurrentSlotAz();
	bAzInit = true;
}

void ADysisMirrorSource::Interact(APawn* Player, bool bFromFront)
{
	if (SlotAzDeg.Num() == 0) return;
	CurrentSlot = (CurrentSlot + 1) % SlotAzDeg.Num();   // 顺时针进一格（灰盒：底座随时能转，不设锁）
	SetActorTickEnabled(true);                            // 转到位自动休眠（Tick 里判）
	// TODO(M3)：绞盘 1:2 手感（像转 1° 轮转 2°）、格位定位声。
}

void ADysisMirrorSource::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 恒速转到目标格——反射光随 CurrentAz 平滑扫动（扫过月石/银月的瞬间正是玩法）。
	// 注意：不能 SetActorTickEnabled(false)——镜面采样需要持续运行（TickInterval=0.1s 已降频）。
	const double Target = GetCurrentSlotAz();
	CurrentAz = FMath::FInterpConstantTo(CurrentAz, Target, DeltaTime, RotateDegPerSec);

	// §4 镜面采样：每帧（10Hz 间隔）更新照亮比例。
	SampleMirrorIllumination();
}

double ADysisMirrorSource::GetCurrentSlotAz() const
{
	return SlotAzDeg.IsValidIndex(CurrentSlot) ? SlotAzDeg[CurrentSlot] : 0.0;
}

FVector ADysisMirrorSource::GetMirrorNormal() const
{
	return UDysisOpticsLibrary::DirFromAzPitch(CurrentAz, MirrorPitchDeg);
}

FText ADysisMirrorSource::GetInteractPrompt() const
{
	// 文案表·交互显示：三相机关 → "转动雕像"
	return FText::FromString(DysisCopy::PromptRotateStatue);
}

// ── 控制台：Dysis.MirrorInfo——镜光落点验证（M3 收尾）：打印镜源当前格/法线，以及场上第一束
//    bFromMirror 光柱的方向与落点，供"第五格 206.19° 把光送到 L3 东南"的点位对照（施工图/Dysis.Go）。
namespace
{
	FAutoConsoleCommandWithWorldAndArgs GDysisMirrorInfo(
		TEXT("Dysis.MirrorInfo"),
		TEXT("打印铜镜当前格/法线 + 反射光柱方向与落点（UE 厘米）"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (!World) return;
			ADysisMirrorSource* Mirror = nullptr;
			{
				TActorIterator<ADysisMirrorSource> It(World);
				if (It) Mirror = *It;   // Mac clang：for+break 单次循环是 -Werror，迭代器判一次
			}
			if (!Mirror) { UE_LOG(LogTemp, Warning, TEXT("Dysis.MirrorInfo：关卡里没有 DysisMirrorSource")); return; }

			const ADysisBeamActor* Beam = nullptr;
			for (TActorIterator<ADysisBeamActor> It(World); It; ++It)
				if ((*It)->bFromMirror) { Beam = *It; break; }

			UE_LOG(LogTemp, Display, TEXT("Dysis.MirrorInfo：slot=%d az=%.2f° pitch=%.2f° normal=(%.1f, %.1f, %.1f)"),
				Mirror->GetCurrentSlot(), Mirror->GetCurrentSlotAz(), Mirror->MirrorPitchDeg,
				Mirror->GetMirrorNormal().X, Mirror->GetMirrorNormal().Y, Mirror->GetMirrorNormal().Z);
			if (Beam)
			{
				UE_LOG(LogTemp, Display, TEXT("  反射光：dir=(%.3f, %.3f, %.3f) tip=(%.0f, %.0f, %.0f)（对照 L3 东南点位用 Dysis.Go 站过去核）"),
					Beam->GetTravelDir().X, Beam->GetTravelDir().Y, Beam->GetTravelDir().Z,
					Beam->GetTipCm().X, Beam->GetTipCm().Y, Beam->GetTipCm().Z);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  场上没有 bFromMirror=true 的光柱"));
			}
		}));
}
