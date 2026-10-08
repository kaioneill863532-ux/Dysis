// 机关总管 · 虹那条支线：灰盒 v0.12 的 IRISREL / RB / BRIDGE / SILL / PRISM / SELENE（updateIrisRelief / updateRainbow）。
//   · 伊莉丝浮雕（L1 东墙）：虹的窗的光落到这里的时候，背对太阳站好，让自己影子的头落进浮雕中间那个空着的人形，
//     停住一秒多，虹就醒过来。
//   · 虹桥：从浮雕旁边拱过中庭，落到三层南墙大窗下面。快到的时候窗下伸出一道石沿，窗洞西边的墙上露出虹之龛。
//     虹桥是阳光的，入夜以后整座消失。
//   · 虹之龛：站在石沿上打开，得到彩虹碎片；窗台上升起一根铜柱，托着棱镜。
//   · 棱镜：一格一格地转（8 格），南窗的光被分成七色；靛色落进水庭北墙上塞勒涅的眼睛，她就醒了（对话）。
// 标准答案：Tools/greybox/golden/greybox_rainbow.json；测试：Tools/tests/pie_rainbow.py。
// 和灰盒不一样的两处（都只是动画的方向，灰盒里看着是反的）：石沿和虹之龛是从墙里往外伸的；虹之龛的两扇门朝外开。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisDialogueComponent.h"
#include "UI/DysisHUD.h"
#include "World/DysisInvisibleWall.h"
#include "World/DysisWorldState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度；就是 greybox_rainbow.json 里 consts 那一段） ─────
	// 伊莉丝浮雕：L1 东墙，方位 79°
	const FVector RbwFace(293.464, 1509.743, 600.0);            // 浮雕面的墙根
	const FVector RbwFaceN(-0.190809, -0.981627, 0.0);          // 浮雕朝着中庭的方向
	const FVector RbwFaceT(-0.981627, 0.190809, 0.0);           // 沿着墙的方向
	const FVector RbwHead(293.464, 1509.743, 726.765);          // 空着的人形：头的中心
	const FVector RbwReliefView(289.08, 1487.17, 600.0);        // “看看浮雕”的位置
	constexpr double RbwPlateW = 220.0, RbwPlateH = 270.0;
	constexpr double RbwHeadTolCm = 34.0;                       // 影子的头离人形的头多近算对上
	// 虹桥：平面上是一条直线，高度是一道拱
	const FVector RbwA(138.018, 1433.371, 600.0);
	const FVector RbwAx(-0.834334, -0.551259, 0.0), RbwSide(0.551259, -0.834334, 0.0);
	constexpr double RbwLen = 1933.1191173, RbwWidth = 160.0, RbwBump = 120.0, RbwZ0 = 600.0, RbwZ1 = 1750.0;
	constexpr int32 RbwSegs = 90;
	// 南窗下的石沿、虹之龛
	constexpr double RbwSillAz = 166.0, RbwSillR0 = 1490.0, RbwSillZ = 1750.0;
	constexpr double RbwSillSlide = 90.0;                       // 缩在墙里的时候往里退多少
	const FVector RbwNichePos(-1454.846, 498.867, 1870.0);
	constexpr double RbwNicheAz = 161.07314828633716;
	// 棱镜：架在窗下石沿上的一根铜柱顶上
	const FVector RbwPrismPos(-1466.343, 399.056, 1807.519);
	constexpr double RbwPrismRise = 77.51928988;
	constexpr double RbwSlots[8] = { 78.04, 81.04, 81.71, 82.41, 83.42, 84.66, 85.99, 87.63 };   // 转台 8 格（棱镜对称轴的方位角）
	constexpr double RbwIor[7] = { 1.600, 1.605, 1.610, 1.617, 1.625, 1.633, 1.642 };           // 红…紫的折射率
	constexpr double RbwApexDeg = 45.0;
	// 塞勒涅：水庭北墙上的浮雕
	const FVector RbwSeleneEye(1545.268, -133.845, 268.04);     // 靛色的光要落的那一点（墙面上）
	const FVector RbwSeleneIp(1454.554, -125.987, 0.0);

	const FLinearColor& RbwBow(int32 K)
	{
		static const FLinearColor Colors[7] = {
			FLinearColor(FColor(0xff, 0x3a, 0x2a)), FLinearColor(FColor(0xff, 0x8a, 0x1c)), FLinearColor(FColor(0xff, 0xe0, 0x3a)), FLinearColor(FColor(0x46, 0xd8, 0x6a)),
			FLinearColor(FColor(0x3a, 0xa8, 0xff)), FLinearColor(FColor(0x4a, 0x4c, 0xff)), FLinearColor(FColor(0xa2, 0x4a, 0xff)) };
		return Colors[FMath::Clamp(K, 0, 6)];
	}

	/** 虹桥上的一点：T 沿桥 0–1，L 横向（厘米，中线是 0）。 */
	FVector RbwBridgeAt(double T, double L)
	{
		const FVector P = RbwA + RbwAx * (RbwLen * T) + RbwSide * L;
		return FVector(P.X, P.Y, FMath::Lerp(RbwZ0, RbwZ1, T) + RbwBump * FMath::Sin(UE_DOUBLE_PI * T));
	}

	bool RbwRefract(const FVector& D, const FVector& N, double Eta, FVector& Out)
	{
		const double C = FVector::DotProduct(D, N), K = 1.0 - Eta * Eta * (1.0 - C * C);
		if (K < 0.0) return false;
		Out = (D * Eta - N * (Eta * C + FMath::Sqrt(K))).GetSafeNormal();
		return true;
	}

	/** 灰盒 prismOut：光从 Din 方向进棱镜（对称轴朝方位 YawDeg），折射率 Ior 的那一色从另一面出去的方向。全反射出不去就是 false。 */
	bool RbwPrismOut(const FVector& Din, double YawDeg, double Ior, FVector& Out)
	{
		const double Y = FMath::DegreesToRadians(YawDeg), Hh = FMath::DegreesToRadians(RbwApexDeg * 0.5);
		const FVector U(FMath::Cos(Y), FMath::Sin(Y), 0.0), V(-FMath::Sin(Y), FMath::Cos(Y), 0.0);
		const FVector NL = -V * FMath::Cos(Hh) + U * FMath::Sin(Hh), NR = V * FMath::Cos(Hh) + U * FMath::Sin(Hh);
		const bool bLeftIn = FVector::DotProduct(Din, NL) < FVector::DotProduct(Din, NR);
		const FVector Ni = bLeftIn ? NL : NR, No = bLeftIn ? NR : NL;
		if (FVector::DotProduct(Din, Ni) >= 0.0) return false;
		FVector T1;
		if (!RbwRefract(Din, Ni, 1.0 / Ior, T1) || FVector::DotProduct(T1, No) <= 0.0) return false;
		return RbwRefract(T1, -No, Ior, Out);
	}

	UStaticMeshComponent* RbwMesh(AActor* A) { return A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr; }

}

// ───────────────────────── 开局 ─────────────────────────

UStaticMeshComponent* ADysisDirector::MakeGlow(UStaticMesh* Mesh, const FLinearColor& Color, float Intensity)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetVisibility(false);
	C->RegisterComponent();
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Beams/M_DysisBeam.M_DysisBeam")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
		MID->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
		C->SetMaterial(0, MID);
	}
	return C;
}

void ADysisDirector::SetupRainbow()
{
	UWorld* World = GetWorld();
	// 灰盒里浮雕、塞勒涅都只是贴在墙上的模型，没有碰撞：不挡光（光是照到后面的墙上的），也不挡人
	// （人要能贴近浮雕站，影子才对得上；挡着的是后面的墙）
	for (const TCHAR* Name : { TEXT("SM_Mech_IrisRelief_Relief"), TEXT("SM_Mech_IrisRelief_CarvedBow"), TEXT("SM_Mech_Selene_Relief") })
		if (AActor* A = Piece(Name)) A->SetActorEnableCollision(false);

	// 浮雕上的七色：一块贴在浮雕前面的透明片，材质按“从对的站位看过去 40°–43° 的那一圈”画出七条色带
	if (PlaneMesh)
	{
		BandsPlane = NewObject<UStaticMeshComponent>(this);
		BandsPlane->SetupAttachment(RootComponent);
		BandsPlane->SetMobility(EComponentMobility::Movable);
		BandsPlane->SetStaticMesh(PlaneMesh);
		BandsPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BandsPlane->SetCastShadow(false);
		BandsPlane->RegisterComponent();
		BandsPlane->SetWorldLocationAndRotation(RbwFace + RbwFaceN * 8.5 + FVector(0.0, 0.0, 5.0 + RbwPlateH * 0.5), FRotationMatrix::MakeFromZX(RbwFaceN, RbwFaceT).ToQuat());
		BandsPlane->SetWorldScale3D(FVector(RbwPlateW / 100.0, RbwPlateH / 100.0, 1.0));
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Rainbow/M_DysisBowBands.M_DysisBowBands")))
		{
			BandsMID = UMaterialInstanceDynamic::Create(Mat, this);
			BandsMID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
			BandsPlane->SetMaterial(0, BandsMID);
		}
		else BandsPlane->SetVisibility(false);
	}

	// 虹桥：模型就是那七条色带。先藏着、不能踩；能踩以后只挡人（不挡光、不挡镜头）
	BridgeActor = Piece(TEXT("SM_Mech_RainbowBridge_Light"));
	if (AActor* Br = BridgeActor.Get())
	{
		if (UStaticMeshComponent* C = RbwMesh(Br))
		{
			C->bUseDefaultCollision = false;
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 模型只管看；能踩的面是下面那 90 段薄板
			C->SetCastShadow(false);
			if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Rainbow/M_DysisRainbowBridge.M_DysisRainbowBridge")))
			{
				BridgeMID = UMaterialInstanceDynamic::Create(Mat, this);
				BridgeMID->SetScalarParameterValue(TEXT("Reveal"), 0.0f);
				C->SetMaterial(0, BridgeMID);
			}
		}
		Br->SetActorHiddenInGame(true);
		// 能踩的面（灰盒 BRIDGE.walk）：沿桥 90 段，每段一块 4 cm 厚的薄板，顶面就是桥面。只挡人。
		// 板挂在虹桥这个 Actor 上：它带着 “DysisZone=rainbow” 的 Tag，人踩上去时间系统就认这里是虹桥。
		BridgeFloor.Reset();
		for (int32 i = 0; i < RbwSegs; ++i)
		{
			const FVector P0 = RbwBridgeAt(double(i) / RbwSegs, 0.0), P1 = RbwBridgeAt(double(i + 1) / RbwSegs, 0.0);
			const FQuat Rot = FRotationMatrix::MakeFromXY((P1 - P0).GetSafeNormal(), RbwSide).ToQuat();
			UBoxComponent* Box = NewObject<UBoxComponent>(Br);
			Box->SetupAttachment(Br->GetRootComponent());
			Box->SetMobility(EComponentMobility::Movable);
			Box->SetCollisionObjectType(ECC_WorldDynamic);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->SetCanEverAffectNavigation(false);
			Box->SetHiddenInGame(true);
			Box->SetBoxExtent(FVector(FVector::Dist(P0, P1) * 0.5 + 0.5, RbwWidth * 0.5, 2.0));
			Box->ComponentTags.Add(TEXT("DysisOneWay"));
			Box->RegisterComponent();
			Br->AddInstanceComponent(Box);
			Box->SetWorldLocationAndRotation((P0 + P1) * 0.5 - Rot.GetUpVector() * 2.0, Rot);
			BridgeFloor.Add(Box);
		}
	}
	else UE_LOG(LogTemp, Warning, TEXT("Dysis 虹：关卡里找不到虹桥的模型"));
	// 桥两边看不见的护栏（灰盒 BRIDGE.rails）：每 3 段一块，高 2.2 m；落到窗台上的那一段不拦
	BridgeRails.Reset();
	const float AxYaw = FMath::RadiansToDegrees(FMath::Atan2(RbwAx.Y, RbwAx.X));
	for (int32 i = 3; i < RbwSegs - 3; i += 3)
		for (const double Sg : { -1.0, 1.0 })
		{
			const FVector A = RbwBridgeAt(double(i) / RbwSegs, Sg * (RbwWidth * 0.5 + 10.0)), B = RbwBridgeAt(double(i + 3) / RbwSegs, Sg * (RbwWidth * 0.5 + 10.0));
			if (i > RbwSegs / 2 && DysisGB::ROf(B) > RbwSillR0 - 40.0) continue;
			UBoxComponent* Box = MakeWall();
			Box->SetBoxExtent(FVector((FVector::Dist(A, B) + 10.0) * 0.5, 5.0, 110.0));
			Box->SetWorldLocationAndRotation((A + B) * 0.5 + FVector(0.0, 0.0, 110.0), FRotator(0.0f, AxYaw, 0.0f));
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->ComponentTags.Add(TEXT("DysisNoTrap"));
			BridgeRails.Add(Box);
		}

	// 窗下的石沿、虹之龛（背板、两扇门、碎片）：开局缩在墙里
	SillParts.Reset();
	const TCHAR* SillNames[] = { TEXT("SM_Mech_Sill_Ledge"), TEXT("SM_Mech_Sill_Niche"), TEXT("SM_Mech_Sill_NicheDoorL"), TEXT("SM_Mech_Sill_NicheDoorR"), TEXT("SM_Mech_Sill_NicheShard") };
	for (int32 i = 0; i < UE_ARRAY_COUNT(SillNames); ++i)
	{
		FSillPart P;
		P.Actor = Piece(SillNames[i]);
		P.bSolid = i == 0;
		if (AActor* A = P.Actor.Get())
		{
			P.Base = A->GetActorLocation();
			P.Yaw0 = A->GetActorRotation().Yaw;
			if (UStaticMeshComponent* C = RbwMesh(A)) { C->SetMobility(EComponentMobility::Movable); C->bUseDefaultCollision = false; }
			if (!P.bSolid) A->SetActorEnableCollision(false);   // 龛只是贴在墙上的模型
		}
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 虹：关卡里找不到部件 %s"), SillNames[i]);
		SillParts.Add(P);
	}
	// 窗洞里的台面一直在（只是太高上不去）：一块看不见的地面，站上去算“窗台”（时间停在浮雕醒过来的那一刻）
	if (World)
	{
		const FTransform Tf(FRotator(0.0f, float(RbwSillAz), 0.0f), DysisGB::PolarCm(float(RbwSillAz), 1611.5f, float(RbwSillZ) - 9.75f));
		if (ADysisInvisibleWall* Floor = World->SpawnActorDeferred<ADysisInvisibleWall>(ADysisInvisibleWall::StaticClass(), Tf))
		{
			Floor->ExtentCm = FVector(63.5, 85.0, 10.25);
			Floor->Group = TEXT("SillRecess");
			Floor->Tags.Add(TEXT("DysisZone=sill"));
			Floor->FinishSpawning(Tf);
		}
	}

	// 棱镜：铜柱、棱镜、铜缝、转盘，开局缩在窗台里
	PrismParts.Reset();
	const TCHAR* PrismNames[] = { TEXT("SM_Mech_Prism_Column"), TEXT("SM_Mech_Prism_Glass"), TEXT("SM_Mech_Prism_Slit"), TEXT("SM_Mech_Prism_Wheel") };
	for (const TCHAR* Name : PrismNames)
	{
		FPrismPart P;
		P.Actor = Piece(Name);
		if (AActor* A = P.Actor.Get())
		{
			P.Base = A->GetActorLocation();
			P.Yaw0 = A->GetActorRotation().Yaw;
			if (UStaticMeshComponent* C = RbwMesh(A)) C->SetMobility(EComponentMobility::Movable);
			A->SetActorEnableCollision(false);   // 灰盒里它们都不挡光；挡人的是升到头以后的一根看不见的柱子
		}
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 虹：关卡里找不到部件 %s"), Name);
		PrismParts.Add(P);
	}
	if (UBoxComponent* Box = MakeWall())
	{
		Box->SetBoxExtent(FVector(30.0, 30.0, 70.0));
		Box->SetWorldLocation(FVector(RbwPrismPos.X, RbwPrismPos.Y, RbwSillZ + 70.0));
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->ComponentTags.Add(TEXT("DysisNoTrap"));
		PrismBlock = Box;
	}
	// 七道色光和它们的落点
	PrismRays.Reset(); PrismSpots.Reset();
	UStaticMesh* Cyl = SluiceMarker ? SluiceMarker->GetStaticMesh() : nullptr;
	for (int32 k = 0; k < 7; ++k)
	{
		PrismRays.Add(MakeGlow(Cyl, RbwBow(k), 0.5f));
		PrismSpots.Add(MakeGlow(Cyl, RbwBow(k), 1.6f));
	}

	// 塞勒涅醒来的对话（文案表：伊莉丝 × 塞勒涅 × 狄西斯）
	SeleneDialogue = NewObject<UDysisDialogueComponent>(this);
	SeleneDialogue->RegisterComponent();
	SeleneDialogue->Lines.SetNum(DysisCopy::PrismDialogueCount);
	for (int32 i = 0; i < DysisCopy::PrismDialogueCount; ++i) SeleneDialogue->Lines[i] = FText::FromString(DysisCopy::PrismDialogue[i]);

	bReliefDone = bBridgeOn = bIrisNicheOpen = bSeleneOn = false;
	ReliefAlign = 0.0f; RainbowT = 0.0f; BridgeK = 0.0f; IrisNicheT = 0.0f; PrismExt = 0.0f;
	PrismSlot = 0; PrismYawDeg = float(RbwSlots[0]); PrismWheelDeg = 0.0f;
	SeleneColor = -1; SeleneLit = 0.0f; SeleneTalkIn = -1.0f;
	PlaceSill(0.0f);
	PlacePrism();
}

void ADysisDirector::AddRainbowInteracts()
{
	{
		// 伊莉丝浮雕：解开以前可以看
		FDysisInteract I;
		I.Id = TEXT("irisRelief");
		I.Pos = []() { return RbwReliefView; };
		I.ZRange = []() { return FVector2D(580.0, 800.0); };
		I.RadiusCm = 90.0f;
		I.When = [this]() { return !bReliefDone; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptViewRelief); };
		I.Act = [this]() { ADysisHUD::Notify(GetWorld(), DysisCopy::ViewIrisRelief, 5.6f); };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 虹之龛：站在窗下的石沿上开，只能开一次
		FDysisInteract I;
		I.Id = TEXT("irisNiche");
		I.Pos = []() { return RbwNichePos; };
		I.ZRange = []() { return FVector2D(RbwSillZ - 30.0, RbwSillZ + 190.0); };
		I.RadiusCm = 150.0f;
		I.When = [this]() { return !bIrisNicheOpen && SillK > 0.98f; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptOpenIrisNiche); };
		I.Act = [this]() { bIrisNicheOpen = true; IrisNicheT = 0.001f; };
		I.Anchor = []() { return RbwNichePos; };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 棱镜的转台：按一次转一格
		FDysisInteract I;
		I.Id = TEXT("prism");
		I.Pos = []() { return FVector(RbwPrismPos.X, RbwPrismPos.Y, RbwSillZ); };
		I.ZRange = []() { return FVector2D(RbwSillZ - 30.0, RbwSillZ + 190.0); };
		I.RadiusCm = 130.0f;
		I.When = [this]() { return PrismExt >= 1.0f; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptRotatePrism); };
		I.Act = [this]() { PrismSlot = (PrismSlot + 1) % int32(UE_ARRAY_COUNT(RbwSlots)); };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 塞勒涅的浮雕：醒了以后再看就是重听那段对话
		FDysisInteract I;
		I.Id = TEXT("selene");
		I.Pos = []() { return RbwSeleneIp; };
		I.Anchor = []() { return RbwSeleneEye - RbwSeleneEye.GetSafeNormal2D() * 25.0; };
		I.ZRange = []() { return FVector2D(-30.0, 190.0); };
		I.RadiusCm = 160.0f;
		I.Label = []() { return FText::FromString(DysisCopy::PromptViewRelief); };
		I.Act = [this]()
		{
			if (bSeleneOn) { if (SeleneDialogue && !SeleneDialogue->IsPlaying()) SeleneDialogue->Play(); }
			else ADysisHUD::Notify(GetWorld(), DysisCopy::ViewSeleneRelief, 4.8f);
		};
		Interacts.Add(MoveTemp(I));
	}
}

// ───────────────────────── 伊莉丝浮雕：影子贴合 ─────────────────────────

bool ADysisDirector::RainbowSeesLight(const FVector& Start, const FVector& Dir) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;
	FCollisionQueryParams Params(TEXT("DysisRainbowLit"), /*bTraceComplex*/ true);
	if (const APlayerController* PC = World->GetFirstPlayerController())
		if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);
	return !World->LineTraceTestByChannel(Start + Dir * 5.0, Start + Dir * 40000.0, ECC_Visibility, Params);
}

void ADysisDirector::UpdateIrisRelief(float Dt)
{
	const UWorld* World = GetWorld();
	const float Now = World ? float(World->GetTimeSeconds()) : 0.0f;
	if (bReliefDone)
	{
		if (BandsMID) BandsMID->SetScalarParameterValue(TEXT("Opacity"), 0.6f + 0.3f * FMath::Sin(Now * 2.0f));
		UpdateRainbow(Dt);
		return;
	}
	if (!IrisBeam.IsValid() && World)
		for (TActorIterator<ADysisBeamActor> It(World); It; ++It)
			if (It->GreyboxId == TEXT("iris")) { IrisBeam = *It; break; }
	const ADysisBeamActor* Iris = IrisBeam.Get();
	const UDysisTimeComponent* Time = PlayerTime();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ACharacter* Char = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	const bool bGrounded = Char && Char->GetCharacterMovement() && Char->GetCharacterMovement()->IsMovingOnGround();
	const bool bLight = Iris && Iris->HasLight() && !bCaught;

	// 人的头顺着阳光投到浮雕面上的那一点，离人形的头有多远
	bool bOk = false;
	ReliefMissCm = 900.0f;
	if (bLight && Time && bGrounded && Time->Zone == TEXT("L1"))
	{
		const FVector Sun = UDysisSkyLibrary::DysisSunDir(Time->H), L = -Sun;
		const FVector Head = Time->FootCm + FVector(0.0, 0.0, 162.0);
		const double Den = FVector::DotProduct(L, RbwFaceN);
		const double T = FMath::Abs(Den) > 1.0e-6 ? FVector::DotProduct(RbwFace - Head, RbwFaceN) / Den : -1.0;
		if (T > 0.0)
		{
			const FVector Q = Head + L * T;
			ReliefMissCm = float(FVector::Dist(Q, RbwHead));
			// 对上了，还要人的头和那一点都真的在阳光里
			if (ReliefMissCm < RbwHeadTolCm) bOk = !UDysisWorldState::InIsleShadow(Head, Sun) && RainbowSeesLight(Head, Sun) && RainbowSeesLight(Q + RbwFaceN * 6.0, Sun);
		}
	}
	ReliefAlign = DysisGB::Toward(ReliefAlign, bOk ? 1.0f : 0.0f, bOk ? 0.7f : 2.0f, Dt);
	// 影子的头离人形越近，刻着的那一圈上越显出虹的颜色（提示）；对准了就整圈亮起来
	const float Hint = bLight ? 0.4f * FMath::Clamp(1.0f - (ReliefMissCm - float(RbwHeadTolCm)) / 120.0f, 0.0f, 1.0f) : 0.0f;
	if (BandsMID) BandsMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Max(Hint, 0.95f * ReliefAlign));
	if (ReliefAlign >= 1.0f)
	{
		bReliefDone = true;
		RainbowT = 0.001f;
		ADysisHUD::Notify(GetWorld(), DysisCopy::IrisPuzzleSolved, 4.6f);
	}
}

void ADysisDirector::DebugResetRelief()
{
	if (!bReliefDone) ReliefAlign = 0.0f;
}

// ───────────────────────── 虹桥、窗台、虹之龛、棱镜、塞勒涅 ─────────────────────────

void ADysisDirector::SetBridgeOn(bool bOn)
{
	bBridgeOn = bOn;
	const ECollisionEnabled::Type Mode = bOn ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
	for (const TWeakObjectPtr<UBoxComponent>& F : BridgeFloor) if (F.IsValid()) F->SetCollisionEnabled(Mode);
	for (const TWeakObjectPtr<UBoxComponent>& R : BridgeRails) if (R.IsValid()) R->SetCollisionEnabled(Mode);
}

void ADysisDirector::PlaceSill(float K)
{
	// 灰盒 placeSill：整组沿半径滑。这里是从墙里往外伸（灰盒的符号是反的，看着像从半空里合过来）
	SillK = K;
	const FVector Out = DysisGB::PolarCm(float(RbwSillAz), 1.0f, 0.0f);
	SillOffset = Out * ((1.0 - K) * RbwSillSlide);
	const bool bShow = K > 0.002f, bOn = K > 0.98f;
	// 虹之龛：门开了多少、碎片飞出来多少
	const float T = IrisNicheT;
	const float DoorDeg = FMath::RadiansToDegrees(1.9f) * DysisGB::Smoothstep(0.0f, 1.2f, T);
	const FVector NicheIn = -DysisGB::PolarCm(float(RbwNicheAz), 1.0f, 0.0f);   // 从龛朝着中庭
	for (int32 i = 0; i < SillParts.Num(); ++i)
	{
		const FSillPart& P = SillParts[i];
		AActor* A = P.Actor.Get();
		if (!A) continue;
		FVector Loc = P.Base + SillOffset;
		float Yaw = P.Yaw0;
		bool bVisible = bShow;
		if (i == 2) Yaw += DoorDeg;          // 西边那扇（铰链在西）朝外开
		else if (i == 3) Yaw -= DoorDeg;
		else if (i == 4)
		{
			Loc += NicheIn * (30.0f * DysisGB::Smoothstep(1.0f, 2.0f, T));
			A->SetActorScale3D(FVector(1.0f - 0.999f * DysisGB::Smoothstep(2.0f, 2.6f, T)));
			Yaw += FMath::RadiansToDegrees(2.0f * T);
			bVisible = bShow && T < 2.6f;    // 到手以后就不在龛里了
		}
		A->SetActorLocationAndRotation(Loc, FRotator(0.0f, Yaw, 0.0f));
		if (A->IsHidden() == bVisible) A->SetActorHiddenInGame(!bVisible);
		if (P.bSolid && A->GetActorEnableCollision() != bOn)
		{
			A->SetActorEnableCollision(bOn);   // 伸到头才能踩（也才挡光）
			if (UDysisWorldState* S = UDysisWorldState::Get(this)) S->BumpLight();
		}
	}
}

void ADysisDirector::PlacePrism()
{
	const bool bShow = PrismExt > 0.0f;
	const FVector Down(0.0, 0.0, -(1.0 - PrismExt) * RbwPrismRise);
	for (int32 i = 0; i < PrismParts.Num(); ++i)
	{
		const FPrismPart& P = PrismParts[i];
		AActor* A = P.Actor.Get();
		if (!A) continue;
		float Yaw = P.Yaw0;
		if (i == 1) Yaw += PrismYawDeg - float(RbwSlots[0]);   // 棱镜：模型摆的是第 0 格
		else if (i == 3) Yaw += PrismWheelDeg;
		A->SetActorLocationAndRotation(P.Base + Down, FRotator(0.0f, Yaw, 0.0f));
		if (A->IsHidden() == bShow) A->SetActorHiddenInGame(!bShow);
	}
}

bool ADysisDirector::SpectrumHit(const FVector& OutDir, FVector& OutPos, FVector& OutNormal) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;
	FCollisionQueryParams Params(TEXT("DysisSpectrum"), /*bTraceComplex*/ true);
	if (const APlayerController* PC = World->GetFirstPlayerController())
		if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, RbwPrismPos + OutDir * 25.0, RbwPrismPos + OutDir * 40000.0, ECC_Visibility, Params)) return false;
	OutPos = Hit.ImpactPoint;
	OutNormal = Hit.ImpactNormal;
	return true;
}

void ADysisDirector::UpdateRainbow(float Dt)
{
	const UWorld* World = GetWorld();
	const float Now = World ? float(World->GetTimeSeconds()) : 0.0f;
	// 虹桥是阳光的：入夜以后整座消失（踩不了）
	if (AActor* Br = BridgeActor.Get())
	{
		const bool bShow = !bCaught && RainbowT > 0.0f;
		if (Br->IsHidden() == bShow) Br->SetActorHiddenInGame(!bShow);
	}
	if (bCaught && bBridgeOn) SetBridgeOn(false);
	else if (!bCaught && !bBridgeOn && RainbowT >= 3.8f) SetBridgeOn(true);

	if (RainbowT > 0.0f)
	{
		RainbowT += Dt;
		BridgeK = DysisGB::Smoothstep(0.6f, 3.8f, RainbowT);
		if (BridgeMID)
		{
			BridgeMID->SetScalarParameterValue(TEXT("Reveal"), BridgeK);
			BridgeMID->SetScalarParameterValue(TEXT("Opacity"), 0.62f + 0.12f * FMath::Sin(Now * 1.7f));
		}
		// 虹快到对岸的时候，南窗下探出一道石沿，窗洞西边的墙上露出虹之龛
		const float Ks = DysisGB::Smoothstep(2.4f, 4.2f, RainbowT);
		if (Ks != SillK) PlaceSill(Ks);
	}

	// 虹之龛：门打开，彩虹碎片飞出来；然后窗台上升起铜柱和棱镜
	if (IrisNicheT > 0.0f)
	{
		const float Before = IrisNicheT;
		IrisNicheT += Dt;
		if (Before < 6.0f) PlaceSill(SillK);
		if (Before < 2.6f && IrisNicheT >= 2.6f)
		{
			if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->SetShard(1, true);
			ADysisHUD::Notify(GetWorld(), DysisCopy::IrisNicheOpened, 4.6f);
		}
		const float Ext = DysisGB::Smoothstep(2.8f, 5.8f, IrisNicheT);
		if (Ext != PrismExt)
		{
			PrismExt = Ext;
			PlacePrism();
			if (PrismExt >= 1.0f && PrismBlock.IsValid()) PrismBlock->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}

	// 棱镜：朝选中的那一格转过去；在南窗的光里，就把光分成七色
	bool bOn = false;
	if (PrismExt >= 1.0f)
	{
		const float Before = PrismYawDeg;
		PrismYawDeg = DysisGB::Toward(PrismYawDeg, float(RbwSlots[PrismSlot]), FMath::RadiansToDegrees(0.5f), Dt);
		if (PrismYawDeg != Before) { PrismWheelDeg -= (PrismYawDeg - Before) * 6.0f; PlacePrism(); }
		const UDysisTimeComponent* Time = PlayerTime();
		const FVector Sun = UDysisSkyLibrary::DysisSunDir(Time ? Time->H : 0.0f);
		const ADysisBeamActor* Iris = IrisBeam.Get();
		bOn = Sun.Z > 0.0 && !bCaught && Iris && Iris->HasLight() && Iris->ContainsPoint(RbwPrismPos, 12.0);
		if (bOn)
			for (int32 k = 0; k < 7; ++k)
			{
				FVector Dir, N;
				bPrismHit[k] = RbwPrismOut(-Sun, PrismYawDeg, RbwIor[k], Dir) && SpectrumHit(Dir, PrismHitPos[k], N);
				UStaticMeshComponent* Ray = PrismRays.IsValidIndex(k) ? PrismRays[k].Get() : nullptr;
				UStaticMeshComponent* Spot = PrismSpots.IsValidIndex(k) ? PrismSpots[k].Get() : nullptr;
				if (Ray) Ray->SetVisibility(bPrismHit[k]);
				if (Spot) Spot->SetVisibility(bPrismHit[k]);
				if (!bPrismHit[k]) continue;
				const double Len = FVector::Dist(RbwPrismPos, PrismHitPos[k]);
				if (Ray)
				{
					Ray->SetWorldLocationAndRotation((RbwPrismPos + PrismHitPos[k]) * 0.5, FRotationMatrix::MakeFromZ(Dir).ToQuat());
					Ray->SetWorldScale3D(FVector(0.05, 0.05, Len / 100.0));
				}
				if (Spot)
				{
					Spot->SetWorldLocationAndRotation(PrismHitPos[k] + N * 3.0, FRotationMatrix::MakeFromZ(N).ToQuat());
					Spot->SetWorldScale3D(FVector(0.18, 0.18, 0.004));
				}
			}
	}
	if (!bOn)
		for (int32 k = 0; k < 7; ++k)
		{
			bPrismHit[k] = false;
			if (PrismRays.IsValidIndex(k) && PrismRays[k]) PrismRays[k]->SetVisibility(false);
			if (PrismSpots.IsValidIndex(k) && PrismSpots[k]) PrismSpots[k]->SetVisibility(false);
		}
	// 哪一色落在塞勒涅的眼睛上（离那一点 9 cm 以内）；靛色（第六色）停够 0.8 秒，她就醒了
	int32 Near = -1;
	double NearDist = 9.0;
	for (int32 k = 0; k < 7; ++k)
		if (bPrismHit[k])
		{
			const double D = FVector::Dist(PrismHitPos[k], RbwSeleneEye);
			if (D < NearDist) { NearDist = D; Near = k; }
		}
	SeleneColor = Near;
	SeleneLit = Near == 5 ? SeleneLit + Dt : 0.0f;
	if (SeleneLit > 0.8f && !bSeleneOn)
	{
		bSeleneOn = true;
		ADysisHUD::Notify(GetWorld(), DysisCopy::PrismPuzzleSolved, 3.0f);
		SeleneTalkIn = 1.4f;
		// 她的眼睛亮起来
		if (UStaticMeshComponent* C = RbwMesh(Piece(TEXT("SM_Mech_Selene_Relief"))))
			if (C->GetNumMaterials() > 1)
				if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Beams/M_DysisBeam.M_DysisBeam")))
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
					MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(0x3a, 0x4c, 0xff)));
					MID->SetScalarParameterValue(TEXT("Intensity"), 2.4f);
					MID->SetScalarParameterValue(TEXT("Brightness"), 1.0f);
					C->SetMaterial(1, MID);
				}
	}
	if (SeleneTalkIn > 0.0f)
	{
		SeleneTalkIn -= Dt;
		if (SeleneTalkIn <= 0.0f && SeleneDialogue) SeleneDialogue->Play();
	}
}

// ───────────────────────── 测试用 ─────────────────────────

FString ADysisDirector::DebugPrismHits(int32 Slot, float H) const
{
	Slot = FMath::Clamp(Slot, 0, int32(UE_ARRAY_COUNT(RbwSlots)) - 1);
	const FVector Din = -UDysisSkyLibrary::DysisSunDir(H);
	FString S = TEXT("{\"hits\":[");
	int32 Near = -1;
	double NearDist = 9.0;
	for (int32 k = 0; k < 7; ++k)
	{
		FVector Dir, P, N;
		if (k) S += TEXT(",");
		if (RbwPrismOut(Din, RbwSlots[Slot], RbwIor[k], Dir) && SpectrumHit(Dir, P, N))
		{
			S += FString::Printf(TEXT("[%.2f,%.2f,%.2f]"), P.X, P.Y, P.Z);
			const double D = FVector::Dist(P, RbwSeleneEye);
			if (D < NearDist) { NearDist = D; Near = k; }
		}
		else S += TEXT("null");
	}
	return S + FString::Printf(TEXT("],\"near\":%d}"), Near);
}

FString ADysisDirector::DescribeRainbow() const
{
	const ADysisBeamActor* Iris = IrisBeam.Get();
	int32 RailsOn = 0;
	for (const TWeakObjectPtr<UBoxComponent>& R : BridgeRails) if (R.IsValid() && R->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++RailsOn;
	return FString::Printf(TEXT("{\"done\":%s,\"align\":%.3f,\"miss\":%.2f,\"T\":%.3f,\"bridgeK\":%.4f,\"sillK\":%.4f,\"bridgeOn\":%s,\"rails\":%d,\"railsOn\":%d,")
		TEXT("\"nicheOpen\":%s,\"nicheT\":%.3f,\"ext\":%.4f,\"slot\":%d,\"yaw\":%.3f,\"color\":%d,\"lit\":%.2f,\"seleneOn\":%s,\"irisLight\":%s,\"prismInLight\":%s,\"talking\":%s}"),
		bReliefDone ? TEXT("true") : TEXT("false"), ReliefAlign, ReliefMissCm, RainbowT, BridgeK, SillK, bBridgeOn ? TEXT("true") : TEXT("false"), BridgeRails.Num(), RailsOn,
		bIrisNicheOpen ? TEXT("true") : TEXT("false"), IrisNicheT, PrismExt, PrismSlot, PrismYawDeg, SeleneColor, SeleneLit, bSeleneOn ? TEXT("true") : TEXT("false"),
		Iris && Iris->HasLight() ? TEXT("true") : TEXT("false"), Iris && Iris->ContainsPoint(RbwPrismPos, 12.0) ? TEXT("true") : TEXT("false"),
		SeleneDialogue && SeleneDialogue->IsPlaying() ? TEXT("true") : TEXT("false"));
}
