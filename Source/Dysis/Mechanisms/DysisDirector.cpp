#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "World/DysisWorldState.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度） ─────
	// 水闸：水庭东边的石台上（SLUICE az 113.8°、r 11.64 m，石台面 0.9 m）
	const FVector DirSluicePos(-469.73, 1065.01, 90.0);
	// L1 的机关：原来 A 在西南（az 226°）、B 在伊莉丝浮雕旁（az 71°），现在合成一个，
	// 立在两处沿回廊走的正中间（接缝那一段不通，所以是经西、北这一侧：az 328.5°），贴着外墙（r 15.05 m）。
	constexpr float DirLeverAz = 328.5f, DirLeverR = 1505.0f, DirLeverZ = 600.0f;
	// 两块推拉石板（sliderSpecs）：贴着外墙面滑，r = R_OUT + 0.1 m
	struct FDirPanelSpec { const TCHAR* Piece; float Az, Shift, Z; bool bOpenAt; };
	const FDirPanelSpec DirPanelSpecs[] = {
		{ TEXT("SM_Mech_Slider_Panel_b2"),   245.72f, 8.55f, 1985.0f, true  },   // 西边“上升的窗”：开局关着
		{ TEXT("SM_Mech_Slider_Panel_iris"), 171.43f, 8.35f, 1905.0f, false },   // 南边“虹的窗”：开局开着
	};
	constexpr float DirSliderSeconds = 1.6f;
}

ADysisDirector::ADysisDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) PlaneMesh = Plane.Object;

	SluiceMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SluiceMarker"));
	SluiceMarker->SetupAttachment(RootComponent);
	SluiceMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded()) SluiceMarker->SetStaticMesh(Cylinder.Object);
	SluiceMarker->SetRelativeLocation(DirSluicePos + FVector(0.0, 0.0, 45.0));
	SluiceMarker->SetRelativeScale3D(FVector(0.25, 0.25, 0.9));

	Apple = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Apple"));
	Apple->SetupAttachment(RootComponent);
	Apple->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sphere.Succeeded()) Apple->SetStaticMesh(Sphere.Object);
	Apple->SetRelativeScale3D(FVector(0.24));   // 灰盒：半径 0.12 m
}

ADysisDirector* ADysisDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	TActorIterator<ADysisDirector> It(World);
	return It ? *It : nullptr;
}

UDysisTimeComponent* ADysisDirector::PlayerTime() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UDysisTimeComponent>() : nullptr;
}

bool ADysisDirector::GameStarted() const
{
	const ADysisHUD* Hud = ADysisHUD::Get(this);
	return !Hud || !Hud->IsMenuOpen();
}

void ADysisDirector::BeginPlay()
{
	Super::BeginPlay();
	CollectPieces();

	// 推拉石板：记下开局的位置和朝向（模型摆的就是开局状态）
	for (const FDirPanelSpec& Spec : DirPanelSpecs)
	{
		FSliderPanel P;
		P.Actor = Piece(Spec.Piece);
		P.Az = Spec.Az; P.Shift = Spec.Shift; P.Z = Spec.Z; P.bOpenAt = Spec.bOpenAt;
		if (AActor* A = P.Actor.Get())
		{
			P.Yaw0 = A->GetActorRotation().Yaw;
			P.Az0 = DysisGB::AzOf(A->GetActorLocation());
			if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>()) C->SetMobility(EComponentMobility::Movable);
		}
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 机关：关卡里找不到部件 %s"), Spec.Piece);
		Panels.Add(P);
	}
	// 机关的杆：绕着“贴墙的那条水平轴”前后扳
	LeverArm = Piece(TEXT("SM_Mech_LeverA_Arm"));
	if (AActor* Arm = LeverArm.Get())
	{
		LeverArmBase = Arm->GetActorQuat();
		const FVector Out = Arm->GetActorLocation().GetSafeNormal2D();
		LeverAxis = FVector(-Out.Y, Out.X, 0.0);
		if (UStaticMeshComponent* C = Arm->FindComponentByClass<UStaticMeshComponent>()) C->SetMobility(EComponentMobility::Movable);
	}
	PlaceSlider();
	UpdateWaterfall();
	SetupRoof();
	SetupMirror();
	SetupRainbow();
	SetupNight();
	SetupTwins();
	SetupFinale();
	BuildInteracts();
}

void ADysisDirector::CollectPieces()
{
	Pieces.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tag : It->Tags)
			if (Tag.ToString().StartsWith(TEXT("SM_"))) { Pieces.Add(Tag, *It); break; }
}

AActor* ADysisDirector::Piece(FName Label) const
{
	const TWeakObjectPtr<AActor>* Found = Pieces.Find(Label);
	return Found ? Found->Get() : nullptr;
}

// ───────────────────────── 互动（灰盒 INTERACTS / nearestInteract） ─────────────────────────

void ADysisDirector::BuildInteracts()
{
	Interacts.Reset();
	{
		// 水闸：只能开，不能关；开了以后就不能再互动了（2026-10-08 定的）
		FDysisInteract I;
		I.Id = TEXT("sluice");
		I.Pos = []() { return DirSluicePos; };
		I.ZRange = []() { return FVector2D(60.0, 270.0); };
		I.RadiusCm = 190.0f;
		I.When = [this]() { const UDysisWorldState* S = UDysisWorldState::Get(this); return S && !S->bSluiceOpen; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptWaterGate); };
		I.Act = [this]() { ToggleSluice(); };
		Interacts.Add(MoveTemp(I));
	}
	{
		FDysisInteract I;
		I.Id = TEXT("lever");
		I.Pos = []() { return DysisGB::PolarCm(DirLeverAz, DirLeverR, DirLeverZ); };
		I.ZRange = []() { return FVector2D(DirLeverZ - 30.0, DirLeverZ + 180.0); };
		I.RadiusCm = 160.0f;
		I.Label = []() { return FText::FromString(TEXT("拉动机关")); };
		I.Act = [this]() { PullLever(); };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 取下金苹果（接住最后一缕光）：站在屋顶最高一级的浑天仪旁边
		FDysisInteract I;
		I.Id = TEXT("takeApple");
		I.Pos = [this]() { return ArmTopCm(); };
		I.ZRange = [this]() { const double Z = ArmTopCm().Z; return FVector2D(Z - 220.0, Z + 20.0); };
		I.RadiusCm = 180.0f;
		I.When = [this]() { return !bCaught; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptTakeApple); };
		I.Act = [this]() { CatchLight(); };
		Interacts.Add(MoveTemp(I));
	}
	AddMirrorInteracts();
	AddRainbowInteracts();
	AddNightInteracts();
	AddTwinsInteracts();
	AddFinaleInteracts();
}

const FDysisInteract* ADysisDirector::NearestInteract(const FVector& Foot) const
{
	const FDysisInteract* Best = nullptr;
	float BestDist = 1.0e9f;
	for (const FDysisInteract& I : Interacts)
	{
		const FVector2D ZR = I.ZRange();
		if (Foot.Z < ZR.X || Foot.Z > ZR.Y) continue;
		if (I.When && !I.When()) continue;
		const float D = FVector::Dist2D(Foot, I.Pos());
		if (D < I.RadiusCm && D < BestDist) { Best = &I; BestDist = D; }
	}
	return Best;
}

bool ADysisDirector::Interact(const FVector& Foot)
{
	const FDysisInteract* I = NearestInteract(Foot);
	if (!I || !I->Act) return false;
	I->Act();
	return true;
}

bool ADysisDirector::DebugInteract(FName Id)
{
	for (const FDysisInteract& I : Interacts)
		if (I.Id == Id && I.Act && (!I.When || I.When())) { I.Act(); return true; }
	return false;
}

// ───────────────────────── 水闸、瀑布 ─────────────────────────

void ADysisDirector::ToggleSluice()
{
	UDysisWorldState* S = UDysisWorldState::Get(this);
	if (!S || S->bSluiceOpen) return;
	S->SetSluiceOpen(true);
	ADysisHUD::Notify(GetWorld(), DysisCopy::WaterGateOpened, 5.6f);
}

void ADysisDirector::UpdateWaterfall()
{
	// 瀑布：有水量才看得见（灰盒 fallMesh.visible = FLOW.k > 0.002）
	const UDysisWorldState* S = UDysisWorldState::Get(this);
	if (AActor* Fall = Piece(TEXT("SM_Mech_Water_Waterfall")))
	{
		const bool bShow = S && S->FlowK > 0.002f;
		if (Fall->IsHidden() == bShow) Fall->SetActorHiddenInGame(!bShow);
	}
}

// ───────────────────────── L1 的机关和两块推拉石板 ─────────────────────────

void ADysisDirector::PullLever()
{
	// 原来 A 只管打开西窗、B 只管复原；合成一个以后每拉一次换一边（文案：单数次 / 双数次）
	++LeverPulls;
	SliderTarget = SliderTarget > 0.5f ? 0.0f : 1.0f;
	ADysisHUD::Notify(GetWorld(), (LeverPulls % 2 == 1) ? DysisCopy::MechOddTrigger : DysisCopy::MechEvenTrigger, 4.6f);
}

void ADysisDirector::UpdateSlider(float Dt)
{
	const float Before = SliderPos;
	SliderPos = DysisGB::Toward(SliderPos, SliderTarget, 1.0f / DirSliderSeconds, Dt);
	if (SliderPos != Before)
	{
		PlaceSlider();
		if (UDysisWorldState* S = UDysisWorldState::Get(this)) S->BumpLight();   // 石板挪了，光要重算
	}
}

void ADysisDirector::PlaceSlider()
{
	const float K = DysisGB::Smoothstep(0.0f, 1.0f, SliderPos);
	for (const FSliderPanel& P : Panels)
	{
		AActor* A = P.Actor.Get();
		if (!A) continue;
		const float Open = P.bOpenAt ? K : 1.0f - K;
		const float Az = P.Az + P.Shift * Open;
		A->SetActorLocationAndRotation(DysisGB::PolarCm(Az, DysisGB::R_OUT + 10.0f, P.Z), FRotator(0.0f, P.Yaw0 + (Az - P.Az0), 0.0f));
	}
	// 杆：从一边扳到另一边（灰盒 ±0.5 弧度）
	if (AActor* Arm = LeverArm.Get())
		Arm->SetActorRotation(FQuat(LeverAxis, SliderPos * 1.0f) * LeverArmBase);
}

void ADysisDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSlider(DeltaTime);
	UpdateWaterfall();
	UpdateCrown(DeltaTime);
	UpdateMirrors(DeltaTime);
	UpdateNiches(DeltaTime);
	UpdateIrisRelief(DeltaTime);
	UpdateSwan(DeltaTime);
	UpdateMoonstones(DeltaTime);
	UpdateTwins(DeltaTime);
	UpdateGoddess(DeltaTime);
	UpdateShadowBridge(DeltaTime);
	UpdateFinale(DeltaTime);
	UpdateLevelTitle();
	UpdateStairs(DeltaTime);
	UpdateCatch(DeltaTime);
}

// ───────────────────────── 关卡标题 ─────────────────────────

void ADysisDirector::UpdateLevelTitle()
{
	// 灰盒 levelOf：按脚下的区域分段——白天 序、日1–日5，入夜以后 月1–月5；每一段头一次走进去出一次标题
	static const TCHAR* const Names[11][2] = {
		{ TEXT("序"), TEXT("登殿") }, { TEXT("日1"), TEXT("午后·开闸") }, { TEXT("日2"), TEXT("未时·双窗") }, { TEXT("日3"), TEXT("申时·光阶") },
		{ TEXT("日4"), TEXT("酉时·圆眼") }, { TEXT("日5"), TEXT("日落·最后一缕") },
		{ TEXT("月1"), TEXT("月升·回廊") }, { TEXT("月2"), TEXT("初夜·天鹅") }, { TEXT("月3"), TEXT("中夜·三相") }, { TEXT("月4"), TEXT("夜半·双子") }, { TEXT("月5"), TEXT("子夜·瀑布后的女神") } };
	if (!GameStarted()) return;
	const UDysisTimeComponent* Time = PlayerTime();
	if (!Time) return;
	const FString& Z = Time->Zone;
	int32 Lv = -1;
	if (!bCaught)
	{
		if (Z == TEXT("out") || Z == TEXT("beam:isle")) Lv = 0;
		else if (Z == TEXT("L0") || Z == TEXT("beam:b1") || Z == TEXT("wfback")) Lv = 1;
		else if (Z == TEXT("L1") || Z == TEXT("beam:b2") || Z == TEXT("rainbow") || Z == TEXT("sill")) Lv = 2;
		else if (Z == TEXT("L2") || Z.StartsWith(TEXT("beam:h")) || Z == TEXT("beam:mirror") || Z == TEXT("ledge")) Lv = 3;
		else if (Z == TEXT("L3")) Lv = 4;
		else if (Z == TEXT("beam:oculus") || Z == TEXT("rbridge") || Z == TEXT("crown")) Lv = 5;
	}
	else
	{
		if (Z == TEXT("crown") || Z == TEXT("rbridge")) Lv = 6;
		else if (Z == TEXT("L3") || Z == TEXT("tun:TS")) Lv = 7;
		else if (Z == TEXT("L2") || Z == TEXT("tun:TR")) Lv = 8;
		else if (Z == TEXT("L1") || Z == TEXT("moonbr")) Lv = 9;
		else if (Z == TEXT("L0") || Z == TEXT("pav") || Z == TEXT("gbridge") || Z == TEXT("shadowbr") || Z == TEXT("wfback")) Lv = 10;
	}
	if (Lv <= SeenLevel) return;
	SeenLevel = Lv;
	if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->ShowTitle(Names[Lv][0], Names[Lv][1], 2.4f);
}
