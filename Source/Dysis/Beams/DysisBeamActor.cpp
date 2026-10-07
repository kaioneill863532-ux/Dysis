#include "DysisBeamActor.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Sky/DysisMPCComponent.h"
#include "Optics/DysisOpticsLibrary.h"
#include "Mechanisms/DysisMirrorSource.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADysisBeamActor::ADysisBeamActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;   // 时间组件（也在 PostPhysics）算完 H 再摆光
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 引擎方块当占位（100 cm 立方，缩放即长宽高）。美术后换成品光柱网格；两盒都 No Nanite（P1-10）。
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetMobility(EComponentMobility::Movable);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Visual->SetStaticMesh(Cube.Object);

	Collision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Collision"));
	Collision->SetupAttachment(RootComponent);
	Collision->SetMobility(EComponentMobility::Movable);
	Collision->SetCollisionProfileName(TEXT("BlockAll"));   // 地板判定走 Pawn 通道扫胶囊，BlockAll 已挡
	Collision->SetCastShadow(false);
	if (Cube.Succeeded()) Collision->SetStaticMesh(Cube.Object);

	// §4 镜光两端封闭碰撞盒（规格书"三相像的镜光除了两头都拦着"）。
	// 窗端（起点）和落点端（终点）各一个薄方块，bMirrorBeam=true 时才开碰撞。
	EndCapStart = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EndCapStart"));
	EndCapStart->SetupAttachment(RootComponent);
	EndCapStart->SetMobility(EComponentMobility::Movable);
	EndCapStart->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 默认关——镜光才有
	EndCapStart->SetCastShadow(false);
	if (Cube.Succeeded()) EndCapStart->SetStaticMesh(Cube.Object);

	EndCapEnd = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EndCapEnd"));
	EndCapEnd->SetupAttachment(RootComponent);
	EndCapEnd->SetMobility(EComponentMobility::Movable);
	EndCapEnd->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndCapEnd->SetCastShadow(false);
	if (Cube.Succeeded()) EndCapEnd->SetStaticMesh(Cube.Object);
}

void ADysisBeamActor::BeginPlay()
{
	Super::BeginPlay();
	// §6 序·登殿开场引导光：光从神殿里伸到岛上——玩家"在小岛上往神殿迈一步"顺着光走上台地。
	// 关卡按施工图光路表摆（窗心 23.2°/31.8/-0.8，岛面 -12.0），这里不覆盖任何已设参数；
	// 时间冻结在 beam:isle（H_I=13:29），光方向不随 H 变。
	if (bPrologueBeam)
	{
		BeamZone = TEXT("beam:isle");   // 时间系统踩上即 H=H_I（SkyLibrary 第 106 行）
		if (WindowCm.IsZero())
		{
			// 未在关卡里填时才用代码默认（PIE 手搓测试用）。
			WindowCm = UDysisOpticsLibrary::AzRyToCm(23.2, 31.8, -0.8);
			FallbackFloorY = -12.0;
		}
	}
	// 打区域 Tag：时间组件踩到本 Actor 会读 Tag "DysisZone=beam:b1"，和墙体 Tag 同一套机制。
	if (!BeamZone.IsNone())
		Tags.Add(FName(FString(TEXT("DysisZone=")) + BeamZone.ToString()));
	TryAddTimePrerequisite();
}

void ADysisBeamActor::TryAddTimePrerequisite()
{
	if (bPrereqSet || !GetWorld()) return;
	// 保证 beam 的 Tick 排在玩家时间组件之后（先有新 H 再摆光）。Pawn 由 GameMode 生成可能晚于本 Actor 的
	// BeginPlay，所以第一次没找到时在 Tick 里重试。（5.8 已删 UWorld::GetFirstPlayerPawn，走 Controller。）
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		if (UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>())
		{
			AddTickPrerequisiteComponent(Time);
			bPrereqSet = true;
		}
}

void ADysisBeamActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TryAddTimePrerequisite();
	if (!CachedSky.IsValid() && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) CachedSky = *It;
	}
	if (!CachedSky.IsValid()) return;
	UpdateFor(CachedSky->GetTime());
}

void ADysisBeamActor::UpdateFor(double H)
{
	// 方向：直射光 = 逆太阳（光从窗往屋里走）；镜段 = 前进方向关于镜面法线反射（§9.2/§8.8）。
	// 镜面法线优先取三相像（MirrorSource），没有再用手填 MirrorNormal。
	TravelDir = -UDysisSkyLibrary::DysisSunDir(float(H));
	if (bFromMirror)
	{
		if (MirrorSource) MirrorNormal = MirrorSource->GetMirrorNormal();
		TravelDir = UDysisOpticsLibrary::ReflectDir(TravelDir, MirrorNormal);
	}
	TravelDir = TravelDir.GetSafeNormal();

	// P1-1 护栏：本帧方向相对上一帧最多转 MaxDirDegPerSec·dt——正常步行远达不到，只在 H 异常跳变
	// （接缝/Tag 遗漏）时把"光甩人"限制成可控摆动，动桥反馈循环不至于发散。
	if (!bDirInit) { PrevDir = TravelDir; bDirInit = true; }
	else
	{
		const FVector3d Prev(PrevDir), Want(TravelDir);
		const double CosA = FMath::Clamp(Prev.Dot(Want), -1.0, 1.0);
		const double AngleDeg = FMath::Acos(CosA) / (UE_DOUBLE_PI / 180.0);
		const double CapDeg = MaxDirDegPerSec * 0.016;   // 帧率无关化：按 60fps 帧步长近似（Tick 无 dt 传入 UpdateFor）
		if (AngleDeg > CapDeg && AngleDeg > 1e-4)
		{
			const FVector3d Axis = FVector3d::CrossProduct(Prev, Want);
			if (Axis.SizeSquared() > 1e-12)
				TravelDir = FVector(FQuat4d(Axis.GetUnsafeNormal(), CapDeg * UE_DOUBLE_PI / 180.0).RotateVector(Prev));
		}
	}
	PrevDir = TravelDir;

	// 长度：trace 优先，打不到落到兜底地面。
	double NewLen = 0.0;
	const bool bHit = SolveLength(TravelDir, NewLen);
	LenCm = NewLen;

	// 坡度门 + §4 四条全量判定：
	//   ① 光有效——窗口采样照亮比例 ≥30%（SampleWindowIllumination 多点 trace）
	//   ② 够长——LenCm >= MinLengthCm
	//   ③ 坡度——SlopeDeg <= Limit（35°/38°）
	//   ④ 雾覆盖——MistLevel >= MistThreshold（25%/15%）
	// 宽限（灰盒：刚踏空 0.14 s 里还站得住）：移动组件解钉那刻起表，期间光不塌。
	const bool bGrace = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(GraceTimer);

	// §4① 窗口真实采样：在窗截面取 3×3=9 个采样点，逐点沿 -SunDir 打 trace；
	// 规格书原话"在窗的内墙面洞口上取样，朝太阳方向打射线，照亮 30% 以上才算有光"。
	const FVector SunDir = UDysisSkyLibrary::DysisSunDir(float(H));
	WindowIllumination = SampleWindowIllumination(SunDir);

	const double SlopeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(FMath::Abs(TravelDir.Z), 0.0, 1.0)));
	const double Limit = (bSomeoneStanding || bGrace) ? SlopeMaxStandingDeg : SlopeMaxDeg;
	const double MistGate = (bSomeoneStanding || bGrace) ? MistThresholdStanding : MistThreshold;
	// 圆眼光柱方向朝上（走的是上表面）——坡度取正向、光长也要够。
	const bool bSlopeOK = bOculusBeam ? (SlopeDeg >= 45.0 && SlopeDeg <= 90.0) : (SlopeDeg <= Limit);
	// §4① 照亮门槛：窗光按窗洞采样；镜光按规格书"镜面照亮一半以上"用三相像的镜面采样（9 点 ≥50%）。
	const double Illumination = (bFromMirror && MirrorSource) ? MirrorSource->GetMirrorIllumination() : WindowIllumination;
	bWalkable = bHit
	         && bSlopeOK
	         && LenCm >= MinLengthCm                                          // §4② 够长
	         && Illumination >= (bFromMirror ? 0.5 : WindowIlluminationThreshold)   // §4① 照亮
	         && MistLevel >= float(MistGate);                                 // §4④ 雾覆盖

	SetActorLocation(WindowCm);
	SetActorRotation(TravelDir.Rotation());

	// 引擎方块 100 cm：缩放 = 全尺寸/100；本地 X 沿光——方块中心外移半长，让"窗端面"贴在 WindowCm（铰链在窗）。
	const double LenFull = FMath::Max(LenCm, 1.0);
	const double Extra = 2.0 * WalkExtraCm;
	Visual->SetRelativeLocation(FVector(LenFull * 0.5, 0, 0));
	Visual->SetRelativeScale3D(FVector(LenFull, BoxExtentCm.Y * 2.0, BoxExtentCm.Z * 2.0) / 100.0);
	Collision->SetRelativeLocation(FVector(LenFull * 0.5, 0, 0));
	Collision->SetRelativeScale3D(FVector(LenFull, BoxExtentCm.Y * 2.0 + Extra, BoxExtentCm.Z * 2.0 + Extra) / 100.0);

	Visual->SetVisibility(bWalkable && MistLevel > 0.01f);
	// 碰撞在宽限期内保持开启（人已解钉但可能还没落稳——"还站得住"）。
	Collision->SetCollisionEnabled((bWalkable || bGrace) ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// §4 镜光两端封闭（规格书原话"除了两头都拦着"）：bMirrorBeam=true 时窗端和落点端各加一块薄封闭板。
	if (bMirrorBeam && EndCapStart && EndCapEnd)
	{
		EndCapStart->SetCollisionEnabled(bWalkable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		EndCapEnd->SetCollisionEnabled(bWalkable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		// 窗端封板：在 WindowCm 处（本地 X=0），截面同碰撞盒宽度。
		EndCapStart->SetRelativeLocation(FVector(0, 0, 0));
		EndCapStart->SetRelativeScale3D(FVector(10.0, BoxExtentCm.Y * 2.0 + 2.0 * WalkExtraCm, BoxExtentCm.Z * 2.0 + 2.0 * WalkExtraCm) / 100.0);
		// 落点端封板：在光末端（本地 X=LenCm），同宽。
		EndCapEnd->SetRelativeLocation(FVector(float(LenFull), 0, 0));
		EndCapEnd->SetRelativeScale3D(FVector(10.0, BoxExtentCm.Y * 2.0 + 2.0 * WalkExtraCm, BoxExtentCm.Z * 2.0 + 2.0 * WalkExtraCm) / 100.0);
	}

	// §4 圆眼顶端检测：玩家 Z 到达光柱顶端 → MPC 写 OpenT=1 触发光圈叶片张开（运行时 MPC）。
	if (bOculusBeam && bWalkable)
	{
		if (APawn* Pawn = (GetWorld() && GetWorld()->GetFirstPlayerController()) ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
		{
			const double PlayerZ = Pawn->GetActorLocation().Z;
			const double TopZ = WindowCm.Z - 200.0;
			if (PlayerZ >= TopZ)
			{
				// MPC 写 OpenT=1（运行时 MPC 已由 MPCFactory 创建——材质图 Collection Parameter 节点按 "OpenT" 引用）。
				if (GetWorld())
				{
					TActorIterator<APawn> ItPawn(GetWorld());
					if (ItPawn)
						if (UDysisMPCComponent* MPC = ItPawn->FindComponentByClass<UDysisMPCComponent>())
							if (MPC->Collection)
								UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), MPC->Collection, TEXT("OpenT"), 1.0f);
				}
				UE_LOG(LogTemp, Display, TEXT("Dysis 圆眼：玩家到达顶端 → MPC OpenT=1"));
			}
		}
	}
}

FDysisBeamParam ADysisBeamActor::WorldToParam(const FVector& PointCm) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(PointCm);
	FDysisBeamParam P;
	P.S = double(Local.X);
	P.E = double(Local.Y);
	P.D = double(Local.Z);
	return P;
}

FVector ADysisBeamActor::ParamToWorld(const FDysisBeamParam& P) const
{
	return GetActorTransform().TransformPosition(FVector(P.S, P.E, P.D));
}

void ADysisBeamActor::NotifyStanding(bool bStanding)
{
	bSomeoneStanding = bStanding;
	// 踏空宽限（P1-4）：离开光那刻起 0.14 s 内光不塌、碰撞不关；站着则清掉倒计时。
	if (GetWorld())
	{
		if (bStanding) GetWorld()->GetTimerManager().ClearTimer(GraceTimer);
		else           GetWorld()->GetTimerManager().SetTimer(GraceTimer, []() {}, FMath::Max(StandingGraceSeconds, 0.0f), false);
	}}

bool ADysisBeamActor::SolveLength(const FVector& InDir, double& OutLenCm) const
{
	if (!GetWorld()) return false;
	FHitResult Hit;
	FCollisionQueryParams Params(TEXT("DysisBeam"), false, this);
	const FVector Start = WindowCm + InDir * 10.0;                 // 从窗外一点开始，避免自撞窗框
	const FVector End = WindowCm + InDir * MaxLengthCm;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutLenCm = FVector::Dist(WindowCm, Hit.ImpactPoint);
		return true;
	}
	// 兜底：与高度 FallbackFloorY 的水平面求交（打不到任何东西的长光，如跨中庭）。
	if (InDir.Z < -1e-4)
	{
		const double T = (FallbackFloorY * 100.0 - double(WindowCm.Z)) / double(InDir.Z);
		if (T > 0.0 && T <= MaxLengthCm) { OutLenCm = T; return true; }
	}
	return false;
}

double ADysisBeamActor::SampleWindowIllumination(const FVector& SunDir) const
{
	if (!GetWorld()) return 1.0;   // 无 World（离线判定）退化为全亮

	// §4① 规格书原话"在窗的内墙面洞口上取样，朝太阳方向打射线"：
	// 窗洞 = 窗台（WindowCm.Z）上方的方洞（施工图：方洞高 = 截面高，拱在其上），
	// 所以采样网格中心取窗台上方 BoxExtentCm.Z 处（网格铺满整个方洞），
	// 并向边缘内缩 25%——贴着窗框的采样点会被框"擦边"挡住，把照亮比例压到阈值以下。
	// 返回被照亮的采样点比例（0–1），bWalkable 判 >= WindowIlluminationThreshold（30%）。
	constexpr int32 GridN = 3;
	constexpr double Inset = 0.75;   // 采样范围 = ±0.75 × 半截面（防窗框擦边）
	int32 LitCount = 0;
	const double HoleCenterZ = WindowCm.Z + BoxExtentCm.Z;   // 方洞竖向中心（窗台之上）
	for (int32 iy = 0; iy < GridN; ++iy)
	{
		for (int32 iz = 0; iz < GridN; ++iz)
		{
			const double FY = ((double(iy) / double(GridN - 1)) * 2.0 - 1.0) * Inset;
			const double FZ = ((double(iz) / double(GridN - 1)) * 2.0 - 1.0) * Inset;
			// 采样点世界坐标：窗洞内 YZ 平面均匀分布（X 方向不动——采样在窗面上）。
			const FVector SamplePoint = FVector(WindowCm.X, WindowCm.Y + FY * BoxExtentCm.Y, HoleCenterZ + FZ * BoxExtentCm.Z);
			FHitResult SampleHit;
			FCollisionQueryParams SampleParams(TEXT("DysisWindowSample"), false, this);
			// 从采样点朝太阳方向打 50m——打不到任何东西 = 太阳照到了这个点。
			if (!GetWorld()->LineTraceSingleByChannel(SampleHit, SamplePoint, SamplePoint + SunDir * 5000.0, ECC_Visibility, SampleParams))
				++LitCount;
		}
	}
	return double(LitCount) / double(GridN * GridN);
}
