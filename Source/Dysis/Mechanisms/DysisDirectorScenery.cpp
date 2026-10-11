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
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Dysis.FloorTone 0|1：殿里的地面和墙一个颜色（1，默认）还是美术原来单配的偏紫的灰（0）
	FAutoConsoleCommandWithWorldAndArgs GDysisFloorTone(TEXT("Dysis.FloorTone"), TEXT("殿里各层的地面：Dysis.FloorTone 1 = 和墙一个颜色，0 = 原来偏紫的灰"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<ADysisDirector> It(World); It; ++It) It->DebugSetFloorTone(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}));
	// 殿里各层的柱子（见 SetupScenery 里的说明）：画出来的那一层收细到多少、往墙那边挪多少厘米
	constexpr float ScnColSlim = 0.86f, ScnColOut = 13.5f;
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
	// 岛的顶面原来在 −120，和圆基座最下面那一圈（半径 24–24.4 m）、柱廊最下面那一级的面是同一个高度：岛缩小、挪过以后它正好铺在这两处底下，
	// 两个面叠在一起闪（2026-10-10 用户：建筑外面新增的阶梯有些闪烁）。岛整个往下沉 1.2 cm；台基的压顶再比岛低 1.8 cm（原来只差半厘米，远看也会闪）。
	constexpr float ScnTerraceDropCm = 1.2f;
	constexpr float ScnBaseTopZ = -123.0f;
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
	// 镜湖的海面映不到画面以外的东西时，会退回去查天光的那张环境图——那张图里只有“算作天”的东西。
	// 头一版直接把云的材质标成“天”（2026-10-11）：环境图里有云了，可“算作天”的模型不记远近，入夜以后会被夜空 / 暮色那层天幕整个盖住
	// （用户：在殿顶能看到水面上云的倒影但天上没有云了；换个角度倒影又没了）。
	// 现在：云本体还是普通的模型（和原来一样，天幕在它后面）；每朵云里面另套一份略小一点、“算作天”的（M_DysisCloudSky）——
	// 画面上它整个藏在本体里面看不见，只有环境图里看得到它。
	if (CloudSkyMIDs.Num() == 0)
		if (UMaterialInterface* CloudSky = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Sky/M_DysisCloudSky.M_DysisCloudSky"), nullptr, LOAD_NoWarn | LOAD_Quiet))
			for (int32 i = 0; i < 3; ++i) CloudSkyMIDs.Add(UMaterialInstanceDynamic::Create(CloudSky, this));

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

	// 外面一圈柱子、门口柱廊的柱子（2026-10-10 用户发现的三处，都在柱头顶上那块方板上；灰盒的模型里每根柱子是按“朝着殿心”摆的）：
	//   · 方板（24 cm 厚，底在 1520、顶在 1544）整个埋在檐部里，檐部的底面也在 1520——两个面叠在同一个高度上，从下面看那一块会闪。
	//     把柱子竖着压矮一点点（1544 → 1520.3，百分之一点五）：方板整个露在檐部下面，本来柱头的方板就该托在檐部底下。
	//   · 门口柱廊是方的，可十根柱子的方板各朝各的（都朝着殿心，一根一个角度），转过来的角还探出屋顶的底面。
	//     每根绕自己的轴转到和柱廊一个方向（方板的边顺着柱廊的边）。外面那一圈是圆的，方板朝着殿心是对的，不转。
	//   · 方板比檐部的底面宽几厘米：柱子整个收细百分之五，方板就都在檐部 / 屋顶的底面以内了。
	// 柱子的模型原点在殿心（位置是做在模型里的），所以“绕自己的轴转、缩”要连位置一起算。
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tag : It->Tags)
		{
			const FString Label = Tag.ToString();
			const bool bPortico = Label.StartsWith(TEXT("SM_PorticoCol_"));
			if (!bPortico && !Label.StartsWith(TEXT("SM_PeriCol_"))) continue;
			// 这根柱子的轴在哪：看它自己那个模型（根部件）在世界里的位置。不能随手找一个部件——挂在它身上的低模是另一套坐标（转过 90° 的）
			USceneComponent* Root = It->GetRootComponent();
			// 只动画出来的那一层（挂在原模型上的低模）。头一版是把整根柱子（连原模型）一起缩、一起转的：原模型管挡光，
			// 外圈的柱子细了 5%，日1 那束光有四个时刻就比灰盒多漏进来二十分之一（pie_beams 的 b1 对不上）。原模型一点不动，光路就还是灰盒的。
			UStaticMeshComponent* Skin = nullptr;
			for (UStaticMeshComponent* S : SkinComps) if (S && S->GetOwner() == *It) { Skin = S; break; }
			if (!Root || !Skin || !FMath::IsNearlyEqual(Skin->GetComponentScale().X, 1.0)) break;   // 没有低模，或者已经摆过了（这个函数可能叫第二遍）
			const FVector2D Axis(Root->Bounds.Origin);
			const float Slim = 0.95f;
			float Turn = 0.0f;
			if (bPortico)
			{
				// 现在朝着殿心（= 它自己的方位）；转到柱廊的方位。方板是方的，转 90° 等于没转，取最小的那个角
				const float Now = float(FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X)));
				Turn = FMath::Fmod(ScnPorticoAz - Now + 45.0f + 3600.0f, 90.0f) - 45.0f;
			}
			const FVector2D Moved = Axis - (Axis * Slim).GetRotated(Turn);                     // 转、缩都以轴为中心：原点要跟着挪
			// 关卡里每个部件自己本来就转着 90°（模型的坐标和世界差 90°）、位置在殿心：在它原来的朝向上再转
			Skin->SetWorldTransform(FTransform(FRotator(0.0f, float(It->GetActorRotation().Yaw) + Turn, 0.0f), FVector(Moved.X, Moved.Y, 0.0), FVector(Slim, Slim, 1520.3 / 1544.0)));
			break;
		}
	// 殿里各层的柱子（2026-10-10 第八轮，用户：柱廊有的地方凸出来一角、有的穿模）。在低模上量出来的：
	//   · 一、二层柱头顶上的方板比上一层楼板的边（半径 12.19 m）还探出去 5–12 cm——从中庭看，楼板底下支出来一个角；
	//   · 二、三、四层的柱础有 10–20 cm 埋在栏杆里（栏杆 12.19–12.49 m，柱础最里面到 12.27–12.41 m），二层的柱身也蹭进栏杆 4 cm。
	// 画出来的那一层整根收细到 0.86、往墙那边挪 13.5 cm（四层用同一组数，上下还是一条线）：柱础正好让开栏杆，方板全在楼板底下。
	// 原模型（挡人、挡光的）一点不动，光路还是灰盒的；收细、挪开以后画出来的柱子朝墙那一面只比原模型多出 5 cm，人蹭不进去。
	InteriorColumnsFixed = 0;
	for (UStaticMeshComponent* Skin : SkinComps)
	{
		const AActor* A = Skin ? Skin->GetOwner() : nullptr;
		const USceneComponent* Root = A ? A->GetRootComponent() : nullptr;
		if (!Root) continue;
		bool bColumn = false;
		for (const FName& Tag : A->Tags) if (Tag.ToString().StartsWith(TEXT("SM_Col_L"))) { bColumn = true; break; }
		if (!bColumn) continue;
		++InteriorColumnsFixed;
		if (!FMath::IsNearlyEqual(Skin->GetComponentScale().X, 1.0)) continue;      // 已经摆过了（这个函数可能叫第二遍）
		const FTransform Cur = Skin->GetComponentTransform();
		const FVector2D Axis(Root->Bounds.Origin), Was(Cur.GetLocation());
		const FVector2D Moved = Was * ScnColSlim + Axis * (1.0f - ScnColSlim) + Axis.GetSafeNormal() * ScnColOut;      // 以柱子自己的轴为中心收细，再顺着半径往外挪
		Skin->SetWorldTransform(FTransform(Cur.GetRotation(), FVector(Moved.X, Moved.Y, Cur.GetLocation().Z), FVector(ScnColSlim, ScnColSlim, 1.0)));
	}
	// 岛：缩小、挪到以殿心为圆心
	if (AActor* Terrain = Piece(TEXT("SM_Terrain")))
	{
		const float K = ScnTerraceR / ScnTerraceModelR;
		if (USceneComponent* Root = Terrain->GetRootComponent()) Root->SetMobility(EComponentMobility::Movable);
		Terrain->SetActorScale3D(FVector(K, K, 1.0));
		Terrain->SetActorLocation(FVector(ScnTerraceC.X - ScnTerraceModelC.X * K, ScnTerraceC.Y - ScnTerraceModelC.Y * K, -ScnTerraceDropCm));
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
		// 底下填实；正面那三级台阶（每级 40 高、40 宽）顺着两侧接过去，一直接进圆基座的台阶里。
		// 2026-10-10 用户：新做的台阶和原来的台阶接得生硬、看得出是断开的。量下来两处：
		//   · 柱廊那片地面的后沿在离殿心 19.4 m，圆基座的顶面只到半径 23.2 m——两侧各有一个三角的坑，原来的填块只从 18.5 m 起、顶面还矮 1 cm；
		//   · 两侧的台阶也只从 18.5 m 起，可同一级的圆台阶要到 17.8–18.2 m 才和它碰上，中间露出一截断头。
		// 现在都从 17 m 起（整个埋进圆基座里才算完），台阶的顶面比圆基座的同一级低 3 mm：叠着的地方看到的是圆基座的面，不闪，也看不出台阶。
		Stone(1700.0f, 2975.0f, -1489.0f, 1489.0f, Top - 4.0f, -0.4f, Trim, true);    // 比那片地面往里收 1 cm、矮 4 mm：不和它的面叠着闪
		for (const float Side : { -1.0f, 1.0f })
		{
			// 每一级往里多伸 10 cm（埋在上一级里面）：两级之间不留缝
			auto Step = [&](float S0, float S1, float D1x, float TopZ) { Stone(1700.0f, D1x, FMath::Min(Side * (S0 - 10.0f), Side * S1), FMath::Max(Side * (S0 - 10.0f), Side * S1), Top - 4.0f, TopZ - 0.3f, Trim, true); };
			Step(1490.0f, 1530.0f, 2980.0f, -40.0f);
			Step(1530.0f, 1570.0f, 3020.0f, -80.0f);
			Step(1570.0f, 1610.0f, 3060.0f, -120.0f);    // 最下面那一级只比地面高一指：正面那一级顺着拐过来
		}
		// 门廊的屋顶和圆殿接上（2026-10-10 用户：门廊的顶和神殿本身是分开的，参考现实里这类建筑怎么合为一体）。
		// 灰盒的模型里人字屋顶的后端停在离殿心 18.85 m，圆殿的外墙在 17.1 m——中间空着一米七，屋顶像是搁在外圈柱子上的一顶帽子。
		// 现实里“圆殿 + 门廊”（罗马万神殿、帕拉第奥和杰斐逊的圆厅）都是屋顶和檐部一直走到主体里：
		//   · 两坡屋面顺着原来的坡度往后伸，一直伸进圆殿的墙里（墙是圆的，所以切成一条一条，每条的后端都落在墙厚的中间，不捅进殿里）；
		//   · 两侧檐口下面的那道梁也往后接，接到外圈柱子顶上的那一圈檐部上（高度本来就一样）。
		// 只管看，不挡人不挡光。
		{
			UMaterialInterface* RoofStone = ScnPalette(TEXT("MI_MATCH_PedimentStone"));
			const float RidgeZ = 2320.0f, EaveS = 1568.0f, TanT = 600.0f / EaveS, CosT = 1.0f / FMath::Sqrt(1.0f + TanT * TanT), SinT = TanT * CosT;
			const float RoofBackD = 1890.0f, WallMidR = 1640.0f, WallInR = DysisGB::R_IN;
			for (const float Side : { -1.0f, 1.0f })
			{
				float S0 = 0.0f;
				while (S0 < EaveS - 1.0f)
				{
					// 西侧靠檐口的那一段，墙里是夜里的楼梯 TS（墙厚的中间是空的：半径 15.8–16.8 m）。屋面伸到墙厚的中间就捅进楼梯里了
					// （2026-10-10 用户：暗门楼梯里有穿模出来的东西）——这一段只伸进外面那层墙皮（16.8–17.1 m），条切得窄一些。
					// 2026-10-11 用户又拍到了（四层下三层的楼梯里，外墙上方戳进来两片）：上一版留的余量只有 4 cm，可屋面是斜着的一块厚板，
					// 板的下沿比“条的后端”还要往里探出 8 cm 左右——现在后端只到 17.12 m、最靠里的角不过 17.02 m（楼梯的外壁在 16.8 m）。
					const bool bStairs = Side < 0.0f && S0 >= 1290.0f;
					const float BackR = bStairs ? 1712.0f : WallMidR, KeepR = bStairs ? 1702.0f : WallInR + 15.0f;
					auto BackOf = [&](float S) { return FMath::Sqrt(FMath::Max(BackR * BackR - S * S, 0.0f)); };
					float S1 = FMath::Min(S0 + 260.0f, EaveS);
					// 这一条的后端按它外边那一侧定；里边那一侧不能越过内墙面（有楼梯的那一段：不能越过外面那层墙皮的里面）
					while (S1 > S0 + 6.0f && BackOf(S1) < FMath::Sqrt(FMath::Max(KeepR * KeepR - S0 * S0, 0.0f))) S1 -= bStairs ? 2.0f : 10.0f;
					const float BackD = BackOf(S1);
					const float Sa = S0 > 0.0f ? S0 : -12.0f;                                            // 屋脊那一条往对面多伸一点，两坡在脊上咬住，不留缝
					const float Sm = (Sa + S1) * 0.5f;
					const FVector Out = V * (Side * CosT) - FVector(0.0, 0.0, SinT);                     // 顺着屋面往檐口去的方向
					const FVector Center = U * ((BackD + RoofBackD) * 0.5) + V * (Side * Sm) + FVector(0.0, 0.0, RidgeZ - Sm * TanT - 30.0f - 0.6f);
					UStaticMeshComponent* C = Place(Cube, Center, 0.0f, FVector((RoofBackD - BackD) / 100.0, (S1 - Sa) / CosT / 100.0, 60.0 * CosT / 100.0), true, false);
					C->SetWorldRotation(FRotationMatrix::MakeFromXY(U, Out * Side).ToQuat());
					if (RoofStone) C->SetMaterial(0, RoofStone);
					S0 = S1;
				}
				// 檐口下面的梁：从门廊侧面那道梁的后端（离殿心 21 m）接到外圈柱子顶上的檐部里
				Stone(1500.0f, 2101.0f, FMath::Min(Side * 1392.0f, Side * 1500.0f), FMath::Max(Side * 1392.0f, Side * 1500.0f), 1520.5f, 1659.0f, Trim, false);
			}
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
				if (CloudSkyMIDs.Num() >= 3)
				{
					UStaticMeshComponent* S = Place(Mesh, At, It.Yaw + SceneryFarTurnDeg + Turn, It.Scale * (SceneryCloudScale * 0.985f), false, false);
					for (int32 Slot = 0; Slot < 3; ++Slot) S->SetMaterial(Slot, CloudSkyMIDs[Slot]);
				}
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
	// 远处的云（2026-10-11 用户：天空远处也随机放一些云，不然云只出现在近处；不要太规整，要像真的云那样自然地排）。
	// 美术那 6 朵（各放三份）都在离殿心 300–450 m 的一圈上。这里在 0.9–3.2 km 之间再撒十来团：
	//   · 一团一团地来：每团 1–4 朵挤在一起，团和团之间隔多远、离多远、多高都不一样（随机数的种子是定死的，每次开游戏都是同一片天）；
	//   · 越远的越贴近天边、看着越小（按“看上去多宽”来定大小：近处那些的三成到九成）；朝向照近处那些的样子，长的一边横着；
	//   · 不投影子、不挡人、不挡光，颜色和近处的云同一套（跟着时刻变）。
	if (UStaticMesh* CloudMesh = Meshes[int32(EDysisSceneryMesh::Cloud)])
	{
		FRandomStream Rng(20261011);
		float GroupAz = Rng.FRandRange(0.0f, 360.0f);
		for (int32 g = 0; g < 15; ++g)
		{
			GroupAz += Rng.FRandRange(13.0f, 43.0f);
			const float Far = Rng.FRand();                                             // 0 = 这一团里最近的那一档，1 = 最远
			const float GroupDist = FMath::Lerp(90000.0f, 320000.0f, Far * Far * 0.4f + Far * 0.6f);
			const float GroupElev = FMath::Lerp(11.0f, 3.5f, Far) + Rng.FRandRange(-1.5f, 2.5f);      // 仰角：远的贴着天边
			const int32 Count = Rng.RandRange(1, 4);
			for (int32 c = 0; c < Count; ++c)
			{
				const float Az = GroupAz + (c == 0 ? 0.0f : Rng.FRandRange(-8.0f, 8.0f));
				const float Dist = GroupDist * Rng.FRandRange(0.86f, 1.16f);
				const float Elev = FMath::Max(1.8f, GroupElev + (c == 0 ? 0.0f : Rng.FRandRange(-1.6f, 1.8f)));
				// 近处的云：横着的那一边放大 54–69 倍、离殿心 330 m 上下。看上去同样宽的话倍数跟着距离涨
				const float Wide = 60.0f * (Dist / 34000.0f) * Rng.FRandRange(0.34f, 1.05f);
				const FVector Scale(Wide * Rng.FRandRange(0.42f, 0.62f), Wide, Wide * Rng.FRandRange(0.28f, 0.50f));
				const FVector At = DysisGB::PolarCm(Az, Dist, Dist * FMath::Tan(FMath::DegreesToRadians(Elev)));
				const float Yaw = Az - 70.0f - Rng.FRandRange(0.0f, 34.0f);
				UStaticMeshComponent* C = Place(CloudMesh, At, Yaw, Scale, false, false);
				for (int32 Slot = 0; Slot < 3 && CloudMIDs.IsValidIndex(Slot); ++Slot) C->SetMaterial(Slot, CloudMIDs[Slot]);
				if (CloudSkyMIDs.Num() >= 3)
				{
					UStaticMeshComponent* S = Place(CloudMesh, At, Yaw, Scale * 0.985f, false, false);
					for (int32 Slot = 0; Slot < 3; ++Slot) S->SetMaterial(Slot, CloudSkyMIDs[Slot]);
				}
			}
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
			// 各层的地面：美术原来单配了一种偏紫的灰（MI_MATCH_InteriorFloor）。2026-10-10 用户：殿里的建筑在同样的光下有两种颜色，夜里尤其明显
			// （夜里的环境光是蓝的：暖白的墙把它吃掉大半、看着是深灰；偏紫灰的地面照出来就是一片蓝）——改成和墙一样的暖白。Dysis.FloorTone 0 可以切回去对比
			if (Label.StartsWith(TEXT("SM_Floor_L"))) Name = bFloorsLikeWalls ? TEXT("MI_MATCH_WarmWhite") : TEXT("MI_MATCH_InteriorFloor");
			// 屋顶桥门前那几级固定的台阶：和那一圈会升降的台阶一个颜色（2026-10-10 用户：那一排阶梯颜色和别的不一样——美术给它配的是沙色）
			// （2026-10-11：桥门脚下那块平台 SM_Roof_Land 也是沙色，上一轮漏了——用户：拱门所在的那片地面颜色还是和别处有色差）
			else if (Label.StartsWith(TEXT("SM_Roof_Fix")) || Label == TEXT("SM_Roof_Land")) Name = TEXT("MI_MATCH_WarmWhite");
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
		if (CloudSkyMIDs.IsValidIndex(i)) CloudSkyMIDs[i]->SetVectorParameterValue(TEXT("Color"), C);
	}
}

void ADysisDirector::RebuildScenery()
{
	SetupScenery();
}

void ADysisDirector::DebugSetFloorTone(bool bLikeWalls)
{
	bFloorsLikeWalls = bLikeWalls;
	UMaterialInterface* M = ScnPalette(bLikeWalls ? TEXT("MI_MATCH_WarmWhite") : TEXT("MI_MATCH_InteriorFloor"));
	if (!M) return;
	for (UStaticMeshComponent* S : SkinComps)
	{
		const AActor* Part = S ? S->GetOwner() : nullptr;
		if (!Part) continue;
		for (const FName& Tag : Part->Tags)
			if (Tag.ToString().StartsWith(TEXT("SM_Floor_L")))
			{
				for (int32 Slot = 0; Slot < S->GetNumMaterials(); ++Slot) S->SetMaterial(Slot, M);
				break;
			}
	}
}

FString ADysisDirector::DescribeRound8() const
{
	// 地面现在用的材质；殿里的柱子（画出来的那一层）收细、挪开了几根，其中一根现在在哪
	FString Floor, Col;
	for (const UStaticMeshComponent* S : SkinComps)
	{
		const AActor* Part = S ? S->GetOwner() : nullptr;
		if (!Part) continue;
		for (const FName& Tag : Part->Tags)
		{
			const FString Label = Tag.ToString();
			if (Floor.IsEmpty() && Label.StartsWith(TEXT("SM_Floor_L")) && S->GetMaterial(0)) Floor = S->GetMaterial(0)->GetName();
			if (Col.IsEmpty() && Label.StartsWith(TEXT("SM_Col_L1_")) && Part->GetRootComponent())
			{
				const FVector2D Axis(Part->GetRootComponent()->Bounds.Origin), At(S->Bounds.Origin);
				Col = FString::Printf(TEXT("{\"tag\":\"%s\",\"axisR\":%.1f,\"skinR\":%.1f,\"scale\":%.3f}"), *Label, Axis.Size(), At.Size(), S->GetComponentScale().X);
			}
		}
	}
	const UStaticMeshComponent* Sea = nullptr;
	if (const AActor* A = Piece(TEXT("SM_Mech_Water_Sea"))) Sea = A->FindComponentByClass<UStaticMeshComponent>();
	const UMaterialInstanceDynamic* SeaNow = Sea ? Cast<UMaterialInstanceDynamic>(Sea->GetMaterial(0)) : nullptr;
	return FString::Printf(TEXT("{\"floor\":\"%s\",\"floorsLikeWalls\":%s,\"columns\":%d,\"column\":%s,\"inletWater\":%s,\"inletVisible\":%s,\"seaMirror\":%s,\"seaMaterial\":\"%s\"}"),
		*Floor, bFloorsLikeWalls ? TEXT("true") : TEXT("false"), InteriorColumnsFixed, Col.IsEmpty() ? TEXT("null") : *Col,
		InletWater ? TEXT("true") : TEXT("false"), InletWater && InletWater->IsVisible() ? TEXT("true") : TEXT("false"),
		bSeaMirror ? TEXT("true") : TEXT("false"), SeaNow && SeaNow->Parent ? *SeaNow->Parent->GetName() : TEXT(""));
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
