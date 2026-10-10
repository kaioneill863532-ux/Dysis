// 日落回廊 · 殿外的配景和夕照的配色（场景美术 2026-10-09 的外景 ReferenceMatch_20261009）。全是看的东西，不参与光路和机关的判定。
//   · 山、石、草坡、云 61 件 + 山花上的日月徽：模型和调色板材质是美术的（/Game/Dysis/ReferenceMatch_20261009、LowPoly_v001），
//     摆放清单在 DysisSceneryData.generated.h（由 Art/Scenery/gen_scenery_data.py 从他的清单生成）。
//   · 建筑（低模）换上他那版的配色：暖白、沙色、赭石；山花深一档，各层地面偏灰粉；台地顶上是沙地、外圈是草、朝小岛那边留一条路。
// 和美术那版不一样的地方——我们的世界是灰盒 v0.12 的：神殿立在海里的台地上，开局在台地下面 12 m 的小岛上，日月是真的在走：
//   1. 他那版把海关了、铺了一圈陆地（地形壳）。陆地会把小岛和开场那道光埋掉，所以地形壳没搬。
//      台地上放得下的石头和草坡照他的位置放；台地外面的石头沉到海里当礁石，草坡不要；小岛和它通到台地的那一条线上不放东西。
//   2. 他那版的夕照是另放的一盏主光，和真的太阳不在一个方向，远山正好挡在我们的太阳落下去的那一边。
//      我们的光路、影子都靠真的太阳，所以远山、巨岩和云整体绕殿心转了 SceneryFarTurnDeg，把日落和月出的方向让出来。
//      他的六朵云只摆在一小段天边（给那台固定镜头看的）；这里同一组云在西北、正北再各摆一份，抬高放大了一些——
//      都避开太阳整个下午走的那一段天、月亮刚升起来的那一段天和主界面上日月转的那一片。
//   3. 都不挡光。台地上的石头挡人、挡镜头；草坡、礁石、山、云什么都不挡。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisSceneryData.generated.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisTimeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* ScnRoot = TEXT("/Game/Dysis/ReferenceMatch_20261009");
	// 台地（SM_Terrain 的顶面）。模型本身是灰盒那版：圆心朝背着小岛的方向偏了 7 m、半径 39 m——神殿不在岛中间，背后空一大片，
	// 门口柱廊的两个前角还悬在崖外面。2026-10-10 用户要整体调：改成以殿心为圆心、半径 32 m（游戏开始时把模型缩一下、挪一下），
	// 四周留的边一样宽（基座外 7.6 m）；朝小岛的前沿还在原处——开场那道光斜着上来、落在殿心外 31.8 m，岛再往前伸就把光挡了。
	// 柱廊是方的、比圆的岛宽：它底下另砌一段石头台基（见 SetupScenery 里的 Stone）。
	const FVector2D ScnTerraceModelC(-647.0, -268.0);
	constexpr float ScnTerraceModelR = 3906.0f;
	const FVector2D ScnTerraceC(0.0, 0.0);
	constexpr float ScnTerraceR = 3200.0f;
	constexpr float ScnPodiumR = 2440.0f;    // 神殿圆基座最下面一级台阶的外沿
	// 柱廊的轴线（方位 22.5°，朝小岛）；台基的尺寸（沿轴线离殿心多远 = D，往两边 = S，厘米）：
	// 柱廊的地面到 D 2980、S ±1490，三级台阶落到 D 3100、S ±1610；台基的墙面在 D 3205（和岛的前沿齐）、S ±1775
	constexpr float ScnPorticoAz = 22.5f, ScnBaseD0 = 2500.0f, ScnBaseD1 = 3205.0f, ScnBaseS = 1775.0f;
	constexpr float ScnBaseTopZ = -120.5f;   // 比台地的顶面（−120）低半厘米：叠在一起的地方不闪
	// 小岛（顶面的圆心在方位 23.7°、离殿心 52.6 m）：从殿心朝它去的这一条线两边各 ScnLaneHalfW 以内不放东西（开场的光从这里伸到岛上）
	const FVector2D ScnIsleC(4815.0, 2114.0);
	constexpr float ScnLaneHalfW = 1100.0f;
	// 石头的模型大约是半径 85 cm、半高 93 cm 的一块（没缩放时）
	constexpr float ScnRockR = 85.0f, ScnRockH = 93.0f;
	// 那一组云再多摆几处：各自在 SceneryFarTurnDeg 之外再转多少度（0 = 美术那一组，转完在东南的山上面；150、200 = 西北、正北的海上）
	const float ScnCloudTurns[] = { 0.0f, 150.0f, 200.0f };

	UMaterialInterface* ScnPalette(const FString& Name)
	{
		const FString Path = FString::Printf(TEXT("%s/Materials/%s.%s"), ScnRoot, *Name, *Name);
		return LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

void ADysisDirector::SetupScenery()
{
	for (UStaticMeshComponent* C : SceneryComps) if (C) C->DestroyComponent();
	SceneryComps.Reset();
	SceneryOnTerrace = ScenerySeaRocks = ScenerySkipped = ScenerySolid = 0;

	static const TCHAR* MeshNames[] = { TEXT("SM_MATCH_RockA"), TEXT("SM_MATCH_RockB"), TEXT("SM_MATCH_Mountain"), TEXT("SM_MATCH_Cloud") };
	UStaticMesh* Meshes[UE_ARRAY_COUNT(MeshNames)] = {};
	for (int32 i = 0; i < UE_ARRAY_COUNT(MeshNames); ++i)
		Meshes[i] = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/Meshes/%s.%s"), ScnRoot, MeshNames[i], MeshNames[i]), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Meshes[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("Dysis 配景：找不到 %s 下的模型，殿外的山石云没放"), ScnRoot);
		return;
	}
	// 云：三个面组各一档颜色，所有的云共用这三份材质（颜色每帧按时刻写，见 UpdateScenery）
	if (CloudMIDs.Num() == 0)
		if (UMaterialInterface* Cloud = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Sky/M_DysisCloud.M_DysisCloud")))
			for (int32 i = 0; i < 3; ++i) CloudMIDs.Add(UMaterialInstanceDynamic::Create(Cloud, this));

	auto Place = [this](UStaticMesh* Mesh, const FVector& Loc, float Yaw, const FVector& Scale, bool bShadow, bool bSolid)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(bShadow);
		C->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bSolid)
		{
			C->SetCollisionObjectType(ECC_WorldStatic);
			C->SetCollisionResponseToAllChannels(ECR_Ignore);          // 不挡光（光路的判定走 Visibility）
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			C->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		}
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(Loc, FRotator(0.0f, Yaw, 0.0f));
		C->SetWorldScale3D(Scale);
		SceneryComps.Add(C);
		return C;
	};

	// 岛：缩小、挪到以殿心为圆心
	if (AActor* Terrain = Piece(TEXT("SM_Terrain")))
	{
		const float K = ScnTerraceR / ScnTerraceModelR;
		if (USceneComponent* Root = Terrain->GetRootComponent()) Root->SetMobility(EComponentMobility::Movable);
		Terrain->SetActorScale3D(FVector(K, K, 1.0));
		Terrain->SetActorLocation(FVector(ScnTerraceC.X - ScnTerraceModelC.X * K, ScnTerraceC.Y - ScnTerraceModelC.Y * K, 0.0));
	}
	// 柱廊下面的石砌台基：按古典台基的三段——基座、墙身、压顶（各带一道线脚），正面的壁柱一根对着上面一根柱子。
	// 墙身是沙色的石头，线脚和壁柱是暖白的；一直砌到海里。挡人、挡镜头，不挡光。
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		const double Ax = FMath::DegreesToRadians(double(ScnPorticoAz));
		const FVector U(FMath::Cos(Ax), FMath::Sin(Ax), 0.0), V(-FMath::Sin(Ax), FMath::Cos(Ax), 0.0);
		UMaterialInterface* Wall = ScnPalette(TEXT("MI_MATCH_Sand"));
		UMaterialInterface* Trim = ScnPalette(TEXT("MI_MATCH_WarmWhite"));
		auto Stone = [&](float D0, float D1, float S0, float S1, float Z0, float Z1, UMaterialInterface* M, bool bSolid)
		{
			const FVector Center = U * ((D0 + D1) * 0.5) + V * ((S0 + S1) * 0.5) + FVector(0.0, 0.0, (Z0 + Z1) * 0.5);
			UStaticMeshComponent* C = Place(Cube, Center, ScnPorticoAz, FVector((D1 - D0) / 100.0, (S1 - S0) / 100.0, (Z1 - Z0) / 100.0), true, bSolid);
			if (M) C->SetMaterial(0, M);
		};
		const float D0 = ScnBaseD0, D1 = ScnBaseD1, S = ScnBaseS, Top = ScnBaseTopZ, Sea = DysisGB::SEA_Z;
		const float CapZ = Top - 85.0f, BedZ = CapZ - 30.0f, FootZ = Sea + 450.0f, ShoeZ = FootZ - 60.0f;
		Stone(D0, D1 + 24.0f, -S - 24.0f, S + 24.0f, CapZ, Top, Trim, true);               // 压顶
		Stone(D0, D1 + 11.0f, -S - 11.0f, S + 11.0f, BedZ, CapZ, Trim, false);             // 压顶下面的一道线脚
		Stone(D0, D1, -S, S, FootZ, BedZ, Wall, true);                                      // 墙身
		Stone(D0, D1 + 24.0f, -S - 24.0f, S + 24.0f, ShoeZ, FootZ, Trim, false);           // 基座上沿的一道线脚
		Stone(D0, D1 + 48.0f, -S - 48.0f, S + 48.0f, Sea - 200.0f, ShoeZ, Wall, true);     // 基座
		// 柱廊的地面（灰盒的模型）只是薄薄的一片：两侧底下是空的，从旁边一看人像踩在悬着的板上。
		// 底下填实；正面那三级台阶（每级 40 高、40 宽）顺着两侧接过去，一直接到圆基座的台阶上。
		Stone(1850.0f, 2975.0f, -1489.0f, 1489.0f, Top - 4.0f, -1.0f, Trim, true);   // 比那片地面往里收 1 cm、矮 1 cm：不和它的面叠着闪
		for (const float Side : { -1.0f, 1.0f })
		{
			auto Step = [&](float S0, float S1, float D1x, float TopZ) { Stone(1850.0f, D1x, FMath::Min(Side * S0, Side * S1), FMath::Max(Side * S0, Side * S1), Top - 4.0f, TopZ, Trim, true); };
			Step(1490.0f, 1530.0f, 2980.0f, -40.0f);
			Step(1530.0f, 1570.0f, 3020.0f, -80.0f);
		}
		// 壁柱：正面六根，对着柱廊前排的六根柱子；两个侧面各一根，对着前排柱子那一线
		for (const float At : { -1320.0f, -790.0f, -264.0f, 264.0f, 790.0f, 1320.0f })
			Stone(D1, D1 + 16.0f, At - 62.0f, At + 62.0f, FootZ, BedZ, Trim, false);
		for (const float Side : { -1.0f, 1.0f })
			Stone(2841.0f - 62.0f, 2841.0f + 62.0f, Side > 0.0f ? S : -S - 16.0f, Side > 0.0f ? S + 16.0f : -S, FootZ, BedZ, Trim, false);
	}

	// 岛的崖壁：灰盒的模型是一个光溜溜的圆筒，从岛外看像一只罐头。2026-10-10 用户：要像海岛——侧边是崎岖的山岩、边缘不规则，
	// 后来又发了两张真海岛的照片（一座礁岩小岛、一座平顶的岛）：要的是主体的侧面轮廓，高度不用加（人还要从光上走过来）。
	// 做法：用周围礁石同一套石头（美术的 RockA / RockB、同样的配色）从崖顶到海面贴四层，越往下越往外，侧面看是一道坡：
	//   崖顶——小一些的石头，有的冒出草地一截，把那一圈圆边打破；半腰——竖着的山岩，嵌进崖里一半；
	//   下坡——再往外的大块山岩；水线——又大又扁的礁盘，探到崖外七八米。外沿不是正圆：沿着一圈有几处凸出去的岬、凹进来的湾。
	//   岛外另有三处离岸的石柱（一高几矮挤在一起）。
	// 位置用固定的随机种子撒（每次开游戏都一样）。正面石砌台基那一段（柱廊轴线两边各 41°）不放，两头各立一块大的把台基夹住。
	// 冒出草地的那些挡人、挡镜头，其余只管看；都不挡光。
	if (Meshes[0] && Meshes[1])
	{
		FRandomStream Rnd(20261010);
		auto CragAt = [&](const FVector& At, const FVector& Sc, bool bLavender, bool bSolid, float Tilt = 14.0f)
		{
			UStaticMeshComponent* C = Place(Meshes[Rnd.RandRange(0, 1)], At, 0.0f, Sc, true, bSolid);
			C->SetWorldRotation(FRotator(Rnd.FRandRange(-Tilt, Tilt), Rnd.FRandRange(0.0f, 360.0f), Rnd.FRandRange(-Tilt, Tilt)));
			const TCHAR* Pal = bLavender ? TEXT("Lavender") : TEXT("Peach");
			for (int32 Slot = 0; Slot < 3; ++Slot)
				if (UMaterialInterface* M = ScnPalette(Slot == 0 ? FString::Printf(TEXT("MI_MATCH_%s"), Pal) : FString::Printf(TEXT("MI_MATCH_%s_%d"), Pal, Slot))) C->SetMaterial(Slot, M);
		};
		auto Crag = [&](float AzDeg, float Rad, float Zc, const FVector& Sc, bool bLavender, bool bSolid, float Tilt = 14.0f)
		{
			CragAt(DysisGB::PolarCm(AzDeg, Rad, Zc), Sc, bLavender, bSolid, Tilt);
		};
		const float R = ScnTerraceR, Top = -120.0f, Sea = DysisGB::SEA_Z;
		// 岬和湾：外沿在这个方位往外（往里）多少（0–1 的比例，乘在每一层自己的幅度上）
		auto Lobe = [](float AzDeg)
		{
			const float A = FMath::DegreesToRadians(AzDeg);
			return 0.5f * FMath::Sin(2.0f * A + 0.9f) + 0.32f * FMath::Sin(3.0f * A + 2.3f) + 0.18f * FMath::Sin(5.0f * A + 4.1f);
		};
		const int32 Slots = 46;
		for (int32 i = 0; i < Slots; ++i)
		{
			const float Az = 360.0f * i / Slots;
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Az, ScnPorticoAz)) < 41.0f) continue;
			const float L = Lobe(Az);
			// 水线：礁盘
			if (Rnd.FRand() < 0.9f)
			{
				const float S = Rnd.FRandRange(7.0f, 11.0f);
				Crag(Az + Rnd.FRandRange(-3.0f, 3.0f), R + 620.0f + 330.0f * L + Rnd.FRandRange(-160.0f, 160.0f), Sea + Rnd.FRandRange(-120.0f, 110.0f),
					FVector(S, S * Rnd.FRandRange(0.75f, 1.15f), Rnd.FRandRange(3.0f, 5.0f)), Rnd.FRand() < 0.45f, false, 9.0f);
			}
			// 下坡：大块山岩
			{
				const float Zc = Rnd.FRandRange(-1330.0f, -980.0f), S = Rnd.FRandRange(6.0f, 9.0f);
				Crag(Az + Rnd.FRandRange(-3.5f, 3.5f), R + 300.0f + 210.0f * L + Rnd.FRandRange(-110.0f, 110.0f), Zc,
					FVector(S, S * Rnd.FRandRange(0.7f, 1.1f), FMath::Min(Rnd.FRandRange(5.0f, 7.6f), (Top - 300.0f - Zc) / ScnRockH)), Rnd.FRand() < 0.3f, false);
			}
			// 半腰：竖着的山岩
			{
				const float Zc = Rnd.FRandRange(-1000.0f, -640.0f), S = Rnd.FRandRange(4.6f, 7.0f);
				Crag(Az + Rnd.FRandRange(-3.5f, 3.5f), R + 20.0f + 90.0f * L + Rnd.FRandRange(-130.0f, 110.0f), Zc,
					FVector(S, S * Rnd.FRandRange(0.7f, 1.1f), FMath::Min(Rnd.FRandRange(5.0f, 9.0f), (Top - 55.0f - Zc) / ScnRockH)), Rnd.FRand() < 0.2f, false);
			}
			// 崖顶
			if (Rnd.FRand() < 0.8f)
			{
				const float Zc = Rnd.FRandRange(-430.0f, -250.0f), S = Rnd.FRandRange(2.5f, 3.5f);
				const float TopZ = Rnd.FRand() < 0.4f ? Top + Rnd.FRandRange(30.0f, 210.0f) : Top - Rnd.FRandRange(10.0f, 80.0f);   // 四成冒出草地一截
				Crag(Az + Rnd.FRandRange(-3.5f, 3.5f), R + Rnd.FRandRange(-60.0f, 45.0f), Zc, FVector(S, S * Rnd.FRandRange(0.75f, 1.1f), (TopZ - Zc) / ScnRockH), Rnd.FRand() < 0.15f, TopZ > Top + 10.0f);
			}
		}
		for (const float Side : { -1.0f, 1.0f })
		{
			Crag(ScnPorticoAz + Side * 44.0f, R - 30.0f, -900.0f, FVector(7.6, 6.4, 8.0), false, false);
			Crag(ScnPorticoAz + Side * 47.0f, R + 330.0f, -1300.0f, FVector(8.4, 7.2, 6.0), Side > 0.0f, false);
		}
		// 正面（2026-10-10 用户：正面也要改，不然一整面石墙直落海里，和周围不搭）：石砌台基前面也堆三层——贴着墙脚的山岩、
		// 下坡的大石、水线的礁盘，台基只从岩石上面露出上半截。开场那道光从小岛斜着上来（落在轴线上离殿心 31.8 m、高 −80，
		// 每往外 1 m 低 0.65 m）：光底下两边各五米多以内的石头，顶都压在光下面两米以外，不挡光也不碍着人走。
		{
			const double Ax = FMath::DegreesToRadians(double(ScnPorticoAz));
			const FVector U(FMath::Cos(Ax), FMath::Sin(Ax), 0.0), V(-FMath::Sin(Ax), FMath::Cos(Ax), 0.0);
			auto BeamZ = [](float D) { return -80.0f - 0.65f * (D - 3180.0f); };
			const float Front = ScnBaseD1 + 48.0f;
			for (float S0 = -2150.0f; S0 <= 2150.0f; S0 += 330.0f)
			{
				const float Sx = S0 + Rnd.FRandRange(-120.0f, 120.0f);
				const bool bLane = FMath::Abs(Sx) < 560.0f;
				{
					// 贴着墙脚
					const float D = Front + 20.0f + Rnd.FRandRange(0.0f, 200.0f), Zc = Rnd.FRandRange(-1000.0f, -700.0f), S = Rnd.FRandRange(4.4f, 6.6f);
					const float TopZ = bLane ? FMath::Min(-340.0f, BeamZ(D) - 210.0f) : Rnd.FRandRange(-340.0f, -190.0f);
					CragAt(U * D + V * Sx + FVector(0.0, 0.0, Zc), FVector(S, S * Rnd.FRandRange(0.7f, 1.1f), (TopZ - Zc) / ScnRockH), Rnd.FRand() < 0.2f, false);
				}
				{
					// 下坡
					const float D = Front + 300.0f + Rnd.FRandRange(-120.0f, 160.0f), Zc = Rnd.FRandRange(-1350.0f, -1050.0f), S = Rnd.FRandRange(6.0f, 8.5f);
					const float TopZ = bLane ? FMath::Min(-620.0f, BeamZ(D + 300.0f) - 220.0f) : Rnd.FRandRange(-700.0f, -470.0f);
					CragAt(U * D + V * (Sx + Rnd.FRandRange(-90.0f, 90.0f)) + FVector(0.0, 0.0, Zc), FVector(S, S * Rnd.FRandRange(0.7f, 1.1f), (TopZ - Zc) / ScnRockH), Rnd.FRand() < 0.3f, false);
				}
				if (Rnd.FRand() < 0.9f)
				{
					// 水线
					const float S = Rnd.FRandRange(7.0f, 10.5f);
					CragAt(U * (Front + 640.0f + Rnd.FRandRange(-150.0f, 250.0f)) + V * (Sx + Rnd.FRandRange(-90.0f, 90.0f)) + FVector(0.0, 0.0, Sea + Rnd.FRandRange(-120.0f, 80.0f)),
						FVector(S, S * Rnd.FRandRange(0.75f, 1.15f), Rnd.FRandRange(3.0f, 4.6f)), Rnd.FRand() < 0.45f, false, 9.0f);
				}
			}
		}
		// 离岸的石柱：三处，各是一根高的带两三根矮的（都比岛顶矮）
		for (const float At : { 118.0f, 205.0f, 292.0f })
		{
			const float Az0 = At + Rnd.FRandRange(-8.0f, 8.0f), R0 = R + Rnd.FRandRange(1150.0f, 1500.0f);
			const float H = Rnd.FRandRange(8.0f, 10.0f);                                          // 高的那根：半高 7–9 m，立在水里，顶在岛顶下面一两米
			Crag(Az0, R0, Sea + H * ScnRockH * 0.45f, FVector(Rnd.FRandRange(3.6f, 4.6f), Rnd.FRandRange(3.2f, 4.2f), H), false, false, 6.0f);
			for (int32 k = 0, N = Rnd.RandRange(2, 3); k < N; ++k)
			{
				const float Hk = Rnd.FRandRange(4.0f, 7.0f);
				Crag(Az0 + Rnd.FRandRange(-5.0f, 5.0f), R0 + Rnd.FRandRange(-330.0f, 330.0f), Sea + Hk * ScnRockH * 0.4f, FVector(Rnd.FRandRange(3.0f, 5.5f), Rnd.FRandRange(3.0f, 5.0f), Hk), Rnd.FRand() < 0.5f, false, 9.0f);
			}
		}
	}

	const FVector2D IsleDir = ScnIsleC.GetSafeNormal();
	for (const FDysisSceneryItem& It : GDysisScenery)
	{
		UStaticMesh* Mesh = Meshes[int32(It.Mesh)];
		if (!Mesh) continue;
		FVector Loc = It.Loc;
		FVector Scale = It.Scale;
		float Yaw = It.Yaw;
		bool bShadow = false, bSolid = false;
		const bool bCloud = It.Kind == EDysisSceneryKind::Cloud;
		const bool bFar = bCloud || It.Kind == EDysisSceneryKind::FarMountain || It.Kind == EDysisSceneryKind::MidMountain || It.Kind == EDysisSceneryKind::Monolith;
		if (bCloud)
		{
			for (const float Turn : ScnCloudTurns)
			{
				FVector At = It.Loc.RotateAngleAxis(SceneryFarTurnDeg + Turn, FVector::UpVector);
				At.Z *= SceneryCloudLift;
				UStaticMeshComponent* C = Place(Mesh, At, It.Yaw + SceneryFarTurnDeg + Turn, It.Scale * SceneryCloudScale, false, false);
				for (int32 Slot = 0; Slot < 3 && CloudMIDs.IsValidIndex(Slot); ++Slot) C->SetMaterial(Slot, CloudMIDs[Slot]);
			}
			continue;
		}
		if (bFar)
		{
			Loc = Loc.RotateAngleAxis(SceneryFarTurnDeg, FVector::UpVector);
			Yaw += SceneryFarTurnDeg;
		}
		else
		{
			const bool bGrass = It.Kind == EDysisSceneryKind::GrassBank || It.Kind == EDysisSceneryKind::EdgeGrassBank;
			float Radius = ScnRockR * float(FMath::Max(It.Scale.X, It.Scale.Y));
			const double FromCenter = FVector2D::Distance(FVector2D(Loc), ScnTerraceC);
			if (FromCenter <= ScnTerraceR + 150.0)
			{
				// 岛上的：基座（半径 24.4 m）和崖边之间只有七米多宽的一圈，大石头放不下（不是探出崖边就是压到台阶上）——
				// 缩到这一圈放得下，再挪到圈里面（离台阶、离崖边都留一点）
				const float Ring = ScnTerraceR - ScnPodiumR;
				const float Fit = FMath::Min(1.0f, (Ring * 0.5f - 60.0f) / FMath::Max(Radius, 1.0f));
				Scale *= Fit;
				Radius *= Fit;
				const double Want = FMath::Clamp(FromCenter, double(ScnPodiumR + Radius + 50.0f), double(ScnTerraceR - Radius - 50.0f));
				const FVector2D Dir = (FVector2D(Loc) - ScnTerraceC).GetSafeNormal();
				Loc.X = ScnTerraceC.X + Dir.X * Want;
				Loc.Y = ScnTerraceC.Y + Dir.Y * Want;
				++SceneryOnTerrace;
				bShadow = true;
				bSolid = !bGrass;
				if (bSolid) ++ScenerySolid;
			}
			else
			{
				// 台地外面：草坡不要；石头沉到海里当礁石（露出水面的部分 = 半高 × (1 + ScenerySeaRockRise)），小岛那一条线上的不要
				const FVector2D P(Loc);
				const bool bInLane = FVector2D::DotProduct(P, IsleDir) > 0.0 && FMath::Abs(FVector2D::CrossProduct(P, IsleDir)) < ScnLaneHalfW + Radius;
				if (bGrass || bInLane) { ++ScenerySkipped; continue; }
				Loc.Z = DysisGB::SEA_Z + ScenerySeaRockRise * ScnRockH * float(It.Scale.Z);
				bShadow = true;
				++ScenerySeaRocks;
			}
		}
		UStaticMeshComponent* C = Place(Mesh, Loc, Yaw, Scale, bShadow, bSolid);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const FString Name = Slot == 0 ? FString::Printf(TEXT("MI_MATCH_%s"), It.Palette) : FString::Printf(TEXT("MI_MATCH_%s_%d"), It.Palette, Slot);
			if (UMaterialInterface* M = ScnPalette(Name)) C->SetMaterial(Slot, M);
		}
	}
	// 山花上的日月徽（美术在低模那一版加的小装饰：一个赭石色的环，模型自带位置，和建筑一样转 90°）
	if (UStaticMesh* Emblem = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dysis/LowPoly_v001/Meshes/LP_Pediment_SunMoon.LP_Pediment_SunMoon"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UStaticMeshComponent* C = Place(Emblem, FVector::ZeroVector, 90.0f, FVector::OneVector, true, false);
		if (UMaterialInterface* M = ScnPalette(TEXT("MI_MATCH_Ochre"))) C->SetMaterial(0, M);
	}
	ApplyPalette();
	UpdateScenery();
	UE_LOG(LogTemp, Log, TEXT("Dysis 配景：放了 %d 件（台地上 %d、礁石 %d，没放 %d），%d 个建筑部件的面换了配色"),
		SceneryComps.Num(), SceneryOnTerrace, ScenerySeaRocks, ScenerySkipped, PaletteSlots);
}

void ADysisDirector::ApplyPalette()
{
	// 建筑（低模那一层）换成美术那版外景的配色。低模自带的材质叫 LP_<颜色>，对应的新材质叫 MI_MATCH_<颜色>；有几处他单独定过
	PaletteSlots = 0;
	for (UStaticMeshComponent* S : SkinComps)
	{
		const AActor* Part = S ? S->GetOwner() : nullptr;
		if (!Part) continue;
		FString Label;
		for (const FName& Tag : Part->Tags) if (Tag.ToString().StartsWith(TEXT("SM_"))) { Label = Tag.ToString(); break; }
		for (int32 Slot = 0; Slot < S->GetNumMaterials(); ++Slot)
		{
			const UMaterialInterface* Cur = S->GetMaterial(Slot);
			FString Name = Cur ? Cur->GetName() : FString();
			if (!Name.RemoveFromStart(TEXT("LP_"))) continue;
			Name = TEXT("MI_MATCH_") + Name;
			if (Label.StartsWith(TEXT("SM_Floor_L"))) Name = TEXT("MI_MATCH_InteriorFloor");                              // 各层的地面
			else if (Label == TEXT("SM_Portico_Roof")) Name = Slot == 0 ? TEXT("MI_MATCH_PedimentStone") : TEXT("MI_MATCH_WarmWhite");   // 门廊的山花和屋面深一档，线脚还是浅的
			else if (Label == TEXT("SM_Terrain")) Name = Slot == 0 ? TEXT("M_MATCH_Ground") : TEXT("MI_MATCH_Peach");      // 台地：顶上沙地 / 草 / 路，侧面的崖壁
			else if (Label == TEXT("SM_Island")) Name = TEXT("MI_MATCH_Peach");                                             // 开局的小岛
			if (UMaterialInterface* M = ScnPalette(Name)) { S->SetMaterial(Slot, M); ++PaletteSlots; }
		}
	}
}

void ADysisDirector::UpdateScenery()
{
	// 云的颜色跟着天走：白天是天光照着的粉白，太阳低了偏橙红，入夜只剩一点月色
	const UDysisTimeComponent* Time = PlayerTime();
	const ADysisSkyActor* Sky = Time ? Time->SkyActor.Get() : nullptr;
	if (!Sky || CloudMIDs.Num() < 3) return;
	float SunAlt = 30.0f, MoonAlt = -30.0f;
	Sky->GetAltitudes(SunAlt, MoonAlt);
	const float Low = 1.0f - DysisGB::Smoothstep(2.0f, 25.0f, SunAlt);
	const float Night = DysisGB::Smoothstep(-2.0f, -10.0f, SunAlt), MoonUp = DysisGB::Smoothstep(0.0f, 12.0f, MoonAlt);
	// 三档：美术那版云的三个颜色（他的数 ÷ 天的亮度 12000）；太阳低的时候各自偏橙红
	static const FLinearColor Day[3] = { FLinearColor(0.875f, 0.633f, 0.692f), FLinearColor(0.725f, 0.517f, 0.642f), FLinearColor(0.542f, 0.492f, 0.650f) };
	static const FLinearColor Dusk[3] = { FLinearColor(1.25f, 0.56f, 0.36f), FLinearColor(0.86f, 0.40f, 0.42f), FLinearColor(0.46f, 0.32f, 0.52f) };
	for (int32 i = 0; i < 3; ++i)
	{
		const FLinearColor C = (Day[i] + (Dusk[i] - Day[i]) * Low) * (Sky->GetSkyGain() * SceneryCloudGain)
			+ FLinearColor(0.11f, 0.14f, 0.22f) * (Night * (0.4f + 0.6f * MoonUp) * (1.0f - 0.2f * i));   // 比夜空稍亮一点：月光照着的云，不是天上的一块黑
		CloudMIDs[i]->SetVectorParameterValue(TEXT("Color"), C);
	}
}

void ADysisDirector::RebuildScenery()
{
	SetupScenery();
}

FString ADysisDirector::DescribeScenery() const
{
	int32 Shown = 0, Solid = 0, BlockLight = 0;
	for (const UStaticMeshComponent* C : SceneryComps)
	{
		if (!C) continue;
		if (C->IsVisible()) ++Shown;
		if (C->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++Solid;
		if (C->GetCollisionEnabled() != ECollisionEnabled::NoCollision && C->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Ignore) ++BlockLight;
	}
	return FString::Printf(TEXT("{\"placed\":%d,\"shown\":%d,\"onTerrace\":%d,\"seaRocks\":%d,\"skipped\":%d,\"solid\":%d,\"blockLight\":%d,\"paletteSlots\":%d,\"clouds\":%d}"),
		SceneryComps.Num(), Shown, SceneryOnTerrace, ScenerySeaRocks, ScenerySkipped, Solid, BlockLight, PaletteSlots, CloudMIDs.Num());
}
