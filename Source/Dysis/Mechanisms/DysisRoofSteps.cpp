#include "DysisRoofSteps.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 灰盒 smoothstep(a, b, x)：a<b 的标准形（SkyActor.cpp 同款）。
	double SmoothStep(double A, double B, double X)
	{
		const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}
	double WrapRoofAz(double A)   // 避免与 SkyLibrary 的 Wrap360 在 unity build 冲突
	{
		double R = FMath::Fmod(A, 360.0);
		if (R < 0.0) R += 360.0;
		return R;
	}

	// ── 控制台：Dysis.Night（调试）——强制接光（SetNight(true)），并触发 Lumen 强制刷新（调研 §12.1：
	// 全局光照变化要数秒传播，切主光当帧刷一次）。M4 真正的触发（苹果+岛影判定）接上后此命令留作跳关调试。
	FAutoConsoleCommandWithWorldAndArgs GDysisNight(
		TEXT("Dysis.Night"),
		TEXT("调试：强制入夜（等价接住最后一缕光）——SetNight(true) + Lumen 强刷"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UDysisTimeComponent* Time = nullptr;
			if (World)
				for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
					if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
						if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) break;
			if (!Time) { UE_LOG(LogTemp, Warning, TEXT("Dysis.Night：没找到玩家的 DysisTimeComponent（要 PIE 里）")); return; }
			Time->SetNight(true);
			if (APlayerController* PC = World->GetFirstPlayerController())
				PC->ConsoleCommand(TEXT("r.LumenScene.Lighting.ForceLightingUpdate 1"));
			UE_LOG(LogTemp, Display, TEXT("Dysis.Night：已入夜（Sticky=%f）"), Time->Sticky);
		}));
}

ADysisRoofSteps::ADysisRoofSteps()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisRoofSteps::GatherParts()
{
	if (!GetWorld()) return;
	// 名字匹配要排掉子件：上行名集 = 踏面(UpNN) / 铜沿(UpNN_Curb) / 柱身(UpNN_Shaft)，下行还带 Curb。
	// 铜沿挂在踏面下随父动，不用收；只收踏面（排 _Shaft/_Curb）和柱身（含 _Shaft）。
	const auto Collect = [this](bool bWantShaft, const TCHAR* Family, TArray<FPart>& Out)
	{
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			AStaticMeshActor* Actor = *It;
			if (!Actor || !Actor->GetName().Contains(Family)) continue;
			const bool bIsShaft = Actor->GetName().Contains(TEXT("_Shaft"));
			const bool bIsCurb = Actor->GetName().Contains(TEXT("_Curb"));
			if (bIsShaft != bWantShaft || bIsCurb) continue;
			FPart P;
			P.Actor = Actor;
			P.Orig = Actor->GetActorTransform();
			P.CurrentY_M = double(Actor->GetActorLocation().Z) / 100.0;
			Out.Add(P);
		}
	};
	Collect(false, TEXT("RoofSteps_Up"), UpSteps);
	Collect(true, TEXT("RoofSteps_Up"), UpShafts);
	Collect(false, TEXT("RoofSteps_Dn"), DnSteps);
	// 名字两位零填充（Up01…Up16 / Dn01…Dn16），字典序=数字序。
	UpSteps.Sort([](const FPart& A, const FPart& B) { return A.Actor->GetName() < B.Actor->GetName(); });
	UpShafts.Sort([](const FPart& A, const FPart& B) { return A.Actor->GetName() < B.Actor->GetName(); });
	DnSteps.Sort([](const FPart& A, const FPart& B) { return A.Actor->GetName() < B.Actor->GetName(); });

	if (UpSteps.Num() != 16) UE_LOG(LogTemp, Warning, TEXT("DysisRoofSteps：上行踏面找到 %d 个（应为 16，名字要含 RoofSteps_Up）"), UpSteps.Num());
	if (UpShafts.Num() != 16) UE_LOG(LogTemp, Warning, TEXT("DysisRoofSteps：上行柱身找到 %d 个（应为 16，名字要含 _Shaft）"), UpShafts.Num());
	if (DnSteps.Num() != 16) UE_LOG(LogTemp, Warning, TEXT("DysisRoofSteps：下行踏面找到 %d 个（应为 16，名字要含 RoofSteps_Dn）"), DnSteps.Num());
}

void ADysisRoofSteps::BeginPlay()
{
	Super::BeginPlay();
	GatherParts();

	// 已知坑①：导入姿态是升起的——开局把上行 16 级归零到 30.30 平面（RING_Y），柱身藏起（高 0）。
	const double RingY = UDysisSkyLibrary::DysisConst(TEXT("RING_Y"));
	for (int32 k = 0; k < UpSteps.Num(); ++k)
	{
		if (AStaticMeshActor* Step = UpSteps[k].Actor.Get())
		{
			FVector Loc = Step->GetActorLocation();
			Loc.Z = RingY * 100.0f;
			Step->SetActorLocation(Loc);
			UpSteps[k].CurrentY_M = RingY;
		}
	}
	for (FPart& Shaft : UpShafts)
	{
		if (AStaticMeshActor* Actor = Shaft.Actor.Get())
		{
			Actor->SetActorHiddenInGame(true);   // 高 0 = 藏（scale 0 会除零，别用）
			Shaft.CurrentY_M = 0.0;
		}
	}
	// 下行开局在环道平面（导入即此姿态，机关清单：摆的是白天样子）。
	for (FPart& P : DnSteps) if (AStaticMeshActor* A = P.Actor.Get()) P.CurrentY_M = double(A->GetActorLocation().Z) / 100.0;
}

UDysisTimeComponent* ADysisRoofSteps::ResolveTime()
{
	if (APawn* Pawn = (GetWorld() && GetWorld()->GetFirstPlayerController()) ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
		return Pawn->FindComponentByClass<UDysisTimeComponent>();
	return nullptr;
}

void ADysisRoofSteps::MoveStepTo(AStaticMeshActor* Step, double TargetY_M, float DeltaTime, double& CurrentY_M)
{
	const double NewY = FMath::FInterpConstantTo(CurrentY_M, TargetY_M, DeltaTime, MoveSpeedCmPerSec / 100.0);
	FVector Loc = Step->GetActorLocation();
	Loc.Z = float(NewY * 100.0);
	Step->SetActorLocation(Loc);
	CurrentY_M = NewY;
}

void ADysisRoofSteps::DriveUp(float DeltaTime)
{
	UDysisTimeComponent* Time = ResolveTime();
	if (!Time || UpSteps.Num() == 0) return;

	// t = smoothstep(0, UP_SPAN, 人从细桥落脚顺时针走过的角度)；夜里锁全开（t=1，接光台在顶）。
	double t = 1.0;
	if (!Time->bNight)
	{
		const double Az = WrapRoofAz(FMath::Atan2(Time->FootCm.Y, Time->FootCm.X) * 180.0 / UE_DOUBLE_PI);
		t = SmoothStep(0.0, UDysisSkyLibrary::DysisConst(TEXT("UP_SPAN")), WrapRoofAz(Az - LandAzDeg));
	}

	const double RingY = UDysisSkyLibrary::DysisConst(TEXT("RING_Y"));   // 30.3
	const double CeilY = 29.5;                                           // CEIL，天花（柱身底）
	for (int32 k = 0; k < UpSteps.Num(); ++k)
	{
		if (AStaticMeshActor* Step = UpSteps[k].Actor.Get())
		{
			const double TargetY = RingY + 0.45 * double(k + 1) * t;      // 第 k+1 级：30.3+0.45(k+1)
			MoveStepTo(Step, TargetY, DeltaTime, UpSteps[k].CurrentY_M);
			// 柱身：从天花长到踏面底（比例缩放，轴心在底=只向上长）；高不足 1cm 就藏。
			if (k < UpShafts.Num())
				if (AStaticMeshActor* Shaft = UpShafts[k].Actor.Get())
				{
					const double HeightM = UpSteps[k].CurrentY_M - CeilY;
					const double OrigHeightM = double(UpSteps[k].Orig.GetLocation().Z) / 100.0 - CeilY;   // 导入（升起）时的高
					Shaft->SetActorHiddenInGame(HeightM < 0.01);
					if (HeightM >= 0.01 && OrigHeightM > 0.01)
					{
						FVector Scale = Shaft->GetActorScale3D();
						const double OrigStepH = double(UpSteps[k].Orig.GetLocation().Z) / 100.0 - CeilY;
						Scale.Z = float(UpShafts[k].Orig.GetScale3D().Z * HeightM / OrigStepH);
						Shaft->SetActorScale3D(Scale);
					}
				}
		}
	}
}

void ADysisRoofSteps::DriveDown(float DeltaTime)
{
	UDysisTimeComponent* Time = ResolveTime();
	if (!Time) return;

	if (Time->bNight)
	{
		if (NightTimer < 0.0) NightTimer = NightDropDelaySeconds;   // 接光那刻起表
		NightTimer -= DeltaTime;
		if (NightTimer > 0.0) return;
		for (int32 k = 0; k < DnSteps.Num() && k < DnTargetY.Num(); ++k)
			if (AStaticMeshActor* Step = DnSteps[k].Actor.Get())
				MoveStepTo(Step, DnTargetY[k], DeltaTime, DnSteps[k].CurrentY_M);
	}
}

void ADysisRoofSteps::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	DriveUp(DeltaTime);
	DriveDown(DeltaTime);
}
