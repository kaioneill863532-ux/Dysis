// 机关总管 · 建筑上的小修补（2026-10-10 第七轮试玩反馈）。都是在原模型上“添”，不改模型，也不动灰盒核对过的光路：
//   · 暗门和墙里楼梯的窗：方洞的两个上角补成半圆拱（只管看，不挡人不挡光）。
//     2026-10-10 第八轮：头一版是一圈小方块拼的、比墙面退进去 3 mm，用户看得出是补的，楼梯里有的地方还露出一截；
//     现在按每个洞口在低模上实量的尺寸现做网格：朝外的面就在墙面上、四边正好卡在洞口里，一丝不多。
//     暗门的门板、月之龛的盖板也照这个办法做成和墙面齐平的（原来那几块方石板比洞口大一圈、凸出墙面 1 cm）；
//   · 各层栏杆：光路从栏杆上越过去的那几段，灰盒只留了 0.95 m 的矮栏杆（高护栏拿掉了，人才能顺着光走过去）——
//     可人一跳有 1.3 m，从这几段能直接跳进中庭、掉到下面一层。补一道护栏：人踩在光 / 虹桥 / 夜里的楼梯上时它不在，别的时候都拦着；
//   · 岛边：岛缩小、挪到殿心以后，关卡里原来那一圈护栏还在老地方（在岛外面），新的岛边、台基的边上补一圈；
//   · 月桥口：栏杆在这里断开，原来拦着一根铜杆，改成和两边一样的一段石栏杆，双子并肩以后收进地里；
//   · 虹之龛前的石沿：改成和窗一样宽、前沿是弧形的（原来是 2.7 m 长的一条直石板，比窗长出一截）。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "Sky/DysisTimeComponent.h"
#include "Audio/DysisSfxSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	UMaterialInterface* DtlMat(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/%s.%s"), Name, Name);
		return LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// 各层回廊的地面；栏杆（灰盒 R_A 12.19 – R_PARA 12.49 m，高 0.95 m）
	constexpr float DtlFloorZ[4] = { 0.0f, 600.0f, 1450.0f, 2300.0f };
	constexpr float DtlParaR0 = 1219.0f, DtlParaR1 = 1249.0f, DtlParaH = 95.0f;

	// 没有高护栏的那几段（在游戏里一圈量出来的；对应灰盒的 PARA_OPEN：日1、日2 的光、虹桥的两头、光阶、三相像的镜光、圆眼的光、夜里的楼梯）。
	// 第几层、方位从哪到哪。三层 142–161.5° 那一段连矮栏杆都没有（虹的窗的光要从这里斜着下去，石头拿掉了）。
	struct FDtlArc { int32 Floor; float A0, A1; };
	const FDtlArc DtlOpenArcs[] = {
		{ 1, 85.5f, 104.5f }, { 1, 141.0f, 156.5f }, { 1, 334.5f, 344.5f },
		{ 2, 72.5f, 86.0f }, { 2, 142.0f, 161.5f }, { 2, 257.0f, 270.5f }, { 2, 281.0f, 284.0f },
		{ 3, 30.5f, 46.0f }, { 3, 111.0f, 115.0f }, { 3, 198.5f, 207.5f },
	};
	constexpr float DtlGuardTopCm = 500.0f;      // 护栏从地面往上多高（下一层的石台、楼板都在这以上）

	// 这些数都是照着“画出来的那层低模墙”量的（碰撞用的原模型和它在门洞边上不完全一样：把低模临时变成能被射线打到的，按三角面量，洞边用二分法逼到万分之一度）。
	// 暗门（开在内墙里，墙厚 30 cm）。A0–A1：回廊这一面洞口两边的方位；T0–T1：通到楼梯的那个洞——比洞口窄，靠楼梯尽头那一侧有一条 9.5 cm 的门框槽
	// （槽从回廊的墙面一直凹到半径 DtlRebateR，比楼梯那一面的墙皮还深 5 cm）；TopZ：洞顶；FaceR / BackR：回廊那一面、楼梯那一面的墙面各在多大的半径上；
	// Leaf：堵着这个洞的门板是哪个部件（月亮浮雕那一处是美术做的月门，不在这里做）。门洞都是 2.4 m 高。
	struct FDtlDoor { float A0, A1, T0, T1, TopZ, FaceR, BackR; const TCHAR* Leaf; };
	const FDtlDoor DtlDoors[] = {
		{ 109.6203f, 114.3797f, 110.0000f, 114.3797f,  840.0f, 1550.00f, 1579.93f, TEXT("SM_Mech_StairSeal_TR") },   // 楼梯 TR 的下口（二层）
		{ 176.3255f, 181.0849f, 176.3255f, 180.7052f, 1690.0f, 1549.95f, 1579.91f, nullptr },                        // TR 的上口（三层，月门）
		{ 319.3483f, 324.1077f, 319.3483f, 323.7280f, 1690.0f, 1549.95f, 1579.96f, TEXT("SM_Mech_StairSeal_TS") },   // 楼梯 TS 的下口（三层）
		{ 257.3483f, 262.1077f, 257.7280f, 262.1077f, 2540.0f, 1549.95f, 1579.93f, TEXT("SM_Mech_Swan_Door") },      // TS 的上口（四层，天鹅浮雕后面）
	};
	constexpr float DtlRebateR = 1584.95f, DtlDoorH = 240.0f;
	// 楼梯的窗（开在外墙里）。窗在外立面的盲拱里：楼梯这一面的墙是圆的（半径 DtlWinInR），外面那一面是盲拱的底——一块平的面，正对着窗的地方离殿心 DtlWinOutD。
	// 最后一扇两面的洞顶不一样高（里面 18.6 m、外面 18.45 m）。
	struct FDtlWindow { float A0, A1, TopIn, TopOut; };
	const FDtlWindow DtlWindows[] = {
		{ 125.8099f, 129.1901f, 985.0f, 985.0f }, { 140.8099f, 144.1901f, 1223.333f, 1223.333f },
		{ 275.8099f, 279.1901f, 2185.0f, 2185.0f }, { 290.8099f, 294.1901f, 2016.667f, 2016.667f }, { 305.8099f, 309.1901f, 1860.0f, 1845.0f } };
	constexpr float DtlWinInR = 1679.96f, DtlWinOutD = 1695.37f;
	// 月之龛的盖板（一层墙上，原来是一块 0.92 × 0.9 m 的平石板）：方位、宽、高
	constexpr float DtlLidAz = 6.577f, DtlLidHalfDeg = 1.70f, DtlLidZ0 = 1604.7f, DtlLidZ1 = 1694.7f, DtlLidProud = 0.4f;

	// 月桥口（栏杆断开的那一段）
	constexpr float DtlGapA0 = 253.8f, DtlGapA1 = 259.738f;

	// 岛边、台基边（和 DysisDirectorScenery.cpp 里的数一致：岛半径 32 m；台基正面离殿心 32.05 m、两侧 ±17.75 m；柱廊朝 22.5°）
	constexpr float DtlIsleR = 3188.0f, DtlPorticoAz = 22.5f, DtlBaseD = 3215.0f, DtlBaseS = 1785.0f;
	constexpr float DtlLaneHalfDeg = 5.0f;       // 开场那道光落上来的地方：柱廊轴线两边各 5° 不拦

	// ── 现做的网格 ──
	struct FDtlMesh
	{
		TArray<FVector> V, N;
		TArray<int32> I;
		// 一个四边形（A→B→C→D 绕一圈）和四个角的法线。引擎里“正面”是按顶点顺序算出来的叉积的反方向：照给的法线定顺序
		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& NA, const FVector& NB, const FVector& NC, const FVector& ND)
		{
			const int32 k = V.Num();
			V.Append({ A, B, C, D }); N.Append({ NA, NB, NC, ND });
			const FVector Face = FVector::CrossProduct(B - A, C - A) + FVector::CrossProduct(C - A, D - A);
			if (FVector::DotProduct(Face, NA + NB + NC + ND) > 0.0) I.Append({ k, k + 2, k + 1, k, k + 3, k + 2 });
			else I.Append({ k, k + 1, k + 2, k, k + 2, k + 3 });
		}
	};
	FVector DtlRad(float Az) { return DysisGB::PolarCm(Az, 1.0, 0.0); }                          // 从殿心朝外
	FVector DtlTan(float Az) { const FVector O = DtlRad(Az); return FVector(-O.Y, O.X, 0.0); }   // 方位增大的方向

	// 墙上的一个拱洞：在半径 RefR 的墙面上，洞口从方位 A0 到 A1、洞顶 TopZ；拱是这个面上的半圆（半径 = 半个洞宽，拱顶正好顶到洞顶）。
	struct FDtlArch
	{
		float Mid, HalfDeg, Hw, Zc;
		FDtlArch(float A0, float A1, float TopZ, float RefR)
			: Mid((A0 + A1) * 0.5f), HalfDeg((A1 - A0) * 0.5f), Hw(FMath::DegreesToRadians((A1 - A0) * 0.5f) * RefR), Zc(TopZ - FMath::DegreesToRadians((A1 - A0) * 0.5f) * RefR) {}
		float U(float Az) const { return FMath::Clamp((Az - Mid) / HalfDeg, -1.0f, 1.0f); }
		float ArcZ(float Az) const { const float u = U(Az); return Zc + Hw * FMath::Sqrt(FMath::Max(0.0f, 1.0f - u * u)); }
		// 拱面上这一点朝洞里的方向
		FVector In(float Az) const { const float u = U(Az); return -(DtlTan(Az) * u + FVector(0.0, 0.0, FMath::Sqrt(FMath::Max(0.0f, 1.0f - u * u)))); }
		// 从 From 到 To 取样：拱的两头（陡的地方）密、中间疏
		TArray<float> Samples(float From, float To, int32 Segs) const
		{
			TArray<float> S;
			S.Add(From);
			for (int32 i = 1; i < Segs; ++i)
			{
				const float Az = Mid - HalfDeg * FMath::Cos(PI * i / Segs);
				if (Az > From + 1.0e-4f && Az < To - 1.0e-4f) S.Add(Az);
			}
			S.Add(To);
			return S;
		}
	};
	enum : int32 { DtlFaceIn = 1, DtlFaceOut = 2, DtlFaceLo = 4, DtlFaceHi = 8, DtlFaceUnder = 16 };
	// 拱上面的那一块石头（洞口的上角到拱之间）：方位 From–To，从半径 R0 到 R1（都可以随方位变：平的面离殿心的距离就是变的），高从拱面到 FillTop。
	// Faces：做哪几个面——朝殿心的、朝外的、方位小的那一头、方位大的那一头、底下的拱面。贴着洞壁的面不用做。OutN：朝外的那个面是平的话，给它的法线。
	void DtlSpandrel(FDtlMesh& M, const FDtlArch& Arch, float From, float To, TFunctionRef<float(float)> R0, TFunctionRef<float(float)> R1, float FillTop, int32 Faces, const FVector* OutN = nullptr, int32 Segs = 28)
	{
		const TArray<float> S = Arch.Samples(From, To, Segs);
		for (int32 i = 0; i + 1 < S.Num(); ++i)
		{
			const float a = S[i], b = S[i + 1], za = Arch.ArcZ(a), zb = Arch.ArcZ(b);
			const FVector Na = DtlRad(a), Nb = DtlRad(b);
			if (Faces & DtlFaceIn)
				M.Quad(DysisGB::PolarCm(a, R0(a), za), DysisGB::PolarCm(b, R0(b), zb), DysisGB::PolarCm(b, R0(b), FillTop), DysisGB::PolarCm(a, R0(a), FillTop), -Na, -Nb, -Nb, -Na);
			if (Faces & DtlFaceOut)
				M.Quad(DysisGB::PolarCm(a, R1(a), za), DysisGB::PolarCm(b, R1(b), zb), DysisGB::PolarCm(b, R1(b), FillTop), DysisGB::PolarCm(a, R1(a), FillTop),
					OutN ? *OutN : Na, OutN ? *OutN : Nb, OutN ? *OutN : Nb, OutN ? *OutN : Na);
			if (Faces & DtlFaceUnder)
				M.Quad(DysisGB::PolarCm(a, R0(a), za), DysisGB::PolarCm(b, R0(b), zb), DysisGB::PolarCm(b, R1(b), zb), DysisGB::PolarCm(a, R1(a), za), Arch.In(a), Arch.In(b), Arch.In(b), Arch.In(a));
		}
		for (int32 Side = 0; Side < 2; ++Side)
			if (Faces & (Side ? DtlFaceHi : DtlFaceLo))
			{
				const float a = Side ? To : From, za = Arch.ArcZ(a);
				const FVector Nn = DtlTan(a) * (Side ? 1.0 : -1.0);
				M.Quad(DysisGB::PolarCm(a, R0(a), za), DysisGB::PolarCm(a, R1(a), za), DysisGB::PolarCm(a, R1(a), FillTop), DysisGB::PolarCm(a, R0(a), FillTop), Nn, Nn, Nn, Nn);
			}
	}
	// 堵着拱洞的门板：从地面到拱面，两面各在一个半径上；顶上那道拱形的边也做（门往下沉的时候看得见）
	void DtlLeaf(FDtlMesh& M, const FDtlArch& Arch, float From, float To, float R0, float R1, float FloorZ, int32 Segs = 28)
	{
		const TArray<float> S = Arch.Samples(From, To, Segs);
		for (int32 i = 0; i + 1 < S.Num(); ++i)
		{
			const float a = S[i], b = S[i + 1], za = Arch.ArcZ(a), zb = Arch.ArcZ(b);
			const FVector Na = DtlRad(a), Nb = DtlRad(b);
			M.Quad(DysisGB::PolarCm(a, R0, FloorZ), DysisGB::PolarCm(b, R0, FloorZ), DysisGB::PolarCm(b, R0, zb), DysisGB::PolarCm(a, R0, za), -Na, -Nb, -Nb, -Na);
			M.Quad(DysisGB::PolarCm(a, R1, FloorZ), DysisGB::PolarCm(b, R1, FloorZ), DysisGB::PolarCm(b, R1, zb), DysisGB::PolarCm(a, R1, za), Na, Nb, Nb, Na);
			M.Quad(DysisGB::PolarCm(a, R0, za), DysisGB::PolarCm(b, R0, zb), DysisGB::PolarCm(b, R1, zb), DysisGB::PolarCm(a, R1, za), -Arch.In(a), -Arch.In(b), -Arch.In(b), -Arch.In(a));
		}
	}

	// 虹之龛前的石沿。窗洞在墙面上的口子（低模）是方位 162.65–169.30°：中线 166°、宽 1.8 m（西边那一侧是斜着收进去的窗套）。
	// 石沿和这个口子一样宽；从墙面 15.5 m 探出来 0.64 m，顶面高 17.5 m、厚 0.35 m
	constexpr float DtlSillAz = 166.0f, DtlSillHalfW = 90.0f, DtlSillDepth = 64.0f, DtlSillEndDepth = 34.0f, DtlSillZ0 = 1715.0f, DtlSillZ1 = 1750.0f;
}

void ADysisDirector::SetupDetails()
{
	for (UStaticMeshComponent* C : DetailComps) if (C) C->DestroyComponent();
	for (UProceduralMeshComponent* C : DetailMeshes) if (C) C->DestroyComponent();
	DetailComps.Reset(); DetailMeshes.Reset(); GapSlab.Reset(); GapSlabT = 0.0f; DetailLeaves = 0;
	for (const FJumpGuard& G : JumpGuards) if (UBoxComponent* B = G.Box.Get()) B->DestroyComponent();
	JumpGuards.Reset();
	DetailArches = DetailEdgeWalls = DetailLedgeParts = 0;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Warm = DtlMat(TEXT("MI_MATCH_WarmWhite"));
	if (!Cube) return;

	// 一块只管看的石头（方块）。Parent 是别的 Actor 的部件时，它就算那个 Actor 的：跟着它挪、跟着它藏
	auto Block = [&](USceneComponent* Parent, const FVector& Center, const FQuat& Rot, const FVector& SizeCm, UMaterialInterface* M, bool bShadow)
	{
		UObject* Outer = Parent && Parent->GetOwner() ? static_cast<UObject*>(Parent->GetOwner()) : static_cast<UObject*>(this);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Outer);
		C->SetupAttachment(Parent ? Parent : RootComponent.Get());
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Cube);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(bShadow);
		C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);          // 还没登记：直接写（SetCollisionEnabled 在“Actor 整体关着碰撞”时不可靠，见下面石沿那一段）
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(Center, Rot);
		C->SetWorldScale3D(SizeCm / 100.0);
		if (M) C->SetMaterial(0, M);
		DetailComps.Add(C);
		return C;
	};

	// ── 多余的两根铜杆 ──
	// 二层拉杆旁边那根竖着的（灰盒里是“拉杆通到上面的链子”，从二层一直伸到三层暗门旁边；用户：像是多余的金属杆）——不画了。
	// 只是不画：它的碰撞留着。灰盒里它挡一点光（日1 那束光有四个时刻被它遮掉二十分之一），关掉的话光路就和灰盒对不上了。
	if (AActor* Chain = Piece(TEXT("SM_Mech_LeverA_Chain"))) Chain->SetActorHiddenInGame(true);
	// 月桥口那根横着的：模型不画了（换成下面的石栏杆）。Actor 本身的显隐还照原来的走（合拢以后藏起来），别处靠它判断“链子收了没有”
	if (AActor* Chain = TwinChain.Get())
		if (UStaticMeshComponent* C = Chain->FindComponentByClass<UStaticMeshComponent>()) C->SetVisibility(false);
	// 水闸位置上那根标记用的小柱子一直是引擎默认的灰格子：换成和拉杆一样的铜色
	if (SluiceMarker)
		if (const AActor* Arm = LeverArm.Get())
			if (const UStaticMeshComponent* C = Arm->FindComponentByClass<UStaticMeshComponent>())
				if (UMaterialInterface* Bronze = C->GetMaterial(0)) SluiceMarker->SetMaterial(0, Bronze);

	// ── 拱：方洞的两个上角补成半圆（按洞口实量的尺寸现做，面就在墙面上） ──
	// 一块现做的石头。Parent 是别的 Actor 的部件时它就算那个 Actor 的（跟着挪、跟着藏）；顶点是按世界坐标算好的，Shift = 这个 Actor 现在离它“关着的位置”多远
	auto Solid = [&](USceneComponent* Parent, const FDtlMesh& M, const FVector& Shift) -> UProceduralMeshComponent*
	{
		if (M.I.Num() == 0) return nullptr;
		UObject* Outer = Parent && Parent->GetOwner() ? static_cast<UObject*>(Parent->GetOwner()) : static_cast<UObject*>(this);
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(Outer);
		C->SetupAttachment(Parent ? Parent : RootComponent.Get());
		C->SetMobility(EComponentMobility::Movable);
		C->bUseAsyncCooking = false;
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(true);
		C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(Shift, FQuat::Identity);
		C->SetWorldScale3D(FVector::OneVector);
		C->CreateMeshSection_LinearColor(0, M.V, M.I, M.N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (Warm) C->SetMaterial(0, Warm);
		DetailMeshes.Add(C);
		return C;
	};
	// 一块机关石板原来的样子不画了（它的碰撞、挡光、会动的逻辑照旧），返回它现在离“关着的位置”多远
	auto HideSlab = [this](AActor* A) -> FVector
	{
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps) C->SetVisibility(false);
		for (const FStairSet& Set : StairSets) if (Set.Seal.Get() == A) return A->GetActorLocation() - Set.SealBase;
		if (SwanDoor.Get() == A) return A->GetActorLocation() - SwanDoorBase;
		return FVector::ZeroVector;
	};
	for (const FDtlDoor& D : DtlDoors)
	{
		const FDtlArch Arch(D.A0, D.A1, D.TopZ, D.FaceR);
		auto Face = [&D](float) { return D.FaceR; };
		auto Back = [&D](float) { return D.BackR; };
		auto Rebate = [](float) { return DtlRebateR; };
		FDtlMesh M;
		// 通到楼梯的那一段：回廊面、楼梯面、拱面
		DtlSpandrel(M, Arch, D.T0, D.T1, Face, Back, D.TopZ, DtlFaceIn | DtlFaceOut | DtlFaceUnder);
		// 门框槽那一条：一直填到槽底；朝着楼梯间的那一小条侧面也做
		if (D.T0 > D.A0 + 0.001f) DtlSpandrel(M, Arch, D.A0, D.T0, Face, Rebate, D.TopZ, DtlFaceIn | DtlFaceUnder | DtlFaceHi);
		if (D.T1 < D.A1 - 0.001f) DtlSpandrel(M, Arch, D.T1, D.A1, Face, Rebate, D.TopZ, DtlFaceIn | DtlFaceUnder | DtlFaceLo);
		if (Solid(nullptr, M, FVector::ZeroVector)) ++DetailArches;
		// 门板：照着拱洞做，两面都和墙面齐平（原来那块方石板比洞口大一圈、凸出墙面 1 cm，看着像墙上鼓出来一块）
		if (AActor* Door = D.Leaf ? Piece(D.Leaf) : nullptr)
		{
			const FVector Shift = HideSlab(Door);
			FDtlMesh L;
			DtlLeaf(L, Arch, D.A0, D.A1, D.FaceR, D.BackR, D.TopZ - DtlDoorH);
			if (Solid(Door->GetRootComponent(), L, Shift)) ++DetailLeaves;
		}
	}
	for (const FDtlWindow& W : DtlWindows)
	{
		const FDtlArch Arch(W.A0, W.A1, FMath::Min(W.TopIn, W.TopOut), DtlWinInR);
		const float Mid = Arch.Mid;
		const FVector OutN = DtlRad(Mid);
		auto In = [](float) { return DtlWinInR; };
		auto Out = [Mid](float Az) { return DtlWinOutD / FMath::Cos(FMath::DegreesToRadians(Az - Mid)); };
		FDtlMesh M;
		DtlSpandrel(M, Arch, W.A0, W.A1, In, Out, FMath::Max(W.TopIn, W.TopOut), DtlFaceIn | DtlFaceOut | DtlFaceUnder, &OutN);
		if (Solid(nullptr, M, FVector::ZeroVector)) ++DetailArches;
	}
	// 月之龛的盖板：一片顺着墙弯的薄石板，只比墙面凸出 4 mm（原来那块是平的：墙是弯的，平板中间就比两边多鼓出来 7 mm，再加上凸出的 1 cm）
	if (AActor* Lid = Piece(TEXT("SM_Mech_MoonShrine_Block")))
	{
		HideSlab(Lid);
		FDtlMesh M;
		const float R = DysisGB::R_IN - DtlLidProud, A0 = DtlLidAz - DtlLidHalfDeg, A1 = DtlLidAz + DtlLidHalfDeg;
		const int32 N = 8;
		for (int32 i = 0; i < N; ++i)
		{
			const float a = A0 + (A1 - A0) * i / N, b = A0 + (A1 - A0) * (i + 1) / N;
			M.Quad(DysisGB::PolarCm(a, R, DtlLidZ0), DysisGB::PolarCm(b, R, DtlLidZ0), DysisGB::PolarCm(b, R, DtlLidZ1), DysisGB::PolarCm(a, R, DtlLidZ1), -DtlRad(a), -DtlRad(b), -DtlRad(b), -DtlRad(a));
		}
		// 四条边（4 mm 厚）
		M.Quad(DysisGB::PolarCm(A0, R, DtlLidZ0), DysisGB::PolarCm(A0, DysisGB::R_IN, DtlLidZ0), DysisGB::PolarCm(A0, DysisGB::R_IN, DtlLidZ1), DysisGB::PolarCm(A0, R, DtlLidZ1), -DtlTan(A0), -DtlTan(A0), -DtlTan(A0), -DtlTan(A0));
		M.Quad(DysisGB::PolarCm(A1, R, DtlLidZ0), DysisGB::PolarCm(A1, DysisGB::R_IN, DtlLidZ0), DysisGB::PolarCm(A1, DysisGB::R_IN, DtlLidZ1), DysisGB::PolarCm(A1, R, DtlLidZ1), DtlTan(A1), DtlTan(A1), DtlTan(A1), DtlTan(A1));
		const FVector Up(0.0, 0.0, 1.0);
		M.Quad(DysisGB::PolarCm(A0, R, DtlLidZ1), DysisGB::PolarCm(A1, R, DtlLidZ1), DysisGB::PolarCm(A1, DysisGB::R_IN, DtlLidZ1), DysisGB::PolarCm(A0, DysisGB::R_IN, DtlLidZ1), Up, Up, Up, Up);
		M.Quad(DysisGB::PolarCm(A0, R, DtlLidZ0), DysisGB::PolarCm(A1, R, DtlLidZ0), DysisGB::PolarCm(A1, DysisGB::R_IN, DtlLidZ0), DysisGB::PolarCm(A0, DysisGB::R_IN, DtlLidZ0), -Up, -Up, -Up, -Up);
		if (Solid(Lid->GetRootComponent(), M, FVector::ZeroVector)) ++DetailLeaves;
	}

	// ── 栏杆上的护栏（跳不出去） ──
	for (const FDtlArc& Arc : DtlOpenArcs)
	{
		const float A0 = Arc.A0 - 0.5f, A1 = Arc.A1 + 0.5f, R = (DtlParaR0 + DtlParaR1) * 0.5f;      // 两头各多半度，和关卡里原来的护栏叠上
		const int32 N = FMath::Max(1, FMath::CeilToInt((A1 - A0) / 2.5f));
		const float Step = (A1 - A0) / N, Z0 = DtlFloorZ[Arc.Floor] + 5.0f, Z1 = DtlFloorZ[Arc.Floor] + DtlGuardTopCm;
		for (int32 i = 0; i < N; ++i)
		{
			UBoxComponent* Box = MakeWall();
			if (!Box) continue;
			const float Mid = A0 + (i + 0.5f) * Step;
			Box->SetBoxExtent(FVector((DtlParaR1 - DtlParaR0) * 0.5f, R * FMath::Tan(FMath::DegreesToRadians(Step * 0.5f)) + 1.0f, (Z1 - Z0) * 0.5f));
			Box->SetWorldLocationAndRotation(DysisGB::PolarCm(Mid, R, (Z0 + Z1) * 0.5f), FRotator(0.0f, Mid, 0.0f));
			Box->ComponentTags.Add(TEXT("DysisNoTrap"));          // 人从光上下来、正好站在它里面的时候不把人卡住
			FJumpGuard G; G.Box = Box; G.Floor = Arc.Floor;
			JumpGuards.Add(G);
		}
	}

	// ── 岛边、台基边的护栏（只挡人；高出地面 4.5 m，踩着石头也跳不过去） ──
	{
		const double Ax = FMath::DegreesToRadians(double(DtlPorticoAz));
		const FVector U(FMath::Cos(Ax), FMath::Sin(Ax), 0.0), V(-FMath::Sin(Ax), FMath::Cos(Ax), 0.0);
		auto Edge = [&](const FVector& A, const FVector& B)
		{
			UBoxComponent* Box = MakeWall();
			if (!Box) return;
			const FVector Dir = (B - A).GetSafeNormal2D();
			Box->SetBoxExtent(FVector(FVector::Dist2D(A, B) * 0.5 + 3.0, 14.0, 300.0));
			Box->SetWorldLocationAndRotation(FVector((A.X + B.X) * 0.5, (A.Y + B.Y) * 0.5, 30.0), FRotator(0.0f, float(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X))), 0.0f));
			++DetailEdgeWalls;
		};
		// 岛是圆的；台基那一块（柱廊轴线两边各 34°）比岛宽、比岛靠前，那一段跟着台基的边走
		const float Half = FMath::RadiansToDegrees(FMath::Asin(DtlBaseS / DtlIsleR));
		const float From = DtlPorticoAz + Half, To = DtlPorticoAz - Half + 360.0f;
		const int32 N = FMath::CeilToInt((To - From) / 3.0f);
		for (int32 i = 0; i < N; ++i)
			Edge(DysisGB::PolarCm(From + (To - From) * i / N, DtlIsleR, 0.0), DysisGB::PolarCm(From + (To - From) * (i + 1) / N, DtlIsleR, 0.0));
		const float SideD = FMath::Sqrt(DtlIsleR * DtlIsleR - DtlBaseS * DtlBaseS), Lane = DtlBaseD * FMath::Tan(FMath::DegreesToRadians(DtlLaneHalfDeg));
		for (const float Side : { -1.0f, 1.0f })
		{
			Edge(U * SideD + V * (Side * DtlBaseS), U * DtlBaseD + V * (Side * DtlBaseS));          // 台基的侧边
			Edge(U * DtlBaseD + V * (Side * DtlBaseS), U * DtlBaseD + V * (Side * Lane));           // 台基的正面：中间留着光落上来的口子
		}
	}

	// ── 月桥口：补一段和两边一样的石栏杆 ──
	{
		const int32 N = 3;
		const float A0 = DtlGapA0 - 0.03f, A1 = DtlGapA1 + 0.03f, Step = (A1 - A0) / N, R = (DtlParaR0 + DtlParaR1) * 0.5f;
		for (int32 i = 0; i < N; ++i)
		{
			const float Mid = A0 + (i + 0.5f) * Step;
			UStaticMeshComponent* C = Block(nullptr, DysisGB::PolarCm(Mid, R, DtlFloorZ[1] + DtlParaH * 0.5f), FRotator(0.0f, Mid, 0.0f).Quaternion(),
				FVector(DtlParaR1 - DtlParaR0, 2.0f * DtlParaR1 * FMath::Tan(FMath::DegreesToRadians(Step * 0.5f)) + 0.4f, DtlParaH), Warm, true);
			GapSlab.Add(C);
		}
	}

	// ── 虹之龛前的石沿：和窗一样宽，前沿是一道弧 ──
	// 原来的模型（一条 2.7 m 的直石板，往西比窗长出 1 m）不画、不挡了；换上的这几块挂在它身上：一起从墙里伸出来、一起能踩、区域照旧算“窗台”。
	// 形状：贴墙那一边和窗一样宽（1.8 m），中间探出来 0.64 m（和原来一样，虹桥的桥尾、棱镜都落在这上面），两头收到 0.34 m，前沿是半径 1.5 m 的一段圆弧。
	if (AActor* Ledge = Piece(TEXT("SM_Mech_Sill_Ledge")))
		if (UStaticMeshComponent* Orig = Ledge->FindComponentByClass<UStaticMeshComponent>())
		{
			// 石沿这个 Actor 开局是“整体关着碰撞”的（伸到头才打开）。在这种状态下动它身上组件的碰撞有两个坑：
			//   · SetCollisionEnabled 先问“现在是不是已经这样了”，这一问把 Actor 的总开关也算进去——想关原来那条石沿时它以为已经关了、什么都不做，
			//     等石沿伸出来 Actor 一开，原来那条长石沿看不见了却还能踩；
			//   · 新挂上去的组件在这时候登记，引擎不给它建物理体，以后 Actor 打开了它也不挡人（新石沿看得见、踩上去却掉下去）。
			// 所以这一段先把 Actor 的碰撞临时打开，照正常的办法设好，最后再恢复原样。
			const bool bWasSolid = Ledge->GetActorEnableCollision();
			Ledge->SetActorEnableCollision(true);
			const ECollisionChannel Type = Orig->GetCollisionObjectType();
			const FCollisionResponseContainer Resp = Orig->GetCollisionResponseToChannels();
			Orig->SetVisibility(false);
			Orig->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			const FVector Out = DysisGB::PolarCm(DtlSillAz, 1.0, 0.0), Tan(-Out.Y, Out.X, 0.0);
			const float Rho = (DtlSillHalfW * DtlSillHalfW + FMath::Square(DtlSillDepth - DtlSillEndDepth)) / (2.0f * (DtlSillDepth - DtlSillEndDepth));   // 前沿那道弧的半径
			const float WallR = DysisGB::R_IN, CenterY = WallR - DtlSillDepth + Rho;                 // 弧心离殿心多远（在墙里面）
			const float PhiMax = FMath::Asin(DtlSillHalfW / Rho), Zm = (DtlSillZ0 + DtlSillZ1) * 0.5f, Th = DtlSillZ1 - DtlSillZ0;
			const FVector Shift = SillOffset;                                                         // 现在它缩在墙里：照着伸出来以后的位置摆，再跟着它挪回去
			TArray<UStaticMeshComponent*> Made;
			const int32 N = 10;
			const float In = CenterY - WallR - 6.0f;                                                  // 每一块从墙面里面 6 cm 起，到弧上
			for (int32 i = 0; i < N; ++i)
			{
				const float Phi = -PhiMax + (i + 0.5f) * (2.0f * PhiMax / N), Dp = 2.0f * PhiMax / N;
				const FVector Dir = Tan * FMath::Sin(Phi) - Out * FMath::Cos(Phi);                    // 从弧心朝中庭
				const FVector C0 = Out * CenterY + Dir * ((In + Rho) * 0.5f) + FVector(0.0, 0.0, Zm) + Shift;
				Made.Add(Block(Orig, C0, FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat(), FVector(Rho - In, 2.0f * Rho * FMath::Tan(Dp * 0.5f) + 0.4f, Th), Warm, true));
			}
			// 两头贴墙的那两个角（上面那一把扇形在这里是斜着收进墙里的）
			const float EndX = In * FMath::Tan(PhiMax);                                               // 扇形在墙面里面 6 cm 那条线上有多宽（半宽）
			for (const float Side : { -1.0f, 1.0f })
			{
				const float X0 = FMath::Min(EndX, DtlSillHalfW - 4.0f), X1 = DtlSillHalfW, Y0 = WallR - DtlSillEndDepth, Y1 = WallR + 6.0f;
				Made.Add(Block(Orig, Out * ((Y0 + Y1) * 0.5f) + Tan * (Side * (X0 + X1) * 0.5f) + FVector(0.0, 0.0, Zm) + Shift, FRotator(0.0f, DtlSillAz, 0.0f).Quaternion(),
					FVector(Y1 - Y0, X1 - X0, Th), Warm, true));
			}
			for (UStaticMeshComponent* C : Made)
			{
				C->SetCollisionObjectType(Type);
				C->SetCollisionResponseToChannels(Resp);
				C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
			Ledge->SetActorEnableCollision(bWasSolid);
			DetailLedgeParts = Made.Num();
		}

	UpdateDetails(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Dysis 小修补：拱 %d 个、照洞口做的门板和盖板 %d 块、栏杆上的护栏 %d 段、岛边护栏 %d 段、石沿 %d 块"), DetailArches, DetailLeaves, JumpGuards.Num(), DetailEdgeWalls, DetailLedgeParts);
}

void ADysisDirector::UpdateDetails(float Dt)
{
	// 栏杆上的护栏：人踩在光上、虹桥上、夜里的楼梯上（区域是 beam:… / rainbow / crown）时不在——这几样本来就是从栏杆上面越过去的；
	// 别的时候（站在回廊上、窗台上、石台上，或者从这些地方跳起来——跳在空中时区域不变）都拦着。
	const UDysisTimeComponent* Time = PlayerTime();
	const FString Zone = Time ? Time->Zone : FString();
	const bool bPass = Zone.StartsWith(TEXT("beam:")) || Zone == TEXT("rainbow") || Zone == TEXT("crown");
	const ECollisionEnabled::Type Want = bPass ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics;
	for (const FJumpGuard& G : JumpGuards)
		if (UBoxComponent* B = G.Box.Get())
			if (B->GetCollisionEnabled() != Want) B->SetCollisionEnabled(Want);

	// 月桥口的石栏杆：双子并肩以后收进地里（1.6 秒），到底就不画了
	if (bTwinsJoined && GapSlabT < 1.7f)
	{
		GapSlabT += Dt;
		const float K = DysisGB::Smoothstep(0.0f, 1.6f, GapSlabT);
		for (const TWeakObjectPtr<UStaticMeshComponent>& W : GapSlab)
			if (UStaticMeshComponent* C = W.Get())
			{
				FVector L = C->GetComponentLocation(), S = C->GetComponentScale();
				S.Z = DtlParaH * (1.0f - K) / 100.0f;
				L.Z = DtlFloorZ[1] + DtlParaH * (1.0f - K) * 0.5f;
				C->SetWorldLocation(L);
				C->SetWorldScale3D(FVector(S.X, S.Y, FMath::Max(S.Z, 0.001)));
				C->SetVisibility(K < 0.999f);
			}
	}
}

FString ADysisDirector::DescribeDetails() const
{
	int32 On = 0, SlabShown = 0;
	for (const FJumpGuard& G : JumpGuards) if (G.Box.IsValid() && G.Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++On;
	for (const TWeakObjectPtr<UStaticMeshComponent>& W : GapSlab) if (W.IsValid() && W->IsVisible()) ++SlabShown;
	const AActor* Chain = Piece(TEXT("SM_Mech_LeverA_Chain"));
	// 现在响着哪几条循环的声音（瀑布近 / 远、水雾、推雕像、捧着苹果）
	const UDysisSfxSubsystem* Loops = UDysisSfxSubsystem::Get(this);
	auto Loop = [Loops](const TCHAR* Id) { return Loops && Loops->IsLoopActive(FName(Id)) ? TEXT("true") : TEXT("false"); };
	return FString::Printf(TEXT("{\"arches\":%d,\"leaves\":%d,\"guards\":%d,\"guardsOn\":%d,\"edgeWalls\":%d,\"gapSlab\":%d,\"gapSlabShown\":%d,\"ledgeParts\":%d,\"leverChainHidden\":%s,\"zone\":\"%s\",")
		TEXT("\"loops\":{\"fallNear\":%s,\"fallFar\":%s,\"mist\":%s,\"push\":%s,\"apple\":%s}}"),
		DetailArches, DetailLeaves, JumpGuards.Num(), On, DetailEdgeWalls, GapSlab.Num(), SlabShown, DetailLedgeParts,
		Chain && Chain->IsHidden() ? TEXT("true") : TEXT("false"), PlayerTime() ? *PlayerTime()->Zone : TEXT(""),
		Loop(TEXT("Amb.WaterfallNear")), Loop(TEXT("Amb.WaterfallFar")), Loop(TEXT("Amb.Mist")), Loop(TEXT("Director.StatuePush")), Loop(TEXT("Director.AppleHold")));
}
