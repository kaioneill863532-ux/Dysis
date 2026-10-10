// 日落回廊 · “光和水”的样子：给虹桥、月桥、瀑布、海、水庭的水换上新材质，撒光粒、瀑布脚下的水雾，
// 水沫里的虹（伊莉丝浮雕前的水帘、殿顶虹门的喷泉），光做的桥落在石头上的那一汪光；
// 每帧按时刻把水的天色、瀑布的明暗、太阳的方向写进材质。全是看的东西，不参与光路和机关的判定。
// 材质由 Art/Fx/ue_make_fx_materials.py 生成（/Game/Dysis/Fx、Rainbow、Night、Beams 下）。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisNightData.generated.h"
#include "Beams/DysisBeamActor.h"
#include "Fx/DysisMoteField.h"
#include "World/DysisWorldState.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// 虹桥的两头（灰盒 BRIDGE.A / B）和桥的走向
	const FVector FxBridgeA(138.018, 1433.371, 600.0), FxBridgeB(-1474.85, 367.721, 1750.0);
	// 瀑布：方位 116.25°–139.75°，水帘在 r = 12.45 m；脚下的水面在 −45
	constexpr float FxFallAz = 128.0f, FxFallR = 1245.0f, FxWaterZ = -45.0f;
	// 伊莉丝浮雕（灰盒 IRISREL / RELIEF）：墙根、朝中庭的方向、沿墙的方向，石板宽 2.2 m、高 2.7 m
	const FVector FxRelFace(293.464, 1509.743, 600.0), FxRelN(-0.190809, -0.981627, 0.0), FxRelT(-0.981627, 0.190809, 0.0);
	constexpr float FxRelW = 220.0f, FxRelH = 270.0f;
	// 殿顶的虹门（灰盒 ARCH）：拱心在环道上的位置、正对接光台的方向、沿拱的方向，拱的半径、拱心比最高一级高多少
	const FVector FxArchC(1246.343, 766.807, 0.0), FxArchN(0.46502, 0.8853, 0.0), FxArchT(-0.8853, 0.46502, 0.0);
	constexpr float FxArchR = 153.069f, FxArchH = 155.396f;
	// 瀑布（方位 116.25°–139.75°；前后两片水帘在 r = 12.45 / 12.73 m，顶上从墙里漫出来）
	constexpr float FxFallA0 = 116.25f, FxFallA1 = 139.75f, FxFallBack = 1273.0f, FxFallTopZ = 2930.0f, FxFallWallR = 1560.0f;
	// 加厚：同一个模型绕殿心放大这么多再放一份（两片水帘落到 r ≈ 13.1 / 13.4 m），和原来的叠着看有前后的错动。
	// 只能往墙那边加：水帘前面 r ≈ 12.3 m 是虹桥的边
	constexpr float FxFallThickK = 1.0526f;
	// 环道面的高度、最高一级升到头比环道高多少（和 DysisDirectorRoof.cpp 里的 RoofRingZ / RoofTopZEnd 是同两个数）
	constexpr float FxRoofRingZ = 3030.0f, FxRoofTopZEnd = 720.0f;
	// 灰盒合成那一遍里虹的总亮度（uRainK）；Boost：UE 的曝光下再提多少才和灰盒看着一样亮
	constexpr float FxRainK = 2.4f, FxBowBoost = 1.5f;

	FLinearColor FxLerp3(const FLinearColor& A, const FLinearColor& B, float T) { return A + (B - A) * FMath::Clamp(T, 0.0f, 1.0f); }
	FLinearColor FxVec(const FVector& V) { return FLinearColor(float(V.X), float(V.Y), float(V.Z), 0.0f); }
}

UMaterialInstanceDynamic* ADysisDirector::FxMaterial(AActor* A, const TCHAR* Path)
{
	UStaticMeshComponent* C = A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	UMaterialInterface* Mat = C ? LoadObject<UMaterialInterface>(nullptr, Path) : nullptr;
	if (!Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dysis 效果：%s 没换上（找不到部件或材质；材质跑一遍 Art/Fx/ue_make_fx_materials.py）"), Path);
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Mat, this);
	for (int32 i = 0; i < C->GetNumMaterials(); ++i) C->SetMaterial(i, MID);
	// 模型原点在世界里的位置：材质里要按“离殿心多远”算东西的都用它
	MID->SetVectorParameterValue(TEXT("Pivot"), FxVec(A->GetActorLocation()));
	return MID;
}

UDysisMoteField* ADysisDirector::FxMotes(const FVector& Center, const FQuat& Rot, const FVector& SizeCm, int32 Count)
{
	// 在世界里的一个斜着的盒子里撒光粒（盒子 = 中心、朝向、三边的长度）
	UDysisMoteField* M = NewObject<UDysisMoteField>(this);
	M->SetupAttachment(RootComponent);
	M->RegisterComponent();
	M->SetWorldLocationAndRotation(Center, Rot);
	M->SetWorldScale3D(SizeCm / 100.0);
	M->Build(Count, Interacts.Num() * 131 + Count);
	M->SetOpacity(0.0f);
	return M;
}

UStaticMeshComponent* ADysisDirector::FxShape(const TCHAR* MeshPath, const TCHAR* MatPath, const FVector& Center, const FQuat& Rot, const FVector& SizeCm, TObjectPtr<UMaterialInstanceDynamic>& OutMID)
{
	// 一个只管看的形状（引擎自带的方块 / 平片，100 cm 见方）+ 它自己的一份材质
	OutMID = nullptr;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, MatPath);
	if (!Mesh || !Mat)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dysis 效果：%s 没建出来（材质跑一遍 Art/Fx/ue_make_fx_materials.py）"), MatPath);
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(RootComponent);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->SetStaticMesh(Mesh);
	C->RegisterComponent();
	C->SetWorldLocationAndRotation(Center, Rot);
	C->SetWorldScale3D(SizeCm / 100.0);
	OutMID = UMaterialInstanceDynamic::Create(Mat, this);
	C->SetMaterial(0, OutMID);
	C->SetVisibility(false);
	return C;
}

void ADysisDirector::FxGlow(const FVector& At, const FVector& Along, const FVector2D& SizeCm, const FLinearColor& Color)
{
	// 光做的桥落在石头上的地方：贴着地面的一汪光（长边顺着桥）
	TObjectPtr<UMaterialInstanceDynamic> MID;
	UStaticMeshComponent* C = FxShape(TEXT("/Engine/BasicShapes/Plane.Plane"), TEXT("/Game/Dysis/Fx/M_DysisGlowPool.M_DysisGlowPool"),
		At + FVector(0.0, 0.0, 1.5), FRotator(0.0f, float(Along.Rotation().Yaw), 0.0f).Quaternion(), FVector(SizeCm.X, SizeCm.Y, 100.0), MID);
	if (MID) MID->SetVectorParameterValue(TEXT("Color"), Color);
	GlowPools.Add(C);
	GlowMIDs.Add(MID);
}

void ADysisDirector::SetupFx()
{
	// 海、水庭的水、瀑布、月桥：换材质
	SeaMID = FxMaterial(Piece(TEXT("SM_Mech_Water_Sea")), TEXT("/Game/Dysis/Fx/M_DysisSea.M_DysisSea"));
	// 水庭的水：场景美术 2026-10-09 的动画水面（大色块、断线、岸边的波纹）；找不到就退回原来那个
	const bool bAnimeWater = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisAnimeWater.M_DysisAnimeWater"), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
	PoolMID = FxMaterial(Piece(TEXT("SM_Mech_Water_Pool")), bAnimeWater ? TEXT("/Game/Dysis/Fx/M_DysisAnimeWater.M_DysisAnimeWater") : TEXT("/Game/Dysis/Fx/M_DysisPool.M_DysisPool"));
	FallMID = FxMaterial(Piece(TEXT("SM_Mech_Water_Waterfall")), TEXT("/Game/Dysis/Fx/M_DysisWaterfall.M_DysisWaterfall"));
	SetupWaterfallLook();
	const int32 MoonN = UE_ARRAY_COUNT(GDysisMoonBridgePts);
	if (AActor* Deck = MoonDeck.Get())
		if ((MoonDeckMID = FxMaterial(Deck, TEXT("/Game/Dysis/Night/M_DysisMoonBridge.M_DysisMoonBridge"))) != nullptr)
		{
			// 桥在平面上是一段圆弧：过头、中、尾三点的圆 → 圆心、半径、从哪个角转过多少（材质里靠这几个数算“离中线多远、沿桥走了多远”）
			const FVector2D A(GDysisMoonBridgePts[0]), B(GDysisMoonBridgePts[MoonN / 2]), C(GDysisMoonBridgePts[MoonN - 1]);
			const double D = 2.0 * (A.X * (B.Y - C.Y) + B.X * (C.Y - A.Y) + C.X * (A.Y - B.Y));
			if (FMath::Abs(D) > 1.0e-3)
			{
				const FVector2D O(
					(A.SizeSquared() * (B.Y - C.Y) + B.SizeSquared() * (C.Y - A.Y) + C.SizeSquared() * (A.Y - B.Y)) / D,
					(A.SizeSquared() * (C.X - B.X) + B.SizeSquared() * (A.X - C.X) + C.SizeSquared() * (B.X - A.X)) / D);
				const double Ang0 = FMath::Atan2(A.Y - O.Y, A.X - O.X);
				const double Sgn = FVector2D::CrossProduct(A - O, B - O) >= 0.0 ? 1.0 : -1.0;
				double Span = Sgn * (FMath::Atan2(C.Y - O.Y, C.X - O.X) - Ang0);
				while (Span <= 0.0) Span += 2.0 * UE_DOUBLE_PI;
				MoonDeckMID->SetVectorParameterValue(TEXT("ArcC"), FLinearColor(float(O.X), float(O.Y), 0.0f, 0.0f));
				MoonDeckMID->SetScalarParameterValue(TEXT("ArcR"), float((A - O).Size()));
				MoonDeckMID->SetScalarParameterValue(TEXT("Ang0"), float(Ang0));
				MoonDeckMID->SetScalarParameterValue(TEXT("AngSpan"), float(Sgn * Span));
			}
			MoonDeckMID->SetScalarParameterValue(TEXT("Reveal"), 0.0f);
			if (UStaticMeshComponent* C2 = Deck->FindComponentByClass<UStaticMeshComponent>()) C2->SetCastShadow(false);   // 光做的桥不投影子
		}

	// 虹桥上往上飘的光点：一个顺着桥斜放的盒子
	{
		const FVector Mid = (FxBridgeA + FxBridgeB) * 0.5 + FVector(0.0, 0.0, 110.0);
		BridgeMotes = FxMotes(Mid, FRotationMatrix::MakeFromXZ(FxBridgeB - FxBridgeA, FVector::UpVector).ToQuat(), FVector(FVector::Dist(FxBridgeA, FxBridgeB), 190.0, 300.0), 220);
		BridgeMotes->SetLook(FLinearColor(1.0f, 0.96f, 0.86f), 5.5f, FVector(0.0, 0.0, 0.04), 0.03f, 3.0f);
	}
	// 月桥上的光点：桥是一道弧，分六段各放一个盒子
	MoonMotes.Reset();
	for (int32 i0 = 0; i0 + 1 < MoonN; i0 += 12)
	{
		const FVector A = GDysisMoonBridgePts[i0], B = GDysisMoonBridgePts[FMath::Min(i0 + 12, MoonN - 1)];
		UDysisMoteField* M = FxMotes((A + B) * 0.5 + FVector(0.0, 0.0, 95.0), FRotationMatrix::MakeFromXZ(B - A, FVector::UpVector).ToQuat(), FVector(FVector::Dist(A, B) + 40.0, 230.0, 250.0), 44);
		M->SetLook(FLinearColor(0.66f, 0.80f, 1.0f), 5.0f, FVector(0.0, 0.0, 0.035), 0.03f, 3.2f);
		MoonMotes.Add(M);
	}
	// 瀑布脚下：一团水雾（用光柱那个“体积”的材质，颜色是水汽的白）和水沫
	{
		const FVector Base = DysisGB::PolarCm(FxFallAz, FxFallR - 60.0f, FxWaterZ + 120.0f);
		const FQuat Rot = FRotator(0.0f, FxFallAz + 90.0f, 0.0f).Quaternion();   // 盒子的长边沿着水帘
		FallMist = FxShape(TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Game/Dysis/Beams/M_DysisBeam.M_DysisBeam"), Base, Rot, FVector(760.0, 460.0, 380.0), FallMistMID);
		if (FallMistMID)
		{
			FallMistMID->SetScalarParameterValue(TEXT("FullDepth"), 260.0f);
			FallMistMID->SetScalarParameterValue(TEXT("Soft"), 130.0f);
			FallMistMID->SetScalarParameterValue(TEXT("StartSoft"), 160.0f);
			FallMistMID->SetScalarParameterValue(TEXT("EndSoft"), 160.0f);
			FallMistMID->SetScalarParameterValue(TEXT("NoiseAmt"), 0.9f);
			FallMistMID->SetScalarParameterValue(TEXT("Brightness"), 0.95f);
			FallMistMID->SetScalarParameterValue(TEXT("Phase"), 0.0f);          // 水雾不分迎着光、背着光
			FallMistMID->SetScalarParameterValue(TEXT("Round"), 1.0f);          // 一团，不是一根柱子
			FallMistMID->SetScalarParameterValue(TEXT("Intensity"), 0.0f);
		}
		FallMotes = FxMotes(Base + FVector(0.0, 0.0, 90.0), Rot, FVector(640.0, 330.0, 440.0), 130);
		FallMotes->SetLook(FLinearColor(0.85f, 0.93f, 1.0f), 8.0f, FVector(0.0, 0.0, 0.06), 0.05f, 1.5f);
		// 溅起来的水珠：从水帘落到水面的那一线蹦出来，往中庭这边抛出去（盒子的长边沿着水帘，+Y 朝中庭）
		auto Drops = [this](const FVector& Center, const FQuat& BoxRot, const FVector& SizeCm, int32 Count, const FVector& Burst, float Life, float DropCm)
		{
			UDysisMoteField* M = NewObject<UDysisMoteField>(this);
			M->SetupAttachment(RootComponent);
			M->RegisterComponent();
			M->SetWorldLocationAndRotation(Center, BoxRot);
			M->SetWorldScale3D(SizeCm / 100.0);
			M->UseMaterial(TEXT("/Game/Dysis/Fx/M_DysisDroplet.M_DysisDroplet"));
			M->Build(Count, Count * 17 + 5);
			if (UMaterialInstanceDynamic* MID = M->GetMID())
			{
				MID->SetVectorParameterValue(TEXT("SizeCm"), FLinearColor(float(SizeCm.X), float(SizeCm.Y), float(SizeCm.Z), 0.0f));
				MID->SetVectorParameterValue(TEXT("Burst"), FLinearColor(float(Burst.X), float(Burst.Y), float(Burst.Z), 0.0f));
				MID->SetScalarParameterValue(TEXT("Life"), Life);
				MID->SetScalarParameterValue(TEXT("Size"), DropCm);
			}
			M->SetOpacity(0.0f);
			return M;
		};
		const FQuat Out = FRotator(0.0f, FxFallAz + 90.0f, 0.0f).Quaternion();
		const FVector BoxCm(520.0, 300.0, 240.0);
		FallDrops = Drops(DysisGB::PolarCm(FxFallAz, FxFallR - 14.0f - float(BoxCm.Y) * 0.5f, FxWaterZ + float(BoxCm.Z) * 0.5f), Out, BoxCm, 190, FVector(70.0, 210.0, 390.0), 0.9f, 5.0f);
		// 两边：水帘散出去的水珠，一路往下掉（用光粒那一套：竖着的一个高盒子，每秒往下飘过大半个盒子，到两头淡出去）
		FallEdgeDrops.Reset();
		for (const float Az : { FxFallA0 + 0.5f, FxFallA1 - 0.5f })
		{
			const FVector Tall(140.0, 170.0, 1500.0);
			UDysisMoteField* M = FxMotes(DysisGB::PolarCm(Az, FxFallR - 30.0f, FxWaterZ + float(Tall.Z) * 0.5f), Out, Tall, 64);
			M->SetLook(FLinearColor(0.85f, 0.93f, 1.0f), 4.5f, FVector(0.0, 0.0, -0.55), 0.012f, 1.0f);
			FallEdgeDrops.Add(M);
		}
	}

	// 水沫里的虹 ①：伊莉丝浮雕前的水帘（灰盒 IRISREL.box：比石板左右各宽 0.5 m、厚 0.9 m，贴着浮雕面）
	{
		const FVector Size(FxRelW + 100.0f, 90.0, FxRelH + 10.0f);
		const FVector Center = FxRelFace + FxRelN * (5.0 + Size.Y * 0.5) + FVector(0.0, 0.0, 2.0 + Size.Z * 0.5);
		const FQuat Rot = FRotationMatrix::MakeFromXY(FxRelT, FxRelN).ToQuat();   // 盒子：x 沿墙、y 朝中庭（厚度）、z 朝上
		ReliefSpray = FxShape(TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Game/Dysis/Rainbow/M_DysisSprayBow.M_DysisSprayBow"), Center, Rot, Size, ReliefSprayMID);
		if (ReliefSprayMID)
		{
			ReliefSprayMID->SetVectorParameterValue(TEXT("EdgeLo"), FLinearColor(0.10f, 0.20f, 0.0001f, 0.0f));
			ReliefSprayMID->SetVectorParameterValue(TEXT("EdgeHi"), FLinearColor(0.10f, 0.40f, 0.15f, 0.0f));
			ReliefSprayMID->SetScalarParameterValue(TEXT("NoiseScale"), 1.6f);
			ReliefSprayMID->SetScalarParameterValue(TEXT("NoiseSpeed"), 0.9f);    // 水帘往下淌
			ReliefSprayMID->SetVectorParameterValue(TEXT("Center"), FxVec(Center));
			ReliefSprayMID->SetScalarParameterValue(TEXT("BowPlane"), -46.0f);   // 虹落在贴着浮雕面的那一层上（和刻着的那一圈对得上）
		}
		ReliefDrops = FxMotes(Center, Rot, FVector(Size.X * 0.8, Size.Y * 0.6, Size.Z), 110);
		ReliefDrops->SetLook(FLinearColor(0.90f, 0.95f, 1.0f), 3.2f, FVector(0.0, 0.0, -0.42), 0.006f, 1.3f);
	}
	// 水沫里的虹 ②：殿顶虹门的喷泉水雾（灰盒 gateBox：比拱左右各宽 0.5 m、厚 1 m，从最高一级的面到拱顶上面 0.6 m；跟着最高一级升降）
	{
		const FVector Size(2.0f * (FxArchR + 50.0f), 100.0, FxArchH + FxArchR + 60.0f);
		const FVector Center(FxArchC.X, FxArchC.Y, FxRoofRingZ + Size.Z * 0.5);
		const FQuat Rot = FRotationMatrix::MakeFromXY(-FxArchT, FxArchN).ToQuat();
		GateSpray = FxShape(TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Game/Dysis/Rainbow/M_DysisSprayBow.M_DysisSprayBow"), Center, Rot, Size, GateSprayMID);
		if (GateSprayMID)
		{
			GateSprayMID->SetVectorParameterValue(TEXT("EdgeLo"), FLinearColor(0.12f, 0.30f, 0.08f, 0.0f));
			GateSprayMID->SetVectorParameterValue(TEXT("EdgeHi"), FLinearColor(0.12f, 0.30f, 0.20f, 0.0f));
			GateSprayMID->SetScalarParameterValue(TEXT("NoiseScale"), 1.3f);
			GateSprayMID->SetScalarParameterValue(TEXT("NoiseSpeed"), -0.6f);     // 喷泉往上涌
		}
		GateDrops = FxMotes(Center, Rot, FVector(2.0f * FxArchR, 80.0, Size.Z - 30.0), 90);
		GateDrops->SetLook(FLinearColor(0.92f, 0.96f, 1.0f), 3.4f, FVector(0.0, 0.0, 0.22), 0.02f, 1.3f);
	}

	// 光做的桥落在石头上的地方各一汪光：虹桥的桥头（一层走廊）、桥尾（窗台）；月桥的桥头（一层）、桥尾（水庭）
	GlowPools.Reset();
	GlowMIDs.Reset();
	{
		const FVector Ax = (FxBridgeB - FxBridgeA).GetSafeNormal2D();
		FxGlow(FxBridgeA + Ax * 25.0, Ax, FVector2D(230.0, 180.0), FLinearColor(1.0f, 0.93f, 0.84f));
		FxGlow(FxBridgeB - Ax * 10.0, Ax, FVector2D(200.0, 170.0), FLinearColor(1.0f, 0.93f, 0.84f));
		const FVector M0 = GDysisMoonBridgePts[0], M1 = GDysisMoonBridgePts[MoonN - 1];
		FxGlow(M0, GDysisMoonBridgePts[1] - M0, FVector2D(260.0, 220.0), FLinearColor(0.60f, 0.76f, 1.0f));
		FxGlow(M1, M1 - GDysisMoonBridgePts[MoonN - 2], FVector2D(300.0, 230.0), FLinearColor(0.60f, 0.76f, 1.0f));
	}
}

void ADysisDirector::UpdateFx(float Dt)
{
	const UDysisTimeComponent* Time = PlayerTime();
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const ADysisSkyActor* Sky = Time ? Time->SkyActor.Get() : nullptr;
	float SunAlt = 30.0f, MoonAlt = -30.0f;
	if (Sky) Sky->GetAltitudes(SunAlt, MoonAlt);
	const float Daylight = DysisGB::Smoothstep(-6.0f, 5.0f, SunAlt);          // 天上还有多少日光
	const float Dusk = Sky ? Sky->GetDuskLook() : 0.0f;                        // 黄昏的调子有多浓
	const float Night = DysisGB::Smoothstep(-2.0f, -10.0f, SunAlt);
	const float MoonUp = DysisGB::Smoothstep(0.0f, 12.0f, MoonAlt);

	// 水面映出的天色：白天淡蓝、黄昏桃红（比天边的橙淡一些：海映的是大半个天，不只是日落的那一边）、夜里是带一点月色的深蓝
	const FLinearColor SkyTint = FxLerp3(FLinearColor(0.40f, 0.58f, 0.86f), FLinearColor(0.92f, 0.54f, 0.44f), Dusk) * (Daylight * 0.9f)
		+ FLinearColor(0.035f, 0.055f, 0.12f) * (Night * (0.7f + 0.9f * MoonUp));
	if (SeaMID) SeaMID->SetVectorParameterValue(TEXT("SkyTint"), SkyTint);
	if (PoolMID)
	{
		PoolMID->SetVectorParameterValue(TEXT("SkyTint"), SkyTint * 0.8f);          // 原来那个水面用的；动画水面没有这个参数
		// 动画水面：瀑布开了水就流得快一些、断线密一些（美术定的 Flow / Ripple 两个状态）；
		// 底色垫多亮（屏幕上的亮度）：白天殿里亮，垫得多；入夜只留一点，剩下的靠月光照
		const float PoolFlow = State ? State->FlowK : 0.0f;
		PoolMID->SetScalarParameterValue(TEXT("FlowState"), PoolFlow);
		PoolMID->SetScalarParameterValue(TEXT("RippleState"), PoolFlow);
		PoolMID->SetScalarParameterValue(TEXT("Lift"), PoolLiftDay * Daylight + PoolLiftNight * Night);
	}

	// 瀑布和脚下的水雾：白天是亮的水白，黄昏偏暖，夜里只剩月光下的一点银蓝
	const float Flow = State ? State->FlowK : 0.0f;
	const FLinearColor FallTint = FxLerp3(FLinearColor(0.80f, 0.90f, 1.0f), FLinearColor(1.0f, 0.78f, 0.58f), Dusk) * (0.18f + 0.60f * Daylight) * FMath::Lerp(1.0f, 0.82f, Dusk)
		+ FLinearColor(0.30f, 0.40f, 0.62f) * (Night * (0.10f + 0.16f * MoonUp));
	if (FallMID)
	{
		FallMID->SetVectorParameterValue(TEXT("Tint"), FallTint);
		FallMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Flow * 1.2f, 0.0f, 1.0f));
		// 夜里镜子反射的月光打到的那一块水帘透开（灰盒 updateFallHole）：光从镜心出发，和前后两层水帘（r = 12.45 m、12.73 m）的交点取中间
		float HoleWant = 0.0f;
		const ADysisBeamActor* Moon = MoonMirrorBeam.Get();
		if (bCaught && Moon && Moon->HasLight() && Flow > 0.02f)
		{
			const FVector O = Moon->WindowCm, L = Moon->GetTravelDir();
			TArray<FVector, TInlineAllocator<2>> Hits;
			for (const double R : { double(FxFallR), double(FxFallR) + 28.0 })
			{
				const double A = L.X * L.X + L.Y * L.Y, B = 2.0 * (O.X * L.X + O.Y * L.Y), C = O.X * O.X + O.Y * O.Y - R * R, D = B * B - 4.0 * A * C;
				if (A < 1.0e-9 || D < 0.0) continue;
				const double T = (-B + FMath::Sqrt(D)) / (2.0 * A);
				if (T <= 0.0) continue;
				const FVector P = O + L * T;
				const float Az = DysisGB::AzOf(P);
				if (Az > 115.25f && Az < 140.75f && P.Z > FxWaterZ && P.Z < 2950.0) Hits.Add(P);
			}
			if (Hits.Num() > 0)
			{
				FallHole = (Hits[0] + Hits.Last()) * 0.5;
				HoleWant = float(Moon->GetLit());
			}
		}
		FallHoleK = DysisGB::Toward(FallHoleK, HoleWant, 3.0f, Dt);
		FallMID->SetVectorParameterValue(TEXT("Hole"), FxVec(FallHole));
		FallMID->SetScalarParameterValue(TEXT("HoleK"), FallHoleK);
		for (UMaterialInstanceDynamic* More : FallMoreMIDs)
			if (More)
			{
				More->SetVectorParameterValue(TEXT("Tint"), FallTint);
				More->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Flow * 1.2f, 0.0f, 1.0f));
				More->SetVectorParameterValue(TEXT("Hole"), FxVec(FallHole));
				More->SetScalarParameterValue(TEXT("HoleK"), FallHoleK);
			}
		// 开了闸、水下来以后：瀑布这一块（连同后面的水槽和女神像）人进不去了
		const ECollisionEnabled::Type Mode = Flow > 0.15f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
		// 镜头也进不去（2026-10-10 用户：镜头在瀑布外面的时候，转视角也不要让它钻进瀑布里）：水帘对镜头来说就是一堵墙。
		// 人自己在里面的时候不拦（水下来时正好站在里面，要走出来）——不然镜头会被顶到脸上。
		bool bInside = false;
		if (Time)
		{
			const float Az = DysisGB::AzOf(Time->FootCm), R = DysisGB::ROf(Time->FootCm);
			bInside = Time->Zone == TEXT("wfback") || (R > FxFallR - 30.0f && R < FxFallWallR + 30.0f && Az > FxFallA0 - 1.0f && Az < FxFallA1 + 1.0f);
		}
		const ECollisionResponse Cam = bInside ? ECR_Ignore : ECR_Block;
		for (const TWeakObjectPtr<UBoxComponent>& B : FallBlocks)
			if (B.IsValid())
			{
				if (B->GetCollisionEnabled() != Mode) B->SetCollisionEnabled(Mode);
				if (B->GetCollisionResponseToChannel(ECC_Camera) != Cam) B->SetCollisionResponseToChannel(ECC_Camera, Cam);
			}
	}
	if (FallMist && FallMistMID)
	{
		const float K = Flow * (0.05f + 0.20f * Daylight + 0.07f * Night * MoonUp);
		FallMist->SetVisibility(K > 0.01f);
		FallMistMID->SetScalarParameterValue(TEXT("Intensity"), K);
		FallMistMID->SetVectorParameterValue(TEXT("Color"), FxLerp3(FLinearColor(0.80f, 0.90f, 1.0f), FLinearColor(1.0f, 0.82f, 0.64f), Dusk) * (1.0f - 0.35f * Night) + FLinearColor(0.0f, 0.0f, 0.1f) * Night);
	}
	if (FallMotes) FallMotes->SetOpacity(Flow * (0.25f + 0.75f * FMath::Max(Daylight, Night * MoonUp * 0.6f)));
	{
		// 水珠：白天是亮的水白，夜里只剩月光下的一点银蓝
		const float DropK = Flow * (0.10f + 0.90f * FMath::Max(Daylight, Night * (0.25f + 0.45f * MoonUp)));
		const FLinearColor DropColor = FxLerp3(FLinearColor(0.85f, 0.93f, 1.0f), FLinearColor(1.0f, 0.84f, 0.68f), Dusk) * (1.0f - 0.5f * Night) + FLinearColor(0.10f, 0.16f, 0.32f) * Night;
		auto Paint = [&](UDysisMoteField* M, float K)
		{
			if (!M) return;
			if (UMaterialInstanceDynamic* MID = M->GetMID())
			{
				MID->SetVectorParameterValue(TEXT("Color"), DropColor);
				MID->SetScalarParameterValue(TEXT("Glow"), 0.9f);
			}
			M->SetOpacity(K);
		};
		Paint(FallDrops, DropK);
		for (UDysisMoteField* M : FallEdgeDrops) Paint(M, DropK * 0.8f);
	}

	// 虹桥上的光点：桥长出来多少就亮多少，入夜桥没了就没了
	const float BridgeShown = bCaught ? 0.0f : BridgeK;
	if (BridgeMotes) BridgeMotes->SetOpacity(DysisGB::Smoothstep(0.55f, 1.0f, BridgeShown));

	// 月桥：显形多少（材质里是从一层那头淌下来的），光点跟着
	float MoonK = 0.0f;
	if (const FMoonstone* Br = FindMoonstone(TEXT("moonBridge"))) MoonK = Br->K;
	if (MoonDeckMID) MoonDeckMID->SetScalarParameterValue(TEXT("Reveal"), MoonK);
	for (UDysisMoteField* M : MoonMotes) if (M) M->SetOpacity(MoonK);

	// 桥落地的那几汪光：虹桥从桥头长到桥尾，月桥从一层淌到水庭——哪一头到了哪一头才亮
	const float GlowK[4] = {
		0.32f * DysisGB::Smoothstep(0.0f, 0.12f, BridgeShown), 0.32f * DysisGB::Smoothstep(0.92f, 1.0f, BridgeShown),
		0.50f * DysisGB::Smoothstep(0.0f, 0.12f, MoonK), 0.50f * DysisGB::Smoothstep(0.90f, 1.0f, MoonK) };
	for (int32 i = 0; i < GlowPools.Num() && i < 4; ++i)
		if (GlowPools[i] && GlowMIDs[i])
		{
			GlowPools[i]->SetVisibility(GlowK[i] > 0.005f);
			GlowMIDs[i]->SetScalarParameterValue(TEXT("Opacity"), GlowK[i]);
		}

	// 水沫里的虹：太阳在哪、多亮（灰盒合成那一遍的 uL、uSunCol、uRelOn、uGateOn）
	const FVector Sun = Time ? UDysisSkyLibrary::DysisSunDir(Time->H) : FVector(0.0, 0.0, 1.0);
	const FLinearColor SunCol = FxLerp3(FLinearColor(1.0f, 0.93f, 0.82f), FLinearColor(1.0f, 0.62f, 0.36f), Dusk);
	const FVector Eye = Time ? Time->FootCm + FVector(0.0, 0.0, 162.0) : FVector::ZeroVector;   // 人的眼睛（灰盒 eye = 1.62 m）
	if (ReliefSpray && ReliefSprayMID)
	{
		// 浮雕前的水帘：开了闸才有水，窗光照着它才有虹
		const ADysisBeamActor* Iris = IrisBeam.Get();
		if (!Iris && GetWorld())
			for (TActorIterator<ADysisBeamActor> It(GetWorld()); It; ++It)
				if (It->GreyboxId == TEXT("iris")) { IrisBeam = *It; Iris = *It; break; }
		const float On = (!bCaught && Iris && Iris->HasLight()) ? Flow * float(Iris->GetLit()) : 0.0f;
		ReliefSpray->SetVisibility(On > 0.005f);
		ReliefSprayMID->SetVectorParameterValue(TEXT("LightDir"), FxVec(-Sun));
		ReliefSprayMID->SetVectorParameterValue(TEXT("Eye"), FxVec(Eye));
		ReliefSprayMID->SetVectorParameterValue(TEXT("SunColor"), SunCol);
		ReliefSprayMID->SetScalarParameterValue(TEXT("Gain"), On * FxRainK * 0.55f * FxBowBoost);
		ReliefSprayMID->SetScalarParameterValue(TEXT("SunGain"), On * 0.02f);
		if (ReliefDrops) ReliefDrops->SetOpacity(bCaught ? 0.0f : Flow * (0.2f + 0.6f * Daylight));
	}
	if (GateSpray && GateSprayMID)
	{
		// 虹门的喷泉：最高一级升起来、太阳还在天上才有；跟着最高一级走
		const float Up = bCaught ? 0.0f : DysisGB::Smoothstep(0.3f, 0.95f, CrownUp);
		const float On = Up * DysisGB::Smoothstep(-0.5f, 1.0f, SunAlt);
		FVector P = GateSpray->GetComponentLocation();
		P.Z = FxRoofRingZ + FxRoofTopZEnd * CrownUp + GateSpray->GetComponentScale().Z * 50.0;
		GateSpray->SetWorldLocation(P);
		GateSpray->SetVisibility(On > 0.005f);
		GateSprayMID->SetVectorParameterValue(TEXT("LightDir"), FxVec(-Sun));
		GateSprayMID->SetVectorParameterValue(TEXT("Eye"), FxVec(Eye));
		GateSprayMID->SetVectorParameterValue(TEXT("SunColor"), SunCol);
		GateSprayMID->SetScalarParameterValue(TEXT("Gain"), On * FxRainK * 0.42f * FxBowBoost);
		GateSprayMID->SetScalarParameterValue(TEXT("SunGain"), On * 0.03f);
		GateSprayMID->SetScalarParameterValue(TEXT("ShadowZ"), float(UDysisWorldState::ShadowZ(Sun)));
		GateSprayMID->SetVectorParameterValue(TEXT("Center"), FxVec(P));
		if (GateDrops)
		{
			GateDrops->SetWorldLocation(P);
			GateDrops->SetOpacity(Up * (0.15f + 0.6f * DysisGB::Smoothstep(-2.0f, 2.0f, SunAlt)));
		}
	}

	// 拍照用的那台镜头：曝光、黄昏的调子照搬人的镜头
	if (DebugCam)
		if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
			if (const APawn* Pawn = PC->GetPawn())
				if (const UCameraComponent* Mine = Pawn->FindComponentByClass<UCameraComponent>())
					if (UCameraComponent* Cam = DebugCam->GetCameraComponent())
					{
						Cam->PostProcessSettings = Mine->PostProcessSettings;
						Cam->PostProcessBlendWeight = 1.0f;
					}
}

// ───────────────────────── 拍照用（测试脚本叫） ─────────────────────────

void ADysisDirector::DebugCamera(FVector EyeCm, FVector LookAtCm, float FovDeg)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	const FRotator Rot = FRotationMatrix::MakeFromX(LookAtCm - EyeCm).Rotator();
	if (!DebugCam)
	{
		DebugCam = World->SpawnActor<ACameraActor>(EyeCm, Rot);
		if (UCameraComponent* Cam = DebugCam ? DebugCam->GetCameraComponent() : nullptr) Cam->bConstrainAspectRatio = false;
	}
	if (!DebugCam) return;
	DebugCam->SetActorLocationAndRotation(EyeCm, Rot);
	if (UCameraComponent* Cam = DebugCam->GetCameraComponent()) Cam->SetFieldOfView(FovDeg);
	PC->SetViewTarget(DebugCam);
}

void ADysisDirector::DebugCameraOff()
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->GetPawn()) PC->SetViewTarget(PC->GetPawn());
}

// ───────────────────────── 瀑布：加厚、两头封上、开闸后挡人 ─────────────────────────

void ADysisDirector::SetupWaterfallLook()
{
	// 2026-10-10 用户定：瀑布要有厚度，左右两边也挡上，开了水闸以后人就没法靠近后面的女神像了
	// （灰盒里瀑布是两片薄水帘，从东边的石台上能绕到水帘后面，一直走到女神像跟前）。
	for (UStaticMeshComponent* C : FallMore) if (C) C->DestroyComponent();
	for (UStaticMeshComponent* C : FallExtras) if (C) C->DestroyComponent();
	FallMore.Reset(); FallExtras.Reset(); FallMoreMIDs.Reset(); FallBlocks.Reset();
	AActor* Fall = Piece(TEXT("SM_Mech_Water_Waterfall"));
	UStaticMeshComponent* Src = Fall ? Fall->FindComponentByClass<UStaticMeshComponent>() : nullptr;
	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisWaterfall.M_DysisWaterfall"));
	if (!Src || !Mat) return;
	// 挂在瀑布这个 Actor 上：没水的时候它整个藏起来，这几片跟着藏
	auto Sheet = [&](UStaticMesh* Mesh, const FVector& Loc, const FQuat& Rot, const FVector& Scale, float Phase, float Flat, float Density, float Strand = 0.0f, UMaterialInterface* Other = nullptr)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Fall);
		C->SetupAttachment(Src);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(Loc, Rot);
		C->SetWorldScale3D(Scale);
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Other ? Other : Mat, this);
		MID->SetVectorParameterValue(TEXT("Pivot"), FxVec(Loc));
		MID->SetScalarParameterValue(TEXT("Phase"), Phase);
		MID->SetScalarParameterValue(TEXT("Flat"), Flat);
		MID->SetScalarParameterValue(TEXT("Density"), Density);
		MID->SetScalarParameterValue(TEXT("Strand"), Strand);
		for (int32 i = 0; i < FMath::Max(C->GetNumMaterials(), 1); ++i) C->SetMaterial(i, MID);
		(Other ? FallExtras : FallMore).Add(C);
		FallMoreMIDs.Add(MID);   // 颜色、显出来多少每帧一起给
		return MID;
	};
	if (FallMID) FallMID->SetScalarParameterValue(TEXT("Strand"), 0.25f);   // 主水帘：往下只散开一点
	// ① 加厚：往墙那边再放一层
	{
		const FVector L = Src->GetComponentLocation();
		const FVector S = Src->GetComponentScale();
		Sheet(Src->GetStaticMesh(), FVector(L.X * FxFallThickK, L.Y * FxFallThickK, L.Z), Src->GetComponentQuat(), FVector(S.X * FxFallThickK, S.Y * FxFallThickK, S.Z), 1.0f, 0.0f, 0.85f);
	}
	// ② 两头各立一片水，从最前面的水帘一直到墙：侧面看过去也是水，看不到水帘后面
	if (PlaneMesh)
		for (const float Az : { FxFallA0, FxFallA1 })
		{
			const FVector Radial = DysisGB::PolarCm(Az, 1.0f, 0.0f).GetSafeNormal();
			const float R0 = FxFallR - 2.0f, R1 = FxFallWallR, Z0 = FxWaterZ, Z1 = FxFallTopZ;
			Sheet(PlaneMesh, DysisGB::PolarCm(Az, (R0 + R1) * 0.5f, (Z0 + Z1) * 0.5f), FRotationMatrix::MakeFromXY(Radial, FVector::UpVector).ToQuat(),
				FVector((R1 - R0) / 100.0, (Z1 - Z0) / 100.0, 1.0), Az == FxFallA0 ? 2.0f : 3.0f, 1.0f, 1.15f);
		}
	// ④ 第二版（2026-10-10 用户：原来平着下来一片、边也硬，要自然些，加粒子，入水的地方也设计一下）：
	//    最前面再罩一层全是散开水股的（比主水帘靠外不到 20 cm——再往外人就能碰到了）；
	//    脚下两层往上蹿的水花；水面上一片白沫，波纹一圈圈往中庭推。水珠在 SetupFx 里（和脚下的水沫放在一起）。
	{
		const FVector L = Src->GetComponentLocation();
		const FVector S = Src->GetComponentScale();
		const float Kf = 0.9855f;
		Sheet(Src->GetStaticMesh(), FVector(L.X * Kf, L.Y * Kf, L.Z), Src->GetComponentQuat(), FVector(S.X * Kf, S.Y * Kf, S.Z), 4.0f, 0.0f, 0.62f, 1.0f);
		if (UMaterialInterface* Splash = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisFallSplash.M_DysisFallSplash"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			// 瀑布的模型从水面（−45）到 2930：竖着压扁成水面以上两米多的一段，再把底对回水面
			int32 i = 0;
			for (const float Kr : { 0.992f, 0.978f })
			{
				const float Kz = i == 0 ? 0.085f : 0.062f;
				UMaterialInstanceDynamic* M = Sheet(Src->GetStaticMesh(), FVector(L.X * Kr, L.Y * Kr, L.Z + FxWaterZ * (1.0f - Kz)), Src->GetComponentQuat(),
					FVector(S.X * Kr, S.Y * Kr, S.Z * Kz), 6.0f + i, 0.0f, 1.0f, 0.0f, Splash);
				M->SetScalarParameterValue(TEXT("BottomZ"), FxWaterZ);
				M->SetScalarParameterValue(TEXT("Height"), (FxFallTopZ - FxWaterZ) * Kz);
				++i;
			}
		}
		UMaterialInterface* Foam = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisFallFoam.M_DysisFallFoam"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Foam && PlaneMesh)
		{
			// 一块平片贴在水面上（高出一点点），盖住从水帘后面的石台到往中庭七米；哪里有白沫是材质里按离落点多远算的
			UMaterialInstanceDynamic* M = Sheet(PlaneMesh, DysisGB::PolarCm(FxFallAz, FxFallR - 230.0f, FxWaterZ + 1.5f), FRotator(0.0f, FxFallAz, 0.0f).Quaternion(),
				FVector(9.6, 13.0, 1.0), 0.0f, 0.0f, 1.0f, 0.0f, Foam);
			M->SetScalarParameterValue(TEXT("R0"), FxFallR);
		}
	}
	// ③ 挡人的墙（不挡光；开闸以后才生效，人在外面的时候也挡镜头）：两头各一道，前面沿水帘四段
	auto Block = [this](const FVector& Center, float YawDeg, const FVector& Half)
	{
		UBoxComponent* Box = MakeWall();
		Box->SetBoxExtent(Half);
		Box->SetWorldLocationAndRotation(Center, FRotator(0.0f, YawDeg, 0.0f));
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->ComponentTags.Add(TEXT("DysisNoTrap"));   // 水下来的时候人要是正好站在里面，不会被卡死
		FallBlocks.Add(Box);
	};
	const float ZMid = (FxWaterZ - 60.0f + FxFallTopZ) * 0.5f, ZHalf = (FxFallTopZ - FxWaterZ + 60.0f) * 0.5f;
	for (const float Az : { FxFallA0, FxFallA1 })
		Block(DysisGB::PolarCm(Az, (FxFallR + FxFallWallR) * 0.5f, ZMid), Az, FVector((FxFallWallR - FxFallR) * 0.5f, 6.0f, ZHalf));
	const int32 Segs = 4;
	const float Step = (FxFallA1 - FxFallA0) / Segs;
	for (int32 i = 0; i < Segs; ++i)
	{
		const float Az = FxFallA0 + Step * (i + 0.5f);
		const float HalfChord = FxFallR * FMath::Tan(FMath::DegreesToRadians(Step * 0.5f)) + 4.0f;
		Block(DysisGB::PolarCm(Az, FxFallR - 6.0f, ZMid), Az, FVector(6.0f, HalfChord, ZHalf));
	}
}

FString ADysisDirector::DescribeWaterfall() const
{
	int32 Sheets = 0, Sides = 0, Blocking = 0;
	for (const UStaticMeshComponent* C : FallMore)
		if (C) { ++Sheets; if (C->GetStaticMesh() == PlaneMesh) ++Sides; }
	for (const TWeakObjectPtr<UBoxComponent>& B : FallBlocks)
		if (B.IsValid() && B->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++Blocking;
	return FString::Printf(TEXT("{\"extraSheets\":%d,\"sideSheets\":%d,\"blocks\":%d,\"blocking\":%d,\"splashFoam\":%d,\"drops\":%d}"), Sheets, Sides, FallBlocks.Num(), Blocking,
		FallExtras.Num(), (FallDrops ? FallDrops->GetInstanceCount() : 0) + (FallEdgeDrops.Num() > 0 && FallEdgeDrops[0] ? FallEdgeDrops[0]->GetInstanceCount() * FallEdgeDrops.Num() : 0));
}
