// 机关总管 · 屋顶：灰盒 v0.12 的 buildCrown / placePiece / updateCrown / updateIris / catchLight / updateCatch。
//   · 环道分成一块块：细桥落脚的一块、白天顺时针升起来的 16 级、夜里逆时针降下去的 16 级、其余不动的。
//   · 每一块内侧带一片光圈叶片。灰盒里叶片的形状随开合重新生成；这里的叶片是做好的模型（合拢时的样子），
//     张开时把它沿半径朝外沿缩短——内缘退到灰盒同样的半径上。
//   · 每一块两边有看不见的墙（跟着踏面走）：人不会掉进光圈，也走不出屋顶。
//   · 细桥的桥门：人走近桥尾自己转开；接住最后一缕光以后转回来锁死。
//   · 接光：踏步完全升起、岛影还没漫过举起来的苹果时，在浑天仪旁按 E 取下金苹果——从这一刻起入夜。
#include "DysisDirector.h"
#include "Audio/DysisMusicManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "DysisRoofData.generated.h"
#include "DysisGreybox.h"
#include "World/DysisWorldState.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "UI/DysisDialogueComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度），见 Tools/greybox/golden/greybox_roof.json ─────
	constexpr float RoofRingZ = 3030.0f;                 // 环道面
	constexpr float RoofIrisR = 1100.0f;                 // 光圈全开的半径 = 环道内沿
	constexpr float RoofCrownR1 = 1545.0f;               // 环道外沿
	constexpr float RoofIrisZ1 = 3028.0f;                // 叶片上沿
	constexpr float RoofOcR = 150.0f, RoofOcPass = 430.0f;   // 白天的小圆眼；人走到光柱顶时张到这么大
	constexpr float RoofHOc = 37.19043f;                 // 圆眼那一刻
	constexpr float RoofLand0 = 219.655f, RoofLand1 = 227.655f;      // 细桥落脚的那一块
	constexpr float RoofUpTop0 = 26.83313f, RoofUpTop1 = 41.83313f;  // 最高一级（宽平台）
	constexpr int32 RoofDnCount = 16;
	constexpr float RoofTopZEnd = 720.0f;                // 最高一级比环道高 7.2 m
	constexpr float RoofWallH = 260.0f;                  // 看不见的墙高 2.6 m
	const FVector RoofBridgeS(-346.331, -271.394, 0.0), RoofBridgeE(-813.948, -776.604, 0.0), RoofBridgeEnd(-824.137, -787.612, 0.0);
	constexpr float RoofBridgeTopZ = 3045.0f, RoofBridgeHalfW = 60.0f;
	constexpr float RoofDoorSwingDeg = -97.742f;         // 桥门从关到开转过的角度（UE 的 Yaw）
	const FVector RoofArmTop(1167.291, 616.306, 3185.0); // 浑天仪中心（踏步没升时；升起来跟着最高一级走）

	enum ERoofKind { RoofLand = 0, RoofUp = 1, RoofDn = 2, RoofFix = 3 };

	float RoofWrap360(float A) { A = FMath::Fmod(A, 360.0f); return A < 0.0f ? A + 360.0f : A; }
	void RoofMakeMovable(AActor* A)
	{
		if (!A) return;
		if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>()) C->SetMobility(EComponentMobility::Movable);
		TArray<AActor*> Kids;
		A->GetAttachedActors(Kids);
		for (AActor* K : Kids) RoofMakeMovable(K);
	}
}

// ───────────────────────── 搭起来 ─────────────────────────

UBoxComponent* ADysisDirector::MakeWall()
{
	UBoxComponent* Box = NewObject<UBoxComponent>(this);
	Box->SetupAttachment(RootComponent);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);   // 只挡人：不挡光、不挡镜头
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCanEverAffectNavigation(false);
	Box->SetHiddenInGame(true);
	Box->RegisterComponent();
	WallComponents.Add(Box);
	return Box;
}

void ADysisDirector::AddArcWall(FRoofPiece& P, float A0, float A1, float R0, float R1)
{
	FRoofWall W;
	W.Box = MakeWall();
	W.AzMid = (A0 + A1) * 0.5f; W.HalfDeg = (A1 - A0) * 0.5f; W.R0 = R0; W.R1 = R1;
	P.Walls.Add(W);
}

void ADysisDirector::PlaceWall(const FRoofWall& W, float ZBottom) const
{
	if (!W.Box) return;
	const float RMid = (W.R0 + W.R1) * 0.5f;
	// 一小段弧用一块直板顶着（每块最多 12°，弧和弦差几厘米）
	W.Box->SetBoxExtent(FVector((W.R1 - W.R0) * 0.5f, RMid * FMath::DegreesToRadians(W.HalfDeg) + 4.0f, RoofWallH * 0.5f));
	W.Box->SetWorldLocationAndRotation(DysisGB::PolarCm(W.AzMid, RMid, ZBottom + RoofWallH * 0.5f), FRotator(0.0f, W.AzMid, 0.0f));
}

void ADysisDirector::SetupRoof()
{
	RoofPieces.Reset();
	int32 FixN = 0;
	for (const FDysisRoofPieceSpec& S : GDysisRoofPieces)
	{
		FRoofPiece P;
		P.Spec = &S;
		P.Z = RoofRingZ;
		const bool bSliver = (S.A1 - S.A0) < 0.5f;   // 第 10 级在接缝边上拆出来的一丝，模型里并在大的那块里
		switch (S.Kind)
		{
		case RoofLand:
			P.Blade = Piece(TEXT("SM_Mech_IrisBlades_Land"));
			break;
		case RoofUp:
			P.Tread = Piece(*FString::Printf(TEXT("SM_Mech_RoofSteps_Up%02d"), S.K));
			P.Shaft = Piece(*FString::Printf(TEXT("SM_Mech_RoofSteps_Up%02d_Shaft"), S.K));
			P.Curb = Piece(*FString::Printf(TEXT("SM_Mech_RoofSteps_Up%02d_Curb"), S.K));
			P.Blade = Piece(*FString::Printf(TEXT("SM_Mech_IrisBlades_Up%02d"), S.K));
			if (S.K == 16) TopPieceIndex = RoofPieces.Num();
			break;
		case RoofDn:
			if (!bSliver)
			{
				P.Tread = Piece(*FString::Printf(TEXT("SM_Mech_RoofSteps_Dn%02d"), S.K));
				P.Curb = Piece(*FString::Printf(TEXT("SM_Mech_RoofSteps_Dn%02d_Curb"), S.K));
				P.Blade = Piece(*FString::Printf(TEXT("SM_Mech_IrisBlades_Dn%02d"), S.K));
			}
			break;
		default:
			P.Blade = Piece(*FString::Printf(TEXT("SM_Mech_IrisBlades_Fix%02d"), ++FixN));
			break;
		}
		if (AActor* T = P.Tread.Get()) { P.TreadBase = T->GetActorLocation(); RoofMakeMovable(T); }
		else if (S.Kind == RoofUp || (S.Kind == RoofDn && !bSliver)) UE_LOG(LogTemp, Warning, TEXT("Dysis 屋顶：找不到踏步部件（kind %d，第 %d 级）"), S.Kind, S.K);
		RoofMakeMovable(P.Shaft.Get());
		if (AActor* Cb = P.Curb.Get())
		{
			// 内沿的铜边：灰盒里只挡镜头（不挡光、不挡人），跟着踏步一起升降
			P.CurbBase = Cb->GetActorLocation();
			RoofMakeMovable(Cb);
			if (UStaticMeshComponent* C = Cb->FindComponentByClass<UStaticMeshComponent>())
			{
				C->bUseDefaultCollision = false;
				C->SetCollisionResponseToAllChannels(ECR_Ignore);
				C->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
			}
		}
		if (AActor* B = P.Blade.Get()) { P.BladeYaw = B->GetActorRotation().Yaw; RoofMakeMovable(B); }

		// 看不见的墙：内侧一道（细桥那一段让开）、外侧一道、有的逆时针那头还有个挡头
		if (!bSliver)
		{
			const float Outer = S.Outer > 0.0f ? S.Outer : RoofCrownR1;
			if (S.Gap0 > 0.0f)
			{
				AddArcWall(P, S.A0, S.Gap0, S.Inner - 30.0f, S.Inner + 5.0f);
				AddArcWall(P, S.Gap1, S.A1, S.Inner - 30.0f, S.Inner + 5.0f);
			}
			else AddArcWall(P, S.A0, S.A1, S.Inner - 30.0f, S.Inner + 5.0f);
			AddArcWall(P, S.A0, S.A1, Outer - 2.0f, Outer + 30.0f);
			if (S.EndAz > 0.0f)
			{
				const float HalfDeg = FMath::RadiansToDegrees(15.0f / S.EndR0);
				AddArcWall(P, S.EndAz - HalfDeg, S.EndAz + HalfDeg, S.EndR0, S.EndR1);
			}
		}
		RoofPieces.Add(MoveTemp(P));
	}

	// 最高一级尽头的栏杆（模型摆的是升到头的位置）和它后面看不见的挡板
	TopBar = Piece(TEXT("SM_Mech_RoofSteps_TopBar"));
	if (AActor* Bar = TopBar.Get()) { TopBarBase = Bar->GetActorLocation(); RoofMakeMovable(Bar); }
	{
		TopBarWall.Box = MakeWall();
		const float RMid = (RoofIrisR + RoofCrownR1) * 0.5f, HalfLen = (RoofCrownR1 - RoofIrisR + 40.0f) * 0.5f;
		TopBarWall.AzMid = RoofUpTop1 - 0.3f; TopBarWall.R0 = RMid - HalfLen; TopBarWall.R1 = RMid + HalfLen;
		TopBarWall.HalfDeg = FMath::RadiansToDegrees(16.0f / RMid);
	}
	// 细桥两边看不见的护栏（2.4 m 高，跳不过去）
	{
		const FVector Dir = (RoofBridgeE - RoofBridgeS).GetSafeNormal();
		const FVector SideV(-Dir.Y, Dir.X, 0.0);
		const float Len = FVector::Dist(RoofBridgeS, RoofBridgeE);
		for (const float Sign : { -1.0f, 1.0f })
		{
			UBoxComponent* Box = MakeWall();
			const FVector Mid = (RoofBridgeS + RoofBridgeE) * 0.5 + SideV * (Sign * (RoofBridgeHalfW - 3.0f));
			Box->SetBoxExtent(FVector(Len * 0.5f + 10.0f, 7.0f, 120.0f));
			Box->SetWorldLocationAndRotation(FVector(Mid.X, Mid.Y, RoofBridgeTopZ + 120.0f), Dir.Rotation());
		}
	}
	// 光圈的叶片：关卡里的那一份只留着挡光（藏起来，照旧按光圈的大小缩短）；另做一份只管看的，形状不动，
	// 用材质把光圈以内的部分剪掉——看上去叶片是收进去了，而不是被压扁了
	IrisMIDs.Reset();
	if (UMaterialInterface* Cut = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Night/M_DysisIrisBlade.M_DysisIrisBlade")))
	{
		TMap<UMaterialInterface*, UMaterialInstanceDynamic*> ByLook;
		for (FRoofPiece& P : RoofPieces)
		{
			AActor* B = P.Blade.Get();
			UStaticMeshComponent* Src = B ? B->FindComponentByClass<UStaticMeshComponent>() : nullptr;
			if (!Src || !Src->GetStaticMesh()) continue;
			UStaticMeshComponent* Look = NewObject<UStaticMeshComponent>(this);
			Look->SetupAttachment(RootComponent);
			Look->SetMobility(EComponentMobility::Movable);
			Look->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Look->SetStaticMesh(Src->GetStaticMesh());
			Look->RegisterComponent();
			Look->SetWorldTransform(Src->GetComponentTransform());
			for (int32 m = 0; m < Src->GetNumMaterials(); ++m)
			{
				UMaterialInterface* Orig = Src->GetMaterial(m);
				UMaterialInstanceDynamic*& MID = ByLook.FindOrAdd(Orig);
				if (!MID)
				{
					MID = UMaterialInstanceDynamic::Create(Cut, this);
					if (UMaterialInstance* Inst = Cast<UMaterialInstance>(Orig)) MID->CopyParameterOverrides(Inst);
					IrisMIDs.Add(MID);
				}
				Look->SetMaterial(m, MID);
			}
			P.BladeLook = Look;
			P.BladeLookDz = float(Src->GetComponentLocation().Z - B->GetActorLocation().Z);
			B->SetActorHiddenInGame(true);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Dysis 屋顶：找不到材质 /Game/Dysis/Night/M_DysisIrisBlade（跑一遍 Art/Night/ue_make_sky_materials.py），叶片先照旧缩短着显示"));
	}

	// 桥门
	DoorLeaf = Piece(TEXT("SM_Mech_RoofBridgeDoor_Leaf"));
	if (AActor* Door = DoorLeaf.Get()) { DoorClosedYaw = Door->GetActorRotation().Yaw; RoofMakeMovable(Door); }

	// 开局：踏步都落平（关卡里摆的是升到头的样子）
	CrownUp = 0.0f; bCrownDn = false;
	for (FRoofPiece& P : RoofPieces) PlaceRoofPiece(P);
	IrisShownCm = -1.0f;
	PlaceBlades();
}

void ADysisDirector::PlaceRoofPiece(FRoofPiece& P)
{
	const FDysisRoofPieceSpec& S = *P.Spec;
	if (AActor* T = P.Tread.Get())
	{
		// 升起来的踏步：模型摆在升到头的位置；夜里的楼梯：模型摆在环道面上
		const float BaseZ = S.Kind == RoofUp ? RoofRingZ + S.ZEnd : RoofRingZ;
		T->SetActorLocation(P.TreadBase + FVector(0.0, 0.0, P.Z - BaseZ));
	}
	if (AActor* Cb = P.Curb.Get())
	{
		const float BaseZ = S.Kind == RoofUp ? RoofRingZ + S.ZEnd : RoofRingZ;
		Cb->SetActorLocation(P.CurbBase + FVector(0.0, 0.0, P.Z - BaseZ));
	}
	if (AActor* Sh = P.Shaft.Get())
	{
		// 踏步下面的柱身：从天花板长到踏步底
		const float K = S.ZEnd > 0.0f ? (P.Z - RoofRingZ) / S.ZEnd : 0.0f;
		Sh->SetActorHiddenInGame(K < 0.002f);
		Sh->SetActorEnableCollision(K >= 0.002f);
		Sh->SetActorScale3D(FVector(1.0, 1.0, FMath::Max(K, 0.002f)));
	}
	for (const FRoofWall& W : P.Walls) PlaceWall(W, P.Z);
	if (TopPieceIndex >= 0 && &P == &RoofPieces[TopPieceIndex])
	{
		const float Dz = P.Z - (RoofRingZ + RoofTopZEnd);
		if (AActor* Bar = TopBar.Get()) Bar->SetActorLocation(TopBarBase + FVector(0.0, 0.0, Dz));
		PlaceWall(TopBarWall, P.Z);
	}
}

void ADysisDirector::PlaceBlades()
{
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const float A = State ? State->IrisACm : 40.0f;
	// 灰盒 irisOpenOf / bladeGeo：内缘从 0.4 m 退到环道内沿；这里把合拢的叶片沿半径朝外沿缩短到同样的内缘
	const float Open = FMath::Clamp((A - 40.0f) / (RoofIrisR - 2.0f - 40.0f), 0.0f, 1.0f);
	const float RIn = FMath::Lerp(40.0f, RoofIrisR - 2.0f, Open);
	const float Scale = FMath::Max((RoofIrisR - RIn) / (RoofIrisR - 40.0f), 0.002f);
	for (const FRoofPiece& P : RoofPieces)
	{
		AActor* B = P.Blade.Get();
		if (!B) continue;
		const float Az = P.BladeYaw + 270.0f;   // 叶片的 Yaw = 它所在方位 − 270°，本地 Y 轴朝殿心
		const float Z = P.Spec->Kind == RoofUp ? P.Z : RoofRingZ;   // 夜里的楼梯降下去时叶片留在天花板的圈上
		B->SetActorLocation(DysisGB::PolarCm(Az, RoofIrisR * (1.0f - Scale), Z));
		B->SetActorScale3D(FVector(1.0, Scale, 1.0));
		B->SetActorEnableCollision(Open < 0.995f);
		if (UStaticMeshComponent* Look = P.BladeLook.Get())
		{
			// 只管看的那一份：不缩，只跟着踏步升降
			FVector L = Look->GetComponentLocation();
			L.Z = Z + P.BladeLookDz;
			Look->SetWorldLocation(L);
			Look->SetVisibility(Open < 0.995f);
		}
		else B->SetActorHiddenInGame(Open >= 0.995f);
	}
	for (UMaterialInstanceDynamic* MID : IrisMIDs) if (MID) MID->SetScalarParameterValue(TEXT("Aperture"), RIn);
	IrisShownCm = A;
}

// ───────────────────────── 每帧 ─────────────────────────

void ADysisDirector::UpdateCrown(float Dt)
{
	const UDysisTimeComponent* Time = PlayerTime();
	const bool bStarted = GameStarted();
	const FString Zone = Time ? Time->Zone : FString();
	const FVector Foot = Time ? Time->FootCm : FVector::ZeroVector;

	// 主界面开着的时候：屋顶的踏步摆成升到头的样子（主界面的镜头就在屋顶上，升起来的那一圈台阶比落平的好看）；点了“开始游戏”再落平
	if (!bStarted && !bMenuPose) { bMenuPose = true; CrownUp = 1.0f; }
	else if (bStarted && bMenuPose) { bMenuPose = false; CrownUp = 0.0f; }

	// 人在环道上：离细桥落脚处顺时针走了多远，前面的踏步就升多少
	const float UpSpan = RoofWrap360(RoofUpTop0 - RoofLand1) + 3.0f;
	if (bStarted && Time && Time->IsOnGround() && Zone == TEXT("crown"))
	{
		const float Psi = RoofWrap360(DysisGB::AzOf(Foot) - RoofLand1);
		CrownUp = Psi < 240.0f ? DysisGB::Smoothstep(0.0f, UpSpan, Psi) : 0.0f;
	}
	// 入夜：回到桥头以后，另外半圈降成楼梯，一直留到天亮
	if (bCaught && CrownUp < 0.03f) bCrownDn = true;

	bool bMoved = false;
	for (FRoofPiece& P : RoofPieces)
	{
		const FDysisRoofPieceSpec& S = *P.Spec;
		float Z = P.Z;
		if (S.Kind == RoofUp) Z = RoofRingZ + S.ZEnd * CrownUp;
		else if (S.Kind == RoofDn)
			Z = DysisGB::Toward(P.Z, bCrownDn ? S.ZNight : RoofRingZ, (0.6f + 0.12f * (bCrownDn ? S.K : RoofDnCount + 1 - S.K)) * 100.0f, Dt);
		if (FMath::Abs(Z - P.Z) > 0.001f) { P.Z = Z; PlaceRoofPiece(P); bMoved = true; }
	}

	// 桥门：人从细桥走近桥尾时自己转开（横在环道上，挡住逆时针那一边）；接住最后一缕光以后转回来封住细桥
	if (bCaught) bDoorLocked = true;
	const bool bNear = (Zone == TEXT("rbridge") || Zone == TEXT("crown")) && FVector::Dist2D(Foot, RoofBridgeEnd) < 450.0f;
	const float Want = bDoorLocked ? 0.0f : ((bNear || DoorOpen > 0.5f) ? 1.0f : 0.0f);
	const float DoorBefore = DoorOpen;
	DoorOpen = DysisGB::Toward(DoorOpen, Want, 0.7f, Dt);
	if (DoorOpen != DoorBefore)
		if (AActor* Door = DoorLeaf.Get())
			Door->SetActorRotation(FRotator(0.0f, DoorClosedYaw + RoofDoorSwingDeg * DysisGB::Smoothstep(0.0f, 1.0f, DoorOpen), 0.0f));

	UpdateIris(Dt);
	if (bMoved || IrisShownCm < 0.0f)
	{
		PlaceBlades();
		if (UDysisWorldState* State = UDysisWorldState::Get(this)) State->BumpLight();
	}
}

void ADysisDirector::UpdateIris(float Dt)
{
	UDysisWorldState* State = UDysisWorldState::Get(this);
	if (!State) return;
	if (!State->IsIrisDebug())
	{
		const UDysisTimeComponent* Time = PlayerTime();
		const FString Zone = Time ? Time->Zone : FString();
		// 灰盒 irisTarget。白天：快到圆眼的时刻张成一只小眼，光柱从这里下来；人顺着光柱走上去、头快碰到叶片时张到 OC_PASS；
		// 上了环道往上走，张到全开。夜里：一直全开，整片天幕都打开。
		float Target = RoofIrisR;
		if (!bCaught)
		{
			const float H = Time ? Time->H : 0.0f;
			const float Eye = FMath::Lerp(40.0f, RoofOcR, DysisGB::Smoothstep(RoofHOc - 3.0f, RoofHOc - 0.3f, H));
			const float Pass = (Zone == TEXT("rbridge") || Zone == TEXT("crown")) ? 1.0f
				: Zone == TEXT("beam:oculus") ? DysisGB::Smoothstep(RoofIrisZ1 - 370.0f, RoofIrisZ1 - 220.0f, Time ? float(Time->FootCm.Z) : 0.0f) : 0.0f;
			Target = CrownUp > 0.001f ? FMath::Lerp(RoofOcPass, RoofIrisR, CrownUp) : FMath::Lerp(Eye, RoofOcPass, Pass);
		}
		const float Rate = bCaught ? 300.0f : (Zone == TEXT("beam:oculus") ? 110.0f : 600.0f);
		State->IrisACm = FMath::Abs(Target - State->IrisACm) < 1.0f ? Target : DysisGB::Toward(State->IrisACm, Target, Rate, Dt);
	}
	// 灰盒 setIris：变了 0.4 厘米以上才重摆叶片、重算光
	if (FMath::Abs(State->IrisACm - IrisShownCm) >= 0.4f)
	{
		PlaceBlades();
		State->BumpLight();
	}
}

// ───────────────────────── 接光、金苹果 ─────────────────────────

FVector ADysisDirector::ArmTopCm() const
{
	return RoofArmTop + FVector(0.0, 0.0, RoofTopZEnd * CrownUp);
}

void ADysisDirector::CatchLight()
{
	if (bCaught) return;
	UDysisTimeComponent* Time = PlayerTime();
	if (!Time) return;
	if (CrownUp < 0.98f) { ADysisHUD::Notify(GetWorld(), TEXT("台阶还没有完全升起来。"), 2.4f); return; }
	// 举起来的苹果（脚上方 1.95 m）还得在光里：岛影已经漫过去就接不到了
	const FVector Sun = UDysisSkyLibrary::DysisSunDir(Time->H);
	if (Sun.Z <= 0.0 || Time->FootCm.Z + 195.0 < UDysisWorldState::ShadowZ(Sun))
	{
		ADysisHUD::Notify(GetWorld(), TEXT("岛影已经漫过这里了。往回走几步，时间会倒回一点。"), 3.6f);
		return;
	}
	bCaught = true;
	CineSeconds = 0.001f;
	Time->SetNight(true);
	if (ADysisMusicManager* Music = ADysisMusicManager::GetDysisMusicManager(this)) Music->SwitchToNight();   // 白天的曲子淡出，换成夜里的
	ADysisHUD::Notify(GetWorld(), DysisCopy::AppleTaken, 5.0f);
}

void ADysisDirector::UpdateCatch(float Dt)
{
	if (!Apple) return;
	const UDysisTimeComponent* Time = PlayerTime();
	if (!bCaught)
	{
		// 金苹果是屋顶浑天仪上的“太阳”：挂在朝着太阳的那一侧
		const FVector Sun = UDysisSkyLibrary::DysisSunDir(Time ? Time->H : RoofHOc);
		Apple->SetWorldLocation(ArmTopCm() + Sun * 55.0);
		return;
	}
	const float Before = CineSeconds;
	CineSeconds += Dt;
	// 取下以后跟在人右手边（放上水亭的月托以后就留在那里了，见 UpdateFinale）
	if (const APlayerController* PC = (GetWorld() && !bApplePlaced) ? GetWorld()->GetFirstPlayerController() : nullptr)
		if (Time)
		{
			const FVector Right = FRotationMatrix(FRotator(0.0f, PC->GetControlRotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
			const FVector Held = Time->FootCm + Right * 35.0 + FVector(0.0, 0.0, 125.0);
			Apple->SetWorldLocation(FMath::Lerp(Apple->GetComponentLocation(), Held, FMath::Min(1.0f, Dt * 6.0f)));
		}
	// 5 秒后：入夜的标题和提示
	if (Before <= 5.0f && CineSeconds > 5.0f)
	{
		if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->ShowTitle(TEXT("日落之后"), TEXT("入夜"), 2.4f);
		// 狄西斯自己说的那一句：用剧情对话框（有她的立绘），不放在提示条里
		if (!NightDialogue)
		{
			NightDialogue = NewObject<UDysisDialogueComponent>(this);
			NightDialogue->RegisterComponent();
			NightDialogue->Lines = { FText::FromString(DysisCopy::NightHint) };
		}
		NightDialogue->Play();
	}
}

FString ADysisDirector::DescribeRoof() const
{
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	FString S = FString::Printf(TEXT("{\"up\":%.4f,\"dn\":%s,\"door\":%.3f,\"doorLocked\":%s,\"caught\":%s,\"iris\":%.2f,\"armTopZ\":%.1f,\"pieces\":["),
		CrownUp, bCrownDn ? TEXT("true") : TEXT("false"), DoorOpen, bDoorLocked ? TEXT("true") : TEXT("false"), bCaught ? TEXT("true") : TEXT("false"),
		State ? State->IrisACm : -1.0f, ArmTopCm().Z);
	for (int32 i = 0; i < RoofPieces.Num(); ++i)
	{
		const FRoofPiece& P = RoofPieces[i];
		const AActor* T = P.Tread.Get();
		S += FString::Printf(TEXT("%s{\"kind\":%d,\"k\":%d,\"z\":%.2f,\"treadZ\":%.2f,\"hasTread\":%s,\"hasBlade\":%s}"), i ? TEXT(",") : TEXT(""),
			P.Spec->Kind, P.Spec->K, P.Z, T ? T->GetActorLocation().Z : -1.0, T ? TEXT("true") : TEXT("false"), P.Blade.IsValid() ? TEXT("true") : TEXT("false"));
	}
	return S + TEXT("]}");
}
