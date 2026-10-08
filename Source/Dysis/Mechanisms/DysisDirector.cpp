#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "World/DysisWorldState.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度） ─────
	// 水闸：水庭东边的石台上（SLUICE az 113.8°、r 11.64 m，石台面 0.9 m）
	const FVector SluicePos(-469.73, 1065.01, 90.0);
	// L1 的机关：原来 A 在西南（az 226°）、B 在伊莉丝浮雕旁（az 71°），现在合成一个，
	// 立在两处沿回廊走的正中间（接缝那一段不通，所以是经西、北这一侧：az 328.5°），贴着外墙（r 15.05 m）。
	constexpr float LeverAz = 328.5f, LeverR = 1505.0f, LeverZ = 600.0f;
	// 两块推拉石板（sliderSpecs）：贴着外墙面滑，r = R_OUT + 0.1 m
	struct FPanelSpec { const TCHAR* Piece; float Az, Shift, Z; bool bOpenAt; };
	const FPanelSpec PanelSpecs[] = {
		{ TEXT("SM_Mech_Slider_Panel_b2"),   245.72f, 8.55f, 1985.0f, true  },   // 西边“上升的窗”：开局关着
		{ TEXT("SM_Mech_Slider_Panel_iris"), 171.43f, 8.35f, 1905.0f, false },   // 南边“虹的窗”：开局开着
	};
	constexpr float SliderSeconds = 1.6f;

	FVector PolarCm(double AzDeg, double R, double Z)
	{
		const double A = FMath::DegreesToRadians(AzDeg);
		return FVector(R * FMath::Cos(A), R * FMath::Sin(A), Z);
	}
}

ADysisDirector::ADysisDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SluiceMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SluiceMarker"));
	SluiceMarker->SetupAttachment(RootComponent);
	SluiceMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded()) SluiceMarker->SetStaticMesh(Cylinder.Object);
	SluiceMarker->SetRelativeLocation(SluicePos + FVector(0.0, 0.0, 45.0));
	SluiceMarker->SetRelativeScale3D(FVector(0.25, 0.25, 0.9));
}

ADysisDirector* ADysisDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	TActorIterator<ADysisDirector> It(World);
	return It ? *It : nullptr;
}

void ADysisDirector::BeginPlay()
{
	Super::BeginPlay();
	CollectPieces();

	// 推拉石板：记下开局的位置和朝向（模型摆的就是开局状态）
	for (const FPanelSpec& Spec : PanelSpecs)
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
		// 水闸：文案表没有“关上”这一项——开了以后再去碰，只是看一眼
		FDysisInteract I;
		I.Id = TEXT("sluice");
		I.PosCm = SluicePos; I.Z0 = 60.0f; I.Z1 = 270.0f; I.RadiusCm = 190.0f;
		I.Label = [this]()
		{
			const UDysisWorldState* S = UDysisWorldState::Get(this);
			return FText::FromString(S && S->bSluiceOpen ? TEXT("看看水闸") : DysisCopy::PromptWaterGate);
		};
		I.Act = [this]() { ToggleSluice(); };
		Interacts.Add(MoveTemp(I));
	}
	{
		FDysisInteract I;
		I.Id = TEXT("lever");
		I.PosCm = PolarCm(LeverAz, LeverR, LeverZ); I.Z0 = LeverZ - 30.0f; I.Z1 = LeverZ + 180.0f; I.RadiusCm = 160.0f;
		I.Label = []() { return FText::FromString(TEXT("拉动机关")); };
		I.Act = [this]() { PullLever(); };
		Interacts.Add(MoveTemp(I));
	}
}

const FDysisInteract* ADysisDirector::NearestInteract(const FVector& Foot) const
{
	const FDysisInteract* Best = nullptr;
	float BestDist = 1.0e9f;
	for (const FDysisInteract& I : Interacts)
	{
		if (Foot.Z < I.Z0 || Foot.Z > I.Z1) continue;
		if (I.When && !I.When()) continue;
		const float D = FVector::Dist2D(Foot, I.PosCm);
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
	if (!S) return;
	if (!S->bSluiceOpen)
	{
		S->SetSluiceOpen(true);
		ADysisHUD::Notify(GetWorld(), DysisCopy::WaterGateOpened, 5.6f);
	}
	else ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackWaterGateReinteract, 5.6f);
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
	SliderPos = DysisGB::Toward(SliderPos, SliderTarget, 1.0f / SliderSeconds, Dt);
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
		A->SetActorLocationAndRotation(PolarCm(Az, DysisGB::R_OUT + 10.0f, P.Z), FRotator(0.0f, P.Yaw0 + (Az - P.Az0), 0.0f));
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
}
