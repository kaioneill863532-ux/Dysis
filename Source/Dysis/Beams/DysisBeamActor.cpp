#include "DysisBeamActor.h"
#include "DysisGreybox.h"
#include "World/DysisWorldState.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
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

namespace
{
	// ───── 灰盒 v0.12 的窗（WIN）和各束光的设置（BEAMS），厘米 / 度 ─────
	struct FGreyboxWindow
	{
		const TCHAR* Id;
		float Az;                 // 窗心的方位角
		float WidthCm, HeightCm;  // 窗洞宽、高
		float Z0Cm;               // 窗台高度
		bool bHarp;               // 光阶的三扇窗：只在光阶那一段时间里有光
		bool bCanWalk;
		float Gain;
		bool bHasFloor; float FloorZ;   // 这束光要落到的那层地面（用来算“够不够长”）
		bool bHasMinZ; float MinZ;      // 只在这个高度以上能踩
	};
	const FGreyboxWindow GWindows[] = {
		{ TEXT("isle"), 206.0f,       200.0f, 165.0f, 2655.0f,   false, true,  1.0f, false, 0.0f,    false, 0.0f },
		{ TEXT("b1"),   164.0f,       240.0f, 240.0f, 940.0f,    false, true,  1.0f, true,  0.0f,    false, 0.0f },
		{ TEXT("b2"),   250.0f,       180.0f, 220.0f, 1830.0f,   false, true,  1.0f, true,  600.0f,  false, 0.0f },
		{ TEXT("iris"), 166.0f,       180.0f, 220.0f, 1750.0f,   false, false, 0.9f, false, 0.0f,    false, 0.0f },
		{ TEXT("c"),    267.303855f,  180.0f, 220.0f, 1756.2045f, false, false, 0.8f, false, 0.0f,    false, 0.0f },
		{ TEXT("h1"),   180.0f,       200.0f, 180.0f, 2640.0f,   true,  true,  1.0f, true,  1450.0f, false, 0.0f },
		{ TEXT("h2"),   193.0f,       200.0f, 180.0f, 2640.0f,   true,  true,  1.0f, true,  1770.0f, true,  1770.0f },
		{ TEXT("h3"),   206.0f,       200.0f, 180.0f, 2640.0f,   true,  true,  1.0f, true,  1770.0f, true,  1770.0f },
	};
	constexpr double GB_H_H0 = 32.5, GB_H_H1 = 35.8;            // 光阶那一段时间
	constexpr double GB_TERR_Z = -120.0, GB_TERR_EDGE_N = 3205.828;   // 北面台地的高度、崖边离内墙多远
	constexpr double GB_BEAM_PAD = 35.0;                         // 能踩的面比光两侧各宽 0.35 m
	constexpr double GB_FAR = 12000.0;                           // 光最远照 120 m
	// 三相像的镜光：灰盒 CROSS.T / CROSS.Mc —— 光要够到的那一点（日之龛石台内沿往里 0.35 m、台面上 0.3 m），和它离镜子下沿的距离
	const FVector GB_MIRROR_CROSS_T(-136.305, 1296.857, 2030.0);
	constexpr double GB_MIRROR_CROSS_DIST = 2629.0;
	constexpr double GB_MIRROR_WALK_PAST = 45.0;                 // 能踩的面比那一点再多铺 0.45 m

	int32 FindGreyboxWindow(const FString& Id)
	{
		if (Id.IsEmpty()) return -1;
		for (int32 i = 0; i < UE_ARRAY_COUNT(GWindows); ++i)
			if (Id.Equals(GWindows[i].Id, ESearchCase::IgnoreCase)) return i;
		return -1;
	}
}

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
	EndCapStart->SetHiddenInGame(true);   // 只是碰撞用的盒子，游戏里不该看见（以前没藏，每束光的位置上都有一个灰方块）

	EndCapEnd = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EndCapEnd"));
	EndCapEnd->SetupAttachment(RootComponent);
	EndCapEnd->SetMobility(EComponentMobility::Movable);
	EndCapEnd->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndCapEnd->SetCastShadow(false);
	if (Cube.Succeeded()) EndCapEnd->SetStaticMesh(Cube.Object);
	EndCapEnd->SetHiddenInGame(true);
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
	// 窗光按灰盒算：名字取 GreyboxId；没填就看区域名（beam:b1 → b1），开场的光是 isle。
	{
		FString Id = GreyboxId.IsNone() ? FString() : GreyboxId.ToString();
		if (Id.IsEmpty() && bPrologueBeam) Id = TEXT("isle");
		if (Id.IsEmpty() && !bFromMirror && !bOculusBeam && BeamZone.ToString().StartsWith(TEXT("beam:"))) Id = BeamZone.ToString().RightChop(5);
		if (Id.Equals(TEXT("mmoon"), ESearchCase::IgnoreCase)) GreyboxMirror = 2;
		else if (bFromMirror || Id.Equals(TEXT("mirror"), ESearchCase::IgnoreCase)) { GreyboxMirror = 1; Id = TEXT("mirror"); }
		GreyboxIndex = FindGreyboxWindow(Id);
		bGreyboxOculus = GreyboxIndex < 0 && GreyboxMirror == 0 && (bOculusBeam || Id.Equals(TEXT("oculus"), ESearchCase::IgnoreCase));
		if (bGreyboxOculus) Id = TEXT("oculus");
		if (IsGreybox())
		{
			GreyboxId = FName(*Id);
			BeamZone = FName(*(FString(TEXT("beam:")) + Id));
			// 能踩的面只挡人：不挡光线（光路自己也要打光线）、不挡镜头
			Collision->SetCollisionObjectType(ECC_WorldDynamic);
			Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
			Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Collision->SetVisibility(false);
			Collision->SetHiddenInGame(true);
			Visual->SetVisibility(false);
			if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Beams/M_DysisBeam.M_DysisBeam")))
				VisualMID = Visual->CreateDynamicMaterialInstance(0, Mat);
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
	if (IsGreybox())
	{
		// 灰盒：时刻变了才重算（|ΔH| > 0.004）；机关的石板在挪的时候时刻不变，所以另外每 0.2 秒也重算一次。
		// 开场的光伸出来、雾升起来是随时间变的：这两样每帧都更新（不用重新打光线）。
		const double H = CachedSky->GetTime();
		SolveClock += DeltaTime;
		const UDysisWorldState* State = UDysisWorldState::Get(this);
		const int32 LightSerial = State ? State->LightSerial : 0;
		const bool bLightMoved = LightSerial != SeenLightSerial;
		SeenLightSerial = LightSerial;
		if (bLightMoved || FMath::Abs(H - LastSolvedH) > 0.004 || SolveClock > 0.2) UpdateGreybox(H);
		else
		{
			ApplyIsleGrow();
			bWalkable = ComputeGreyboxWalkable();
			ApplyGreyboxComponents();
		}
		return;
	}
	UpdateFor(CachedSky->GetTime());
}

void ADysisBeamActor::UpdateFor(double H)
{
	if (IsGreybox()) { UpdateGreybox(H); return; }
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
	if (IsGreybox())
	{
		FDysisBeamParam G;
		WorldToStrip(PointCm, G.E, G.S);
		return G;
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(PointCm);
	FDysisBeamParam P;
	P.S = double(Local.X);
	P.E = double(Local.Y);
	P.D = double(Local.Z);
	return P;
}

FVector ADysisBeamActor::ParamToWorld(const FDysisBeamParam& P) const
{
	if (IsGreybox()) return StripToWorld(P.E, P.S);
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

// ───────────────────────── 窗光：灰盒 v0.12 的算法 ─────────────────────────

double ADysisBeamActor::CastLight(const FVector& Start, const FVector& Dir, double Far) const
{
	const UWorld* World = GetWorld();
	if (!World) return -1.0;
	FCollisionQueryParams Params(TEXT("DysisLight"), /*bTraceComplex*/ true);
	if (const APlayerController* PC = World->GetFirstPlayerController())
		if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);   // 人不挡光路
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);   // 别的光（它们的踩踏面）也不挡
	if (GreyboxMirror != 0)
		if (const UDysisWorldState* MirrorState = UDysisWorldState::Get(this))
			for (const TWeakObjectPtr<AActor>& A : MirrorState->Mirror.Self) if (A.IsValid()) Params.AddIgnoredActor(A.Get());   // 雕像和镜子自己不挡自己的光
	if (bGreyboxOculus)
	{
		// 圆眼光柱是从光圈中间的洞下来的：叶片和屋顶细桥不算挡它（灰盒 b._blk）
		if (!bOculusIgnoreBuilt)
		{
			bOculusIgnoreBuilt = true;
			for (TActorIterator<AActor> It(World); It; ++It)
				for (const FName& Tag : It->Tags)
				{
					const FString T = Tag.ToString();
					if (T.StartsWith(TEXT("SM_Mech_IrisBlades_")) || T == TEXT("SM_RoofBridge")) { OculusLightIgnore.Add(*It); break; }
				}
		}
		for (const TWeakObjectPtr<AActor>& A : OculusLightIgnore) if (A.IsValid()) Params.AddIgnoredActor(A.Get());
	}
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, Start + Dir * Far, ECC_Visibility, Params)) return Hit.Distance;
	return -1.0;
}

void ADysisBeamActor::UpdateGreybox(double H)
{
	LastSolvedH = H;
	SolveClock = 0.0;
	++FrameSerial;
	bFrameValid = false; bClean = false; Lit = 0.0; Need = 0.0;
	if (bGreyboxOculus) { UpdateGreyboxOculus(H); return; }
	if (GreyboxMirror != 0) { UpdateGreyboxMirror(H); return; }
	const FGreyboxWindow& W = GWindows[GreyboxIndex];
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const bool bNight = State && State->IsNight();
	const bool bIsle = GreyboxIndex == 0;

	auto Done = [this]()
	{
		bWalkable = ComputeGreyboxWalkable();
		ApplyGreyboxComponents();
	};

	const FVector Sun = UDysisSkyLibrary::DysisSunDir(float(H));   // 指向太阳的单位向量
	if (Sun.Z < 0.003) { Done(); return; }
	if (FMath::Abs(DysisGB::AngDiff(DysisGB::AzOf(Sun), W.Az)) > 85.0) { Done(); return; }
	if (W.bHarp && !bNight && (H < GB_H_H0 - 1.5 || H > GB_H_H1 + 1.5)) { Done(); return; }

	// 窗洞上 8 × 5 个取样点，各朝太阳打一条光线：没被挡住的算照亮；岛影以下的不算
	const double ShadowZ = UDysisWorldState::ShadowZ(Sun);
	constexpr int32 NU = 8, NV = 5;
	const double HalfDeg = FMath::RadiansToDegrees((W.WidthCm * 0.5) / DysisGB::R_IN);
	double UMin = 2.0, UMax = -1.0, VMin = 2.0, VMax = -1.0;
	int32 LitCount = 0;
	for (int32 i = 0; i < NU; ++i)
		for (int32 j = 0; j < NV; ++j)
		{
			const double U = (i + 0.5) / NU, V = (j + 0.5) / NV;
			const FVector P = DysisGB::PolarCm(W.Az - HalfDeg + 2.0 * HalfDeg * U, DysisGB::R_IN - 2.0, FMath::Lerp(double(W.Z0Cm), double(W.Z0Cm + W.HeightCm), V));
			if (P.Z < ShadowZ || UDysisWorldState::InIsleShadow(P, Sun)) continue;
			if (CastLight(P + Sun * 0.1, Sun, 40000.0) < 0.0)
			{
				++LitCount;
				UMin = FMath::Min(UMin, U - 0.5 / NU); UMax = FMath::Max(UMax, U + 0.5 / NU);
				VMin = FMath::Min(VMin, V - 0.5 / NV); VMax = FMath::Max(VMax, V + 0.5 / NV);
			}
		}
	Lit = double(LitCount) / double(NU * NV);
	if (LitCount == 0) { Done(); return; }

	// 被照亮的那一块窗 → 光束的四个角；光前进的方向是逆着太阳
	const double A0 = W.Az - HalfDeg + 2.0 * HalfDeg * UMin, A1 = W.Az - HalfDeg + 2.0 * HalfDeg * UMax;
	double Z0 = FMath::Lerp(double(W.Z0Cm), double(W.Z0Cm + W.HeightCm), VMin);
	const double Z1 = FMath::Lerp(double(W.Z0Cm), double(W.Z0Cm + W.HeightCm), VMax);
	const FVector L = -Sun;
	const double Hz = FMath::Sqrt(L.X * L.X + L.Y * L.Y), Tn = Sun.Z / Hz;
	double WalkFrom = 0.0, NeedCm = 0.0;
	if (bIsle)
	{
		// 崖边把光束的下半截切掉：能踩的下表面正好擦过崖边，人从岛上走上来，在崖边踏上台地
		const double X = DysisGB::R_IN + GB_TERR_EDGE_N;
		Z0 = FMath::Max(Z0, GB_TERR_Z + 18.0 + X * Tn);
		if (Z0 > Z1 - 20.0) { Done(); return; }
		WalkFrom = (X - 30.0) / Hz;
		NeedCm = (Z0 - DysisGB::ISLAND_TOP_Z) / Sun.Z - 80.0;
	}
	else if (W.bHasFloor) NeedCm = (W.Z0Cm - W.FloorZ) / FMath::Max(0.05, double(Sun.Z)) - 150.0;

	const FVector Corners[4] = { DysisGB::PolarCm(A0, DysisGB::R_IN, Z0), DysisGB::PolarCm(A1, DysisGB::R_IN, Z0), DysisGB::PolarCm(A0, DysisGB::R_IN, Z1), DysisGB::PolarCm(A1, DysisGB::R_IN, Z1) };
	SetGreyboxFrame(Corners, L, 5.0, WalkFrom, W.bHasMinZ, W.MinZ);
	if (bIsle)
	{
		for (int32 k = 0; k < 4; ++k) FrameCFull[k] = FrameC[k];
		ApplyIsleGrow();
	}
	// 人已经踩在这束光上：光脚爬上了墙也不要紧，只要人脚下往下一段还没被挡住
	if (bSomeoneStanding)
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (const ACharacter* C = Cast<ACharacter>(PC->GetPawn()))
			{
				const FVector Foot = C->GetActorLocation() - FVector(0.0, 0.0, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				NeedCm = FMath::Min(NeedCm, FVector::DotProduct(Foot - FrameO, L) - 200.0);
			}
	Need = NeedCm;
	double MinEdge = FrameEdge[0];
	for (int32 i = 1; i < 7; ++i) MinEdge = FMath::Min(MinEdge, FrameEdge[i]);
	bClean = W.bCanWalk && Lit > 0.3 && MinEdge >= FMath::Min(NeedCm, 10000.0);
	Done();
}

void ADysisBeamActor::SetGreyboxFrame(const FVector Corners[4], const FVector& L, double StartOff, double WalkFrom, bool bHasMinZ, double MinZ)
{
	FrameO = Corners[0]; FrameU = Corners[1] - Corners[0]; FrameV = Corners[2] - Corners[0]; FrameL = L;
	// 四个角各自照多远
	for (int32 k = 0; k < 4; ++k)
	{
		const double Hit = CastLight(Corners[k] + L * StartOff, L, GB_FAR);
		FrameC[k] = Hit >= 0.0 ? Hit + StartOff : GB_FAR;
	}
	// 下沿上 7 个取样点：能踩的面只到最先被挡住的那一条为止
	double MinEdge = GB_FAR;
	for (int32 i = 0; i < 7; ++i)
	{
		const FVector P = FMath::Lerp(Corners[0], Corners[1], (i + 0.5) / 7.0);
		const double Hit = CastLight(P + L * StartOff, L, GB_FAR);
		FrameEdge[i] = Hit >= 0.0 ? Hit + StartOff : GB_FAR;
		MinEdge = FMath::Min(MinEdge, FrameEdge[i]);
	}
	const double F = -GB_BEAM_PAD / FrameU.Size();
	FrameB0 = Corners[0] + FrameU * F;
	FrameB1 = Corners[0] + FrameU * (1.0 - F);
	FrameS0 = WalkFrom;
	FrameS1 = FMath::Min(MinEdge + 5.0, bHasMinZ ? (Corners[0].Z - MinZ) / FMath::Max(0.05, -double(L.Z)) : 1.0e9);
	FrameFar = FVector::ZeroVector;
	for (int32 k = 0; k < 4; ++k) FrameFar += Corners[k] + L * FrameC[k];
	FrameFar *= 0.25;
	bFrameValid = true;
}

void ADysisBeamActor::ApplyIsleGrow()
{
	if (GreyboxIndex != 0 || !bFrameValid) return;
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const bool bStarted = State && State->bIsleGrowStarted;
	const float K = State ? State->IsleGrowK : 0.0f;
	double Full = FrameCFull[0];
	for (int32 k = 1; k < 4; ++k) Full = FMath::Max(Full, FrameCFull[k]);
	const double Lim = !bStarted ? 0.0 : (K >= 1.0f ? 1.0e9 : FMath::Lerp(2800.0, Full + 100.0, double(DysisGB::Smoothstep(0.0f, 1.0f, K))));
	const FVector C[4] = { FrameO, FrameO + FrameU, FrameO + FrameV, FrameO + FrameU + FrameV };
	FrameFar = FVector::ZeroVector;
	for (int32 k = 0; k < 4; ++k)
	{
		FrameC[k] = FMath::Min(FrameCFull[k], Lim);
		FrameFar += C[k] + FrameL * FrameC[k];
	}
	FrameFar *= 0.25;
}

void ADysisBeamActor::UpdateGreyboxOculus(double H)
{
	// 灰盒 computeOculusBeam。光圈半径 a 由机关总管写在世界状态里。
	auto Done = [this]()
	{
		bWalkable = ComputeGreyboxWalkable();
		ApplyGreyboxComponents();
	};
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const double A = State ? State->IrisACm : 40.0;
	const FVector Sun = UDysisSkyLibrary::DysisSunDir(float(H));
	constexpr double IrisZ0 = 3004.0, IrisZ1 = 3028.0, F3Z = 2300.0;
	if (A < 100.0 || Sun.Z < 0.01) { Done(); return; }
	if (IrisZ1 < UDysisWorldState::ShadowZ(Sun)) { Done(); return; }
	const FVector L = -Sun;
	const FVector Hd = FVector(L.X, L.Y, 0.0).GetSafeNormal();     // 光的水平方向
	const FVector Side(-Hd.Y, Hd.X, 0.0);                          // 和它垂直（灰盒 side = (−h.z, 0, h.x) 换成 UE 的轴）
	constexpr double Half = 95.0;
	const FVector Top = FVector(-Hd.X * A, -Hd.Y * A, IrisZ1), Bot = FVector(Hd.X * A, Hd.Y * A, IrisZ0);
	const FVector Corners[4] = { Top - Side * Half, Top + Side * Half, Bot - Side * Half, Bot + Side * Half };
	SetGreyboxFrame(Corners, L, 35.0, -30.0, false, 0.0);
	Lit = 1.0;
	Need = (IrisZ1 - F3Z) / FMath::Max(0.05, -double(L.Z)) - 50.0;
	double MinEdge = FrameEdge[0];
	for (int32 i = 1; i < 7; ++i) MinEdge = FMath::Min(MinEdge, FrameEdge[i]);
	bClean = MinEdge >= Need;
	Done();
}

void ADysisBeamActor::UpdateGreyboxMirror(double H)
{
	// 灰盒 computeMirrorBeam / mirrorLitBy / mirrorFrame。镜子的位置和朝向由机关总管写在世界状态里。
	auto Done = [this]()
	{
		bWalkable = ComputeGreyboxWalkable();
		ApplyGreyboxComponents();
	};
	UDysisWorldState* State = UDysisWorldState::Get(this);
	if (!State || !State->Mirror.bValid) { Done(); return; }
	FDysisMirrorState& M = State->Mirror;
	const bool bMoon = GreyboxMirror == 2;
	const int32 Form = bMoon ? 2 : 1;
	// 镜面上被照亮的那一块是日光、月光两束共用的一个量：各管各的那一相，不去清别人的
	if (M.Form != Form || M.W3[Form] < 0.95f) { if (M.LitBoxForm == Form) M.bHasLitBox = false; Done(); return; }
	M.bHasLitBox = false; M.LitBoxForm = Form;
	const FVector LightDir = bMoon ? UDysisSkyLibrary::DysisMoonDir(float(H)) : UDysisSkyLibrary::DysisSunDir(float(H));

	// 镜面上哪一块被照着（6 × 4 取样）
	double LitFrac = 0.0, U0 = 2.0, U1 = -1.0, V0 = 2.0, V1 = -1.0;
	if (LightDir.Z > 0.003 && FVector::DotProduct(M.N, LightDir) > 0.05)
	{
		constexpr int32 NU = 6, NV = 4;
		int32 Hit = 0;
		for (int32 i = 0; i < NU; ++i)
			for (int32 j = 0; j < NV; ++j)
			{
				const double U = (i + 0.5) / NU, V = (j + 0.5) / NV;
				const FVector P = M.Center + M.U * ((U - 0.5) * M.WidthCm) + M.V * ((V - 0.5) * M.HeightCm) + M.N * 8.0;
				if ((bMoon || !UDysisWorldState::InIsleShadow(P, LightDir)) && CastLight(P + LightDir * 2.0, LightDir, 40000.0) < 0.0)
				{
					++Hit;
					U0 = FMath::Min(U0, U - 0.5 / NU); U1 = FMath::Max(U1, U + 0.5 / NU);
					V0 = FMath::Min(V0, V - 0.5 / NV); V1 = FMath::Max(V1, V + 0.5 / NV);
				}
			}
		LitFrac = double(Hit) / double(NU * NV);
		if (Hit > 0)
		{
			M.bHasLitBox = true;
			M.LitBox[0] = float(U0); M.LitBox[1] = float(U1); M.LitBox[2] = float(V0); M.LitBox[3] = float(V1);
		}
	}
	Lit = LitFrac;
	if (LitFrac < 0.34) { Done(); return; }

	// 按反射定律射出去
	const FVector D = -LightDir;
	const FVector R = (D - M.N * (2.0 * FVector::DotProduct(D, M.N))).GetSafeNormal();
	M.ReflectDir = R;
	const FVector Q0 = M.Center + M.U * ((U0 - 0.5) * M.WidthCm) + M.V * ((V0 - 0.5) * M.HeightCm);
	const FVector Q1 = Q0 + M.U * ((U1 - U0) * M.WidthCm);
	const FVector Q2 = Q0 + M.V * ((V1 - V0) * M.HeightCm);
	const FVector Q3 = Q1 + M.V * ((V1 - V0) * M.HeightCm);
	// 能踩的一面是光束的下表面：看沿光走 3 m 以后哪条边更低
	const bool bLowFirst = (Q0 + R * 300.0).Z <= (Q2 + R * 300.0).Z;
	const FVector Corners[4] = { bLowFirst ? Q0 : Q2, bLowFirst ? Q1 : Q3, bLowFirst ? Q2 : Q0, bLowFirst ? Q3 : Q1 };
	SetGreyboxFrame(Corners, R, 8.0, -30.0, false, 0.0);
	double MinEdge = FrameEdge[0];
	for (int32 i = 1; i < 7; ++i) MinEdge = FMath::Min(MinEdge, FrameEdge[i]);
	// 日相的镜光要够得到日之龛的石台（镜子下沿到石台 26.3 m，留 2.5 m 余量）
	bClean = !bMoon && Lit > 0.5 && FrameU.Size() > 120.0 && MinEdge > GB_MIRROR_CROSS_DIST - 250.0;
	Done();
}

bool ADysisBeamActor::ComputeGreyboxWalkable() const
{
	if (GreyboxMirror == 2) return false;   // 月光反射不能踩
	if (bGreyboxOculus || GreyboxMirror == 1)
	{
		if (!bFrameValid || !bClean) return false;
	}
	else
	{
		const FGreyboxWindow& W0 = GWindows[GreyboxIndex];
		if (!bFrameValid || !bClean || !W0.bCanWalk) return false;
	}
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	if (!State) return false;
	if (GreyboxIndex == 0 && State->IsleGrowK < 1.0f) return false;   // 开场的光还在往岛上伸
	const bool bGrace = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(GraceTimer);
	const bool bOn = bSomeoneStanding || bGrace;
	if (FMath::Abs(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(double(FrameL.Z), -1.0, 1.0)))) > (bOn ? 38.0 : 35.0)) return false;
	// 雾：光路上有一段在中庭（或海峡）的雾里
	const FVector Mid = (FrameB0 + FrameB1) * 0.5;
	int32 InMist = 0, N = 0;
	for (double S = FMath::Max(FrameS0, 0.0); S <= FrameS1; S += 50.0)
	{
		const FVector P = Mid + FrameL * S + FVector(0.0, 0.0, 30.0);
		++N;
		if (State->MistAt(P) >= 0.85f) ++InMist;
	}
	return N > 0 && double(InMist) / double(N) > (bOn ? 0.15 : 0.25);
}

void ADysisBeamActor::ApplyGreyboxComponents()
{
	TravelDir = FrameL;
	WindowCm = FrameO + (FrameU + FrameV) * 0.5;
	double Len = 0.0;
	for (int32 k = 0; k < 4; ++k) Len += FrameC[k];
	Len *= 0.25;
	LenCm = bFrameValid ? Len : 0.0;

	// 看得见的光：一根沿着光的长条（截面 = 被照亮的那一块窗）。亮度：照亮比例 × 增益，不能踩的暗一些。
	const bool bShow = bFrameValid && Len > 5.0;
	Visual->SetVisibility(bShow);
	if (bShow)
	{
		const FVector X = FrameL;
		FVector Y = FrameU - X * FVector::DotProduct(FrameU, X);
		const double Wd = Y.Size();
		Y = Wd > 1.0e-3 ? Y / Wd : FVector::RightVector;
		const FVector Z = FVector::CrossProduct(X, Y);
		const double Ht = FMath::Abs(FVector::DotProduct(FrameV, Z));
		Visual->SetWorldLocationAndRotation(FrameO + (FrameU + FrameV) * 0.5 + X * (Len * 0.5), FRotationMatrix::MakeFromXY(X, Y).ToQuat());
		Visual->SetWorldScale3D(FVector(Len, Wd, FMath::Max(Ht, 1.0)) / 100.0);
		if (VisualMID) VisualMID->SetScalarParameterValue(TEXT("Intensity"), float(Lit) * (bGreyboxOculus ? 1.1f : GreyboxMirror == 1 ? 0.85f : GreyboxMirror == 2 ? 0.6f : GWindows[GreyboxIndex].Gain) * (bWalkable ? 1.0f : 0.55f));
	}

	// 能踩的面：光的下表面，从 s0 到 s1，两侧各宽 0.35 m。用一块 4 cm 厚的板，顶面就是那个面。
	const bool bGrace = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(GraceTimer);
	// 镜光例外：只铺到日之龛石台的上方。灰盒里是一直铺到墙的，人走到头再往旁边迈一步落到石台上；
	// 但最后那一段光贴着四层楼板的底面过去（离楼板不到 1.8 m），UE 里人有头顶碰撞、站不直。
	// 所以这里铺到石台上方就停，人顺着光走到头自然落在石台上（落差 0.25–0.55 m）。光本身照多远不变。
	double WalkS1 = FrameS1;
	if (GreyboxMirror == 1 && bFrameValid)
		WalkS1 = FMath::Min(WalkS1, FVector::DotProduct(GB_MIRROR_CROSS_T - (FrameB0 + FrameB1) * 0.5, FrameL) + GB_MIRROR_WALK_PAST);
	const bool bSolid = bFrameValid && (bWalkable || bGrace) && WalkS1 > FrameS0 + 1.0;
	Collision->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bSolid)
	{
		const FVector E = FrameB1 - FrameB0;
		FVector N = FVector::CrossProduct(E, FrameL).GetSafeNormal();
		if (N.Z < 0.0) N = -N;
		const FVector Yw = FVector::CrossProduct(N, FrameL).GetSafeNormal();
		const double Width = FMath::Abs(FVector::DotProduct(E, Yw));
		constexpr double Thick = 4.0;
		const FVector Center = FrameB0 + E * 0.5 + FrameL * ((FrameS0 + WalkS1) * 0.5) - N * (Thick * 0.5);
		Collision->SetWorldLocationAndRotation(Center, FRotationMatrix::MakeFromXY(FrameL, Yw).ToQuat());
		Collision->SetWorldScale3D(FVector(WalkS1 - FrameS0, Width, Thick) / 100.0);
	}
}

void ADysisBeamActor::WorldToStrip(const FVector& P, double& OutA, double& OutS) const
{
	const FVector E = FrameB1 - FrameB0, D = P - FrameB0, L = FrameL;
	const double EE = FVector::DotProduct(E, E), EL = FVector::DotProduct(E, L), LL = FVector::DotProduct(L, L);
	const double ED = FVector::DotProduct(E, D), LD = FVector::DotProduct(L, D), Det = EE * LL - EL * EL;
	if (FMath::Abs(Det) < 1.0e-9) { OutA = 0.5; OutS = 0.0; return; }
	OutA = (ED * LL - LD * EL) / Det;
	OutS = (LD * EE - ED * EL) / Det;
}

FVector ADysisBeamActor::StripToWorld(double A, double S) const
{
	return FrameB0 + (FrameB1 - FrameB0) * A + FrameL * S;
}

bool ADysisBeamActor::ContainsPoint(const FVector& P, double TolCm) const
{
	// 灰盒 insidePrism：把点换成光柱自己的坐标（沿窗宽 0–1、沿窗高 0–1、沿光多少厘米），再看有没有超出照到的地方
	if (!HasLight()) return false;
	const FVector D = P - FrameO, VxL = FVector::CrossProduct(FrameV, FrameL);
	const double Det = FVector::DotProduct(FrameU, VxL);
	if (FMath::Abs(Det) < 1.0e-9) return false;
	const double X = FVector::DotProduct(D, VxL) / Det;
	const double Y = FVector::DotProduct(FrameU, FVector::CrossProduct(D, FrameL)) / Det;
	const double S = FVector::DotProduct(FrameU, FVector::CrossProduct(FrameV, D)) / Det;
	if (X < -0.02 || X > 1.02 || Y < -0.02 || Y > 1.02 || S < 0.0) return false;
	const double Reach = FMath::Lerp(FMath::Lerp(FrameC[0], FrameC[1], X), FMath::Lerp(FrameC[2], FrameC[3], X), Y);
	return S <= Reach + TolCm;
}

bool ADysisBeamActor::ClampToRail(FVector& Foot) const
{
	if (!IsGreybox() || !bFrameValid) return false;
	// 横向：和光的水平方向垂直的那个水平方向
	FVector NH(-FrameL.Y, FrameL.X, 0.0);
	if (NH.SizeSquared() < 1.0e-8) return false;
	NH.Normalize();
	const double L1 = FVector::DotProduct(FrameB1 - FrameB0, NH);
	const double Lo = FMath::Min(0.0, L1) + 6.0, Hi = FMath::Max(0.0, L1) - 6.0;
	const double Lat = FVector::DotProduct(Foot - FrameB0, NH);
	if (Lat >= Lo && Lat <= Hi) return false;
	// 三相像的镜光很长：除了起头 1.5 m 和落到石台前的 2.5 m，两边一直拦着，走歪了也不会掉下去
	const double SAlong = FVector::DotProduct(Foot - (FrameB0 + FrameB1) * 0.5, FrameL);
	const bool bHold = GreyboxMirror == 1 && SAlong > 150.0 && SAlong < FrameS1 - 250.0;
	// 边上往下 6.2 m 以内有别的落脚处，就不拦（可以从光上走到旁边的地面上）
	if (const UWorld* World = bHold ? nullptr : GetWorld())
	{
		FCollisionQueryParams Params(TEXT("DysisBeamRail"), true);
		Params.AddIgnoredActor(this);
		if (const APlayerController* PC = World->GetFirstPlayerController())
			if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Foot + FVector(0.0, 0.0, 55.0), Foot - FVector(0.0, 0.0, 620.0), ECC_Pawn, Params)) return false;
	}
	Foot -= NH * (Lat - FMath::Clamp(Lat, Lo, Hi));
	return true;
}

FString ADysisBeamActor::DebugSolve(float H)
{
	if (!IsGreybox()) return TEXT("{}");
	UpdateGreybox(H);
	auto V3 = [](const FVector& V) { return FString::Printf(TEXT("[%.3f,%.3f,%.3f]"), V.X, V.Y, V.Z); };
	auto V6 = [](const FVector& V) { return FString::Printf(TEXT("[%.6f,%.6f,%.6f]"), V.X, V.Y, V.Z); };
	FString S = FString::Printf(TEXT("{\"id\":\"%s\",\"valid\":%s,\"lit\":%.4f,\"clean\":%s,\"walkable\":%s"),
		*GreyboxId.ToString(), bFrameValid ? TEXT("true") : TEXT("false"), Lit, bClean ? TEXT("true") : TEXT("false"), bWalkable ? TEXT("true") : TEXT("false"));
	if (bFrameValid)
	{
		S += FString::Printf(TEXT(",\"O\":%s,\"U\":%s,\"V\":%s,\"L\":%s"), *V3(FrameO), *V3(FrameU), *V3(FrameV), *V6(FrameL));
		S += FString::Printf(TEXT(",\"c\":[%.3f,%.3f,%.3f,%.3f]"), FrameC[0], FrameC[1], FrameC[2], FrameC[3]);
		S += TEXT(",\"edge\":[");
		for (int32 i = 0; i < 7; ++i) S += FString::Printf(TEXT("%s%.3f"), i ? TEXT(",") : TEXT(""), FrameEdge[i]);
		S += FString::Printf(TEXT("],\"b0\":%s,\"b1\":%s,\"s0\":%.3f,\"s1\":%.3f,\"far\":%s,\"need\":%.3f"), *V3(FrameB0), *V3(FrameB1), FrameS0, FrameS1, *V3(FrameFar), Need);
	}
	return S + TEXT("}");
}
