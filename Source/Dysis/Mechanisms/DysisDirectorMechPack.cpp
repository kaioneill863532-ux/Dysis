// 机关总管 · 美术的机关模型（2026-10-10 的《打包.zip》，转换和导入见 Art/Models/mech-pack-20261010/）。
// 只换“看起来的样子”：原来的部件不画了，碰撞、挡光、会动的逻辑都还是它；新模型挂在原部件身上，跟着它转、跟着它沉、跟着它藏。
// 模型的规矩（转换时摆好的）：+Y 是正面、+Z 朝上、原点在脚底中心（浮雕：原点在墙面上、底边中点，+Y 朝殿心，背板已经顺着圆墙弯好）。
// 颜色都在顶点色里：材质 M_DysisMechPack 把没什么颜色的（石头、石膏）统一成墙的那种暖白，金边、镜面、彩虹这些点缀照原样。
// 哪个模型换哪个部件，有四处是问过用户的（三相像上日中月下暗；推的是“双子_右”；拱门给屋顶细桥的桥门，拱还给虹门；月门给月亮浮雕、不要深紫色的门板）。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisNightData.generated.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ProceduralMeshComponent.h"
#include "Sky/DysisTimeComponent.h"
#include "Sky/DysisSkyLibrary.h"
#include "World/DysisWorldState.h"
#include "Beams/DysisBeamActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Dysis.GoddessShift <度> <厘米> [抬高厘米]：把瀑布后面的女神像（连判定月光的点）从灰盒的位置绕殿心转几度、往墙那边挪几厘米、抬高几厘米（试位置用）
	FAutoConsoleCommandWithWorldAndArgs GDysisGoddessShift(TEXT("Dysis.GoddessShift"), TEXT("瀑布后面的女神像：Dysis.GoddessShift <绕殿心转几度> <往墙那边挪几厘米> [抬高几厘米]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2) return;
			for (TActorIterator<ADysisDirector> It(World); It; ++It) It->DebugMoveGoddess(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.0f);
		}));
	// Dysis.SwanShift <厘米>：四层的天鹅像往墙那边挪多少（0 = 灰盒的位置；和灰盒逐点比的测试用）
	FAutoConsoleCommandWithWorldAndArgs GDysisSwanShift(TEXT("Dysis.SwanShift"), TEXT("四层的天鹅像：Dysis.SwanShift <往墙那边挪几厘米>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1) return;
			for (TActorIterator<ADysisDirector> It(World); It; ++It) It->DebugMoveSwan(FCString::Atof(*Args[0]));
		}));
	// 瀑布后面的女神像（2026-10-10 用户：没在瀑布里的正中，要挪到正中、连底座往墙那边靠；怀里的月亮、底座都要正圆）：
	//   · 模型里她怀里那面圆盘的圆心在身子的轴线右边 63.8 cm、前面 48.5 cm、离脚 395.3 cm；灰盒判定月光的那一点在“右 35.1、前 96.8、高 381.5”。
	//     模型相对原来的部件往殿心挪 48.3、往左挪 28.7、往下沉 13.8：圆盘的圆心就正好是那一点（月光照到的就是她怀里的月亮）；
	//   · 整件（像、挡人的部分、判定月光的点一起）从灰盒的位置绕殿心转 MpGodShiftAz 度、往墙那边挪 MpGodShiftR 厘米。
	//     用户想要的是瀑布正中（方位 128°，要转 −6.5°）。在游戏里把月桥上每一步、三相像的八个格都量了一遍（Dysis.GoddessShift + DebugGoddessShift）：
	//     放到正中的话她怀里的月亮五个点最多照到一个，哪一格都不行；转到 −3° 还剩四个，−2.5° 以内才五个全照到。
	//     所以定在 −2°、往墙那边 68 cm：身子的轴线在方位 132.5°（原来 135.7°，往中间挪了 0.8 m）、离殿心 14.4 m。
	//     月光全照到她的时刻是 159.6–160.0，月桥上是第 58 步（灰盒原位是第 61 步）——pie_night3 按这个核。
	//   2026-10-11 用户：位置看着还是没变；脚下那两层台子换成一层小一点的；要是照不到了可以再放大一点。
	//   → 模型不带台座了（转换时去掉），脚下另放一个圆台（从水面以下一直到脚底）。模型按“怀里圆盘的圆心 = 判定月光的那一点”来摆：
	//     圆盘的圆心在身子的轴线右边 63.8、前面 48.5、离脚 339.1 厘米（都乘放大的倍数）。
	//   → 这一回是把瀑布后面那片空间逐点量了（DebugMoonLitAt：方位 124–139° × 半径 12.6–15.2 m × 高 1–6.4 m，月桥上每一步）：
	//     人在月桥上越往下走，三相像反过来的那束月光照得越低、越靠瀑布中间。五个点能全照到的地方，最靠中间到方位 131°——
	//     但那里只能在离地 3.4 m 高（原来 4.7 m）。圆盘在人的右手边 2.5°，所以人的轴线能到 128.5°（瀑布正中 128°，只差 14 cm），
	//     代价是整个人比原来低 1.3 m：脚落到和水庭的地面一样高（脚下的圆台只高出地面几厘米），头顶离天花板一米半。
	//     放大没有用（圆盘会更高、更偏），所以没放大。月光全照到她是在月桥的第 67–68 步（原来 58，灰盒 61）。
	//     圆盘的圆心：方位 131.0°、离殿心 14.4 m、高 340（灰盒 137.2° / 13.24 m / 471.5）。
	//     （2026-10-11 重新转过一遍——衣褶细了——圆盘的位置跟着差了不到 1 cm：右 64.6、前 48.2、离脚 338.4）
	constexpr float MpGodDiscRightCm = 64.6f, MpGodDiscFwdCm = 48.2f, MpGodDiscUpCm = 338.4f, MpGodBaseRadiusCm = 122.0f;
	constexpr float MpGodShiftAz = -6.2f, MpGodShiftR = 116.4f, MpGodShiftZ = -131.5f, MpGodScale = 1.0f;
	// 四层的天鹅像往墙那边挪多少（见 DebugMoveSwan）：台座原来压在栏杆上，天鹅转到朝殿心的那几格时身子还穿过栏杆（天鹅身长的一半有 80 cm）
	constexpr float MpSwanShiftCm = 45.0f;
	UStaticMesh* MpMesh(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Game/Dysis/MechPack_20261010/SM_MP_%s.SM_MP_%s"), Name, Name);
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M) UE_LOG(LogTemp, Warning, TEXT("Dysis 机关模型：找不到 %s（跑一遍 Art/Models/mech-pack-20261010/ue_import_mech_pack.py）"), *Path);
		return M;
	}
	enum class EMpLook : uint8 { Stone, AllStone, AsIs };      // 石头换成墙色、点缀留着 / 全当石头 / 颜色全照模型自己的

	// 三相像（2026-10-10 用户：太小了，要大一些；圆底座缩小、不要和旁边的手轮穿在一起；像站在底座正中）：
	// 模型原来做成 1.5 m 高，放大到 1.4 倍（2.1 m）；圆底座半径 1.72 → 1.07 m、高 0.9 → 0.4 m（看得见的、挡人的都是；挡光的还是原来那个大的，光路不变）。
	// 镜子的中心在身前 27 cm、离脚 1.48 m：底座矮了半米以后，它离地的高度正好还是灰盒里反光出去的那个高度（底座面上 0.95 m → 离三层地面 1.85 m）。
	// 2026-10-11 用户（他说的“抱镜女神像”就是三相像）：脚下那两层台子换成一层——只留上面那一层、再小一点；这样像矮下去了，
	// 镜子要是够不着光就把像再放大一点；镜面要平整的石面、不要花纹（转换时抹平了）。
	// ——灰盒的圆台是两层（下层半径 1.72 m、上层 1.25 m，各 45 cm 高）。现在只画一层：半径 70 cm、高 20 cm（挡人的也是这么大；挡光的还是原来那个，光路不变）。
	// 像比原来矮了 20 cm，放大到 1.567 倍（2.35 m 高）：镜子的中心离脚 105.7 × 1.567 = 165.6 cm，加上 20 cm 的台子还是离地 1.85 m——灰盒里反光出去的那个高度。
	constexpr float MpMirrorScale = 1.567f, MpPlinthRadius = 70.0f, MpPlinthH = 20.0f, MpPlinthWasH = 90.0f;
	// 立像的棱角软一点（2026-10-11 用户：三相像、双子、抱月女神的棱角不要那么分明，尤其披风、衣服）：
	// 材质的 Facet = 1 是纯按面着色（一块面一个明暗，棱最硬），0 是全用模型的平滑法线；这几尊用 0.45——面还分得出来，棱是软的
	constexpr float MpSoftFacet = 0.45f;
	// 波吕丢刻斯缩到 0.9（龛只有 1.36 m 宽、进深不到 1.3 m，原大小的圆台座顶着龛的后壁）
	constexpr float MpPolluxScale = 0.90f;
	// 水闸的开关（换成拉杆的模型）：多大、推倒多少度
	constexpr float MpSluiceLeverScale = 0.62f, MpSluiceTiltDeg = 68.0f;
	// 日之龛：盒子的中心比原来靠墙 12 cm（盖子朝着人掀开、竖起来时不蹭墙）；盖子最多掀 80°
	constexpr float MpNicheBackCm = 12.0f, MpNicheTopCm = 43.0f, MpNicheLidHalfCm = 31.0f, MpNicheOpenRad = 1.40f;
	// 拉杆：缩到 0.62、装到墙上——底座的底面贴着墙（嵌进去 1.5 cm），杆从墙上伸出来、上下扳；离地 1.05 m。模型里转轴在底座的底面上面 37.2 cm
	constexpr float MpLeverScale = 0.62f, MpLeverUpCm = 105.0f, MpLeverPivotCm = 37.2f, MpLeverSinkCm = 1.5f;
	// 外立面神像：去了底座以后人直接站在窗台上（窗台面比部件的原点高 15 cm），往墙里收：衣摆的后沿嵌进墙里 12 cm
	constexpr float MpFacadeBackCm = 56.0f, MpFacadeInsetCm = 12.0f, MpFacadeSillCm = 15.0f;
	// 2026-10-11 用户：外面的装饰小神像再缩小一点、放在各自窗洞（盲拱）的正中；窗洞是开着的那一间不放。
	// 像高 302 cm × 倍数。各层盲拱从窗台到拱顶多高（灰盒 FACADE：起拱高 − 窗台 + 半个拱宽）：二层 4.25 m、三层 4.6 m、四层 4.15 m。
	// 像悬在拱的正中偏下一点（脚离窗台 = 空余高度的 45%），脚下垫一块小托石（不然像飘着）
	constexpr float MpFacadeModelH = 302.0f, MpFacadeScaleL1 = 0.70f, MpFacadeScaleL2 = 0.66f, MpFacadeScaleL3 = 0.60f;
	constexpr float MpFacadeNicheL1 = 425.0f, MpFacadeNicheL2 = 460.0f, MpFacadeNicheL3 = 415.0f;
	constexpr bool MpAppleHalo = false;
	// 虹之龛（三层虹的窗旁边墙上那个带两扇小门的龛）。灰盒里它只有 5 cm 深，碎片有 22 cm 大——关着门的时候碎片穿在门和背板外面
	// （2026-10-10 用户：龛里挖一点空间出来放碎片）。墙是实心的挖不进去，改成往外做：四面加上龛壁，做成一个 28 cm 深的小柜子，
	// 两扇门挪到柜子的前口上，碎片放在柜子正中。
	constexpr float MpIrisNicheDeepCm = 28.0f, MpIrisNicheWallCm = 2.5f;
	constexpr float MpIrisHoleHalfW = 29.0f, MpIrisHoleHalfH = 34.0f;      // 墙上那个洞：两扇门合起来 60 × 68，洞比它四周各小 1 cm（门把洞口盖严）
	// 伊莉丝浮雕下面那个空着的人形：头的中心（灰盒 RbwHead，方位 79.0°、离二层地面 1.268 m）、头的半径；被光照着时浮雕压暗到多少
	constexpr float MpSpotAz = 79.0f, MpSpotHeadZ = 726.765f, MpSpotHeadR = 15.5f, MpSpotFootZ = 603.0f, MpIrisLitDim = 0.40f;
	// 塞勒涅的浮雕：她的眼睛（模型里那一小粒青金石）在浮雕中线左边 15.0 cm、离底边 206.9 cm。靛色的光要落的那一点是灰盒定死的，浮雕按它来摆
	// （2026-10-10 用户：浮雕放大一点、让它的底落到地上，眼睛的位置不变 → 模型放大到 1.393 倍，下面两个数跟着乘：原来是 −15.0、206.9）
	const FVector MpSeleneEye(1545.268, -133.845, 268.04);
	constexpr float MpSeleneEyeX = -15.0f * 1.393f, MpSeleneEyeZ = 206.9f * 1.393f;
	// 浮雕贴墙：背板的面比墙面只凸出这么多（厘米）
	constexpr float MpReliefProud = 0.3f;
	// 屋顶细桥：桥头在哪、朝哪（在关卡里量的：桥面宽 108 cm、长 690 cm、顶面高 3045）；桥门：拱洞中线离门轴 58 cm
	const FVector MpBridgeHead(-790.1 - 38.0 * 0.6788, -750.9 - 38.0 * 0.7343, 3045.0);
	constexpr float MpBridgeYaw = 47.25f, MpBridgeLenCm = 690.0f, MpBridgeWideCm = 124.0f;
	// 桥门（2026-10-11 第二回）：拱立在脚下那块不动的平台正中——平台占方位 219.7°–227.66°（一边是夜里会沉下去的楼梯，一边是白天会升起来的踏步），
	// 拱心放在 223.68°、离殿心 11.6 m（最靠里的面 11.43 m，叶片盘的铜边到 11.18 m）。门板挂在拱朝外的那个面上（半个进深 17 + 半个板厚 3.5）。
	constexpr float MpGateCenterCm = 58.0f, MpGateFootZ = 3030.0f, MpGateAz = 223.68f, MpGateR = 1160.0f, MpGateHalfDeepCm = 17.0f, MpGateDoorHalfCm = 3.5f, MpBridgeMoreCm = 62.0f;
	// 水亭的浑天仪：中心、太阳在多大的圈上（灰盒 R = 0.52 m）、月托在多大的圈上（0.92 R）；太阳的直径（= 金苹果的高）；月托的碟口比苹果的中心低多少
	const FVector MpPavArm(0.0, 0.0, 180.0);
	constexpr float MpPavSunR = 52.0f, MpPavMoonR = 47.84f, MpPavSunCm = 14.0f, MpHolderBelowCm = 4.2f;
	// 二层外立面也放神像（2026-10-11 用户：隔一个窗洞放一个，不然玩家基本看不见它们）：和三层一样放在单数的开间（22.5° + 30°k）里，
	// 正门上面那一间（22.5°）是真的窗洞，不放。窗台高 8.2 m（灰盒 FACADE[1].sill），像缩到 0.8（高 2.4 m；这一层的盲拱高 4.4 m）
	constexpr float MpFacadeL1SillZ = 820.0f, MpFacadeL1R = 1712.0f;
	// （142.5° 那一间上半截开着墙里楼梯的窗，像会挡在窗口前面，不放；172.5° 那一间是真的窗洞（开着的），也不放——2026-10-11 用户）
	constexpr float MpFacadeL1Az[] = { 52.5f, 82.5f, 112.5f, 202.5f, 232.5f, 262.5f, 292.5f, 322.5f, 352.5f };
}

void ADysisDirector::SetupMechPack()
{
	for (UStaticMeshComponent* C : MechPackComps) if (C) C->DestroyComponent();
	MechPackComps.Reset(); MechPackReplaced = 0;
	for (TWeakObjectPtr<UStaticMeshComponent>& F : MirrorForms) F = nullptr;
	SeleneEyeGlow = nullptr;

	// 原件不画了：它身上现在所有看得见的网格（原模型、退到和墙齐平的那一份、低模）。碰撞和逻辑照旧
	auto HideLook = [](AActor* A)
	{
		if (!A) return;
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps) if (C) C->SetVisibility(false);
	};
	// 往 On 这个部件身上挂一件新模型（只管看；空 = 挂在机关总管自己身上）。Loc / YawDeg / Scale 是它在世界里摆在哪、网格的 +X 朝哪、多大——
	// 按“那个部件现在没缩放”来算相对位置（天鹅开局是缩小着的）；以后它怎么动新模型跟着动
	auto Put = [this](AActor* On, const TCHAR* MeshName, const FVector& Loc, float YawDeg, const FVector& Scale, EMpLook Look) -> UStaticMeshComponent*
	{
		UStaticMesh* Mesh = MpMesh(MeshName);
		if (!Mesh) return nullptr;
		AActor* Host = On ? On : this;
		USceneComponent* Parent = Host->GetRootComponent();
		if (!Parent) return nullptr;
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Host);
		C->SetupAttachment(Parent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		const FTransform Unscaled(Parent->GetComponentQuat(), Parent->GetComponentLocation(), FVector::OneVector);
		C->SetRelativeTransform(FTransform(FRotator(0.0f, YawDeg, 0.0f), Loc, Scale).GetRelativeTransform(Unscaled));
		if (Look != EMpLook::Stone)
			if (UMaterialInstanceDynamic* MID = C->CreateDynamicMaterialInstance(0))
			{
				if (Look == EMpLook::AsIs) MID->SetScalarParameterValue(TEXT("StoneMix"), 0.0f);
				else { MID->SetScalarParameterValue(TEXT("SatLo"), 2.0f); MID->SetScalarParameterValue(TEXT("SatHi"), 3.0f); }
			}
		MechPackComps.Add(C);
		++MechPackReplaced;
		return C;
	};
	auto AzOfActor = [](const AActor* A) { return DysisGB::AzOf(A->GetActorLocation()); };
	const FVector One = FVector::OneVector;
	// 棱角软一点的那几尊立像（见 MpSoftFacet）
	auto Soft = [](UStaticMeshComponent* C)
	{
		if (!C) return;
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
		if (!MID) MID = C->CreateDynamicMaterialInstance(0);
		if (MID) MID->SetScalarParameterValue(TEXT("Facet"), MpSoftFacet);
	};

	// ── 三相像：三个姿势各一件，哪一相就画哪一件（UpdateMechPack 里切）；原来的像和那面铜镜不画了 ──
	if (AActor* S = MirrorStatue.Get())
	{
		HideLook(S); HideLook(MirrorPlate.Get());
		// 圆底座：半径缩小（连挡人的一起缩，手轮就不嵌在它边上了）；看得见的另挂一份矮的
		float Drop = 0.0f;
		if (AActor* Pl = Piece(TEXT("SM_Mech_Mirror_Plinth")))
			if (UStaticMeshComponent* Src = Pl->FindComponentByClass<UStaticMeshComponent>())
			{
				// 原来那个大圆台（挡光的）一点不动——头一版把它连碰撞一起缩小，三层那束窗光（c）有三十几个时刻就和灰盒对不上了（它本来挡着一截）。
				// 它不再挡人、挡镜头；挡人的另放一块和小底座一样大的看不见的方块。
				Src->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
				Src->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
				if (UBoxComponent* Wall = MakeWall())
				{
					const float Half = MpPlinthRadius * 0.9f;
					Wall->SetBoxExtent(FVector(Half, Half, MpPlinthH * 0.5f));
					Wall->SetWorldLocationAndRotation(Pl->GetActorLocation() + FVector(0.0, 0.0, MpPlinthH * 0.5f), FRotator(0.0f, AzOfActor(Pl), 0.0f));
				}
				HideLook(Pl);
				// 看得见的台子：只有一层的小圆台（引擎的圆柱：直径 100、高 100）
				if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
				{
					UStaticMeshComponent* V = NewObject<UStaticMeshComponent>(Pl);
					V->SetupAttachment(Src);
					V->SetMobility(EComponentMobility::Movable);
					V->SetStaticMesh(Cyl);
					V->SetMaterial(0, Src->GetMaterial(0));
					V->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
					V->RegisterComponent();
					V->SetWorldTransform(FTransform(FQuat::Identity, Pl->GetActorLocation() + FVector(0.0, 0.0, MpPlinthH * 0.5f), FVector(MpPlinthRadius / 50.0, MpPlinthRadius / 50.0, MpPlinthH / 100.0)));
					MechPackComps.Add(V);
				}
				Drop = MpPlinthWasH - MpPlinthH;
			}
		const FVector Loc = S->GetActorLocation() - FVector(0.0, 0.0, Drop);      // 站在底座正中（转的时候绕这根轴转）
		const TCHAR* Names[3] = { TEXT("MirrorDark"), TEXT("MirrorSun"), TEXT("MirrorMoon") };      // 和 MirrorW3 一个顺序：暗、日、月
		for (int32 i = 0; i < 3; ++i) { MirrorForms[i] = Put(S, Names[i], Loc, MirrorYawNow - 90.0f, FVector(MpMirrorScale), EMpLook::Stone); Soft(MirrorForms[i].Get()); }
	}

	// ── 瀑布后面的女神、双子：都朝殿心站 ──
	if (AActor* G = Piece(TEXT("SM_Mech_Goddess_Statue")))
	{
		DebugMoveGoddess(MpGodShiftAz, MpGodShiftR, MpGodShiftZ);
		HideLook(G); HideLook(Piece(TEXT("SM_Mech_Goddess_Plinth")));      // 原来那根方墩子不画了
		// 怀里那面圆盘的圆心 = 判定月光的五个点里正中的那个。人朝着殿心站：轴线在哪决定了朝向，朝向又决定了圆盘在哪——来回算三遍就定住了
		const FVector Disc = GodPoint(GDysisGoddessSamples[0]);
		FVector Axis = Disc;
		float Az = float(DysisGB::AzOf(Disc));
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Out = DysisGB::PolarCm(Az, 1.0, 0.0), Tan(-Out.Y, Out.X, 0.0);
			Axis = Disc - Tan * (MpGodDiscRightCm * MpGodScale) + Out * (MpGodDiscFwdCm * MpGodScale);
			Az = float(DysisGB::AzOf(Axis));
		}
		Axis.Z = Disc.Z - MpGodDiscUpCm * MpGodScale;      // 脚底
		UStaticMeshComponent* Body = Put(G, TEXT("Goddess"), Axis, Az + 90.0f, FVector(MpGodScale), EMpLook::Stone);
		Soft(Body);
		// 脚下的圆台：只有一层，比衣摆大一圈，从水面以下一直到脚底（脚踩进去 3 cm）
		if (UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
			if (USceneComponent* Root = G->GetRootComponent())
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(G);
				C->SetupAttachment(Root);
				C->SetMobility(EComponentMobility::Movable);
				C->SetStaticMesh(Cyl);
				if (Body) C->SetMaterial(0, Body->GetMaterial(0));
				C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->RegisterComponent();
				const double Top = Axis.Z + 3.0, Bottom = -70.0, R = MpGodBaseRadiusCm * MpGodScale;
				C->SetWorldTransform(FTransform(FQuat::Identity, FVector(Axis.X, Axis.Y, (Top + Bottom) * 0.5), FVector(R / 50.0, R / 50.0, (Top - Bottom) / 100.0)));
				MechPackComps.Add(C);
			}
	}
	if (AActor* A = Castor.Get()) { HideLook(A); Soft(Put(A, TEXT("TwinCastor"), A->GetActorLocation(), AzOfActor(A) + 90.0f, One, EMpLook::Stone)); }
	if (AActor* A = Pollux.Get()) { HideLook(A); PolluxModel = Put(A, TEXT("TwinPollux"), A->GetActorLocation(), AzOfActor(A) + 90.0f, FVector(MpPolluxScale), EMpLook::Stone); Soft(PolluxModel.Get()); }

	// ── 天鹅和它的圆台座（女神像没有新模型，还是原来那个；月光照到她变天鹅的时候两件一个缩、一个长，新天鹅挂在原来的天鹅身上跟着长） ──
	DebugMoveSwan(MpSwanShiftCm);
	if (AActor* P = Piece(TEXT("SM_Mech_Swan_Plinth"))) { HideLook(P); Put(P, TEXT("SwanPlinth"), P->GetActorLocation(), AzOfActor(P) + 90.0f, FVector(0.92, 0.92, 1.0), EMpLook::Stone); }
	// 变成天鹅以前的那尊女神像（2026-10-11 美术补的模型）：站在同一个圆台座上，脸朝着天鹅头朝的那一边；月光照到她的时候跟着原件一起缩下去
	if (AActor* Gs = SwanGoddess.Get())
		if (MpMesh(TEXT("SwanGoddess")))
		{
			HideLook(Gs);
			Soft(Put(Gs, TEXT("SwanGoddess"), Gs->GetActorLocation(), SwanYawNow - 90.0f, One, EMpLook::Stone));
		}
	if (AActor* B = SwanBird.Get())
	{
		// 模型里天鹅的头朝 −X；游戏里头朝 SwanYawNow 那个方向（影子按这个算）。放大到 1.28 倍：头正好在灰盒说的“往前 58 cm、高 138 cm”附近
		HideLook(B);
		Put(B, TEXT("SwanBird"), B->GetActorLocation(), SwanYawNow + 180.0f, FVector(1.28), EMpLook::Stone);
	}

	// ── 三块浮雕：贴在内墙面上（背板 = 墙面），原来的不画了 ──
	auto Relief = [&](AActor* On, const TCHAR* MeshName, float Az, float BottomZ)
	{
		Put(On, MeshName, DysisGB::PolarCm(Az, DysisGB::R_IN - MpReliefProud, BottomZ), Az + 90.0f, One, EMpLook::Stone);
	};
	if (AActor* A = Piece(TEXT("SM_Mech_Selene_Relief")))
	{
		HideLook(A);
		const float Az = DysisGB::AzOf(MpSeleneEye) - FMath::RadiansToDegrees(MpSeleneEyeX / DysisGB::R_IN);
		Relief(A, TEXT("Selene"), Az, float(MpSeleneEye.Z) - MpSeleneEyeZ);
		// 她醒过来的时候眼睛亮起来：原来是把浮雕上眼睛那一格材质换成发光的，新模型的眼睛只是一小粒顶点色——在那一点上另放一粒会亮的
		if (UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
			if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisGlow.M_DysisGlow")))
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
				C->SetupAttachment(RootComponent);
				C->SetMobility(EComponentMobility::Movable);
				C->SetStaticMesh(Ball);
				C->SetCastShadow(false);
				C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->RegisterComponent();
				C->SetWorldLocation(MpSeleneEye - MpSeleneEye.GetSafeNormal2D() * 12.0);
				C->SetWorldScale3D(FVector(0.075));
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Glow, this);
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(FColor(0x3a, 0x4c, 0xff)));
				MID->SetScalarParameterValue(TEXT("Intensity"), 2.4f);
				MID->SetScalarParameterValue(TEXT("Edge"), 0.0f);
				C->SetMaterial(0, MID);
				C->SetVisibility(false);
				MechPackComps.Add(C);
				SeleneEyeGlow = C;
			}
	}
	if (AActor* A = Piece(TEXT("SM_Mech_IrisRelief_Relief")))
	{
		// 原来的浮雕 2.35 m 宽、2.7 m 高，中心在方位 78.7°、高 740；新的 2.34 × 2.21 m，放在同一个中心上。旁边那道刻出来的弓和上沿的铜檐是配原浮雕的，一起不画了
		HideLook(A); HideLook(Piece(TEXT("SM_Mech_IrisRelief_CarvedBow"))); HideLook(Piece(TEXT("SM_Mech_IrisRelief_Lip")));
		// 整块都是石头色（2026-10-10 用户：浮雕里彩虹的颜色也去掉）
		// （2026-10-11：整块放大到 1.15 倍、彩虹补全了——模型从离原点 14 cm 高的地方开始、到 251 cm，中心还是放在高 740 上）
		if (UStaticMeshComponent* C = Put(A, TEXT("IrisRelief"), DysisGB::PolarCm(78.7f, DysisGB::R_IN - MpReliefProud, 740.0f - 132.5f), 78.7f + 90.0f, One, EMpLook::AllStone))
		{
			IrisReliefModel = C;
			IrisReliefMID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
		}
		// 原来浮雕上那一圈“影子靠近了就显出虹的颜色”的提示是另铺在浮雕面前的一片（按旧浮雕的弓摆的），压在新浮雕上是歪的——不画了；
		// 提示改成下面那个空着的人形自己亮起来（UpdateMechPack）
		if (BandsPlane) BandsPlane->SetVisibility(false);
		// 浮雕下面那个空着的人形（“让影子的头落进去”的地方，原来是旧浮雕石板上挖出来的，换模型的时候跟着没了——用户发现的）：
		// 在墙面上补一个颜色深一档的人形，头的位置就是判定用的那一点
		if (ShadowSpot) { ShadowSpot->DestroyComponent(); ShadowSpot = nullptr; }
		if (UMaterialInterface* Warm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_WarmWhite.MI_MATCH_WarmWhite"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			TArray<FVector> V, N;
			TArray<int32> I;
			const float R = DysisGB::R_IN - 0.45f;
			const FVector Nrm = -DysisGB::PolarCm(MpSpotAz, 1.0, 0.0);
			auto At = [R](float U, float Z) { return DysisGB::PolarCm(MpSpotAz + FMath::RadiansToDegrees(U / R), R, Z); };      // U = 顺着墙往方位增大的方向多少厘米
			auto Fan = [&](const TArray<FVector2D>& Poly)      // 一个凸多边形：从第一个点扇开
			{
				const int32 k = V.Num();
				for (const FVector2D& P : Poly) { V.Add(At(float(P.X), float(P.Y))); N.Add(Nrm); }
				for (int32 i = 1; i + 1 < Poly.Num(); ++i)
				{
					// 面朝殿心：照法线定顶点的顺序（引擎里正面是叉积的反方向）
					const bool bFlip = FVector::DotProduct(FVector::CrossProduct(V[k + i] - V[k], V[k + i + 1] - V[k]), Nrm) > 0.0;
					if (bFlip) I.Append({ k, k + i + 1, k + i }); else I.Append({ k, k + i, k + i + 1 });
				}
			};
			// 身子：肩窄脚宽的一块；头：一个圆
			Fan({ FVector2D(-27.0, MpSpotFootZ), FVector2D(27.0, MpSpotFootZ), FVector2D(11.0, MpSpotHeadZ - 12.0), FVector2D(-11.0, MpSpotHeadZ - 12.0) });
			TArray<FVector2D> Head;
			for (int32 i = 0; i < 20; ++i) Head.Add(FVector2D(MpSpotHeadR * FMath::Cos(2.0 * PI * i / 20.0), MpSpotHeadZ + MpSpotHeadR * FMath::Sin(2.0 * PI * i / 20.0)));
			Fan(Head);
			ShadowSpot = NewObject<UProceduralMeshComponent>(this);
			ShadowSpot->SetupAttachment(RootComponent);
			ShadowSpot->SetMobility(EComponentMobility::Movable);
			ShadowSpot->bUseAsyncCooking = false;
			ShadowSpot->SetCastShadow(false);
			ShadowSpot->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ShadowSpot->RegisterComponent();
			ShadowSpot->SetWorldLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
			ShadowSpot->CreateMeshSection_LinearColor(0, V, I, N, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
			UMaterialInstanceDynamic* Dark = UMaterialInstanceDynamic::Create(Warm, this);
			Dark->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.43f, 0.35f, 0.26f));      // 墙色的六成：像墙上浅浅挖下去的一个人形
			ShadowSpot->SetMaterial(0, Dark);
			// 外面一小圈金边（2026-10-11 用户）：比人形大 1.8 cm 的同一个形状，垫在它后面（更贴墙 2.5 mm），露出来的就是一圈边
			{
				TArray<FVector> V2, N2;
				TArray<int32> I2;
				const float R2 = DysisGB::R_IN - 0.2f, Grow = 1.8f;
				auto At2 = [R2](float U, float Z) { return DysisGB::PolarCm(MpSpotAz + FMath::RadiansToDegrees(U / R2), R2, Z); };
				auto Fan2 = [&](const TArray<FVector2D>& Poly)
				{
					const int32 k = V2.Num();
					for (const FVector2D& P : Poly) { V2.Add(At2(float(P.X), float(P.Y))); N2.Add(Nrm); }
					for (int32 i = 1; i + 1 < Poly.Num(); ++i)
					{
						const bool bFlip = FVector::DotProduct(FVector::CrossProduct(V2[k + i] - V2[k], V2[k + i + 1] - V2[k]), Nrm) > 0.0;
						if (bFlip) I2.Append({ k, k + i + 1, k + i }); else I2.Append({ k, k + i, k + i + 1 });
					}
				};
				Fan2({ FVector2D(-27.0 - Grow, MpSpotFootZ - 1.0), FVector2D(27.0 + Grow, MpSpotFootZ - 1.0), FVector2D(11.0 + Grow, MpSpotHeadZ - 12.0), FVector2D(-11.0 - Grow, MpSpotHeadZ - 12.0) });
				TArray<FVector2D> Head2;
				for (int32 i = 0; i < 24; ++i) Head2.Add(FVector2D((MpSpotHeadR + Grow) * FMath::Cos(2.0 * PI * i / 24.0), MpSpotHeadZ + (MpSpotHeadR + Grow) * FMath::Sin(2.0 * PI * i / 24.0)));
				Fan2(Head2);
				ShadowSpot->CreateMeshSection_LinearColor(1, V2, I2, N2, TArray<FVector2D>(), TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
				if (UMaterialInterface* Gold = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Temple_v0_12/M_gold.M_gold"), nullptr, LOAD_NoWarn | LOAD_Quiet)) ShadowSpot->SetMaterial(1, Gold);
			}
		}
	}
	if (AActor* A = SwanRelief.Get())
	{
		// 四只小天鹅（两对，中间空着一只的位置）。它挂在原来的浮雕身上，解开以后一起沉下去。
		// 2026-10-10 用户：天鹅像的影子和浮雕一高一低。——判定用的是“天鹅头的影子落到灰盒定的那一点”（方位 257.57°、离四层地面 0.95 m）：
		// 浮雕原来摆得比这一点高半米多（影子的头只到浮雕上天鹅的肚子）、中间的空位也偏右 46 cm。现在往下、往左挪：
		// 中间的空位正对那一点，浮雕上天鹅的头和影子的头一样高（天鹅的身子落到地面上，像浮在水面）。
		HideLook(A);
		// 2026-10-11 用户：浮雕再往右一点，让天鹅的影子落在空位的正中，现在有点歪。——判定用的那一点是“头顶”的影子；
		// 看得见的影子是头连着喙的一整块（喙比头顶还往前伸 30 cm），它的正中比那一点偏右。浮雕往右（方位增大）挪 0.35°（9.5 cm）
		Relief(A, TEXT("SwanRelief"), 257.74f, 2300.0f - 40.0f);
	}

	// ── 月亮浮雕 → 月门：门板填在门洞里（和墙一个颜色）、银边的门框压在洞口外面、月牙在门板上。月光照够了它们跟着原来的两件一起隐去 ──
	{
		AActor* Block = Piece(TEXT("SM_Mech_MoonRelief_Block")); AActor* Disk = Piece(TEXT("SM_Mech_MoonRelief_Disk"));
		const float Az = 178.7f;
		const FVector Loc = DysisGB::PolarCm(Az, DysisGB::R_IN - 0.2f, 1450.0f);
		if (Block) { HideLook(Block); Put(Block, TEXT("MoonGateSlab"), Loc, Az + 90.0f, One, EMpLook::AllStone); Put(Block, TEXT("MoonGateFrame"), Loc, Az + 90.0f, One, EMpLook::AllStone); }
		// 月牙在模型里是嵌在门板里面的（比门板的面还靠里 2–5 cm），门板换成墙色的实心板以后它就被盖住了（2026-10-10 用户：月门上的月亮不见了）——往外挪 6.5 cm，浮在门板面上
		// 颜色和门一样（用户后来要的）：全按墙的石头色，不留模型自己的银色
		// 2026-10-11 用户：月亮的弧度再圆滑一点，现在外弧线有点怪。——美术那枚月牙是几块大平面拼的；换成按尺寸现做的（两段圆弧围成，
		// 贴在门板面上凸出 2.2 cm，见 make_extra_shapes.py），朝向、大小照原来的。弧面用平滑的法线（Facet = 0）
		if (Disk)
		{
			HideLook(Disk);
			if (UStaticMeshComponent* C = Put(Disk, TEXT("MoonGateCrescent"), Loc, Az + 90.0f, One, EMpLook::AllStone))
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0))) MID->SetScalarParameterValue(TEXT("Facet"), 0.0f);
		}
	}

	// ── 日之龛：盒和盖。新的比原来大（0.8 m 宽），盒往墙那边挪了 17 cm，让盖的轴还在盒朝殿心的那条边上 ──
	if (AActor* Box = Piece(TEXT("SM_Mech_SunNiche_Box")))
	{
		// 2026-10-10 用户（先后三条）：朝着墙开的，翻过来；美术那个哥特式的盒子面数太多、太突兀，换回原来的容器，只是要把里面挖空、注意开盖的方向。
		// ——所以这里不用美术的模型了：盒子还是灰盒那个尺寸和材质，但换成“四面壁 + 底”的空盒子（原来是一整块实心的）；
		// 盖子还是原来那块板，轴改在靠墙的那条边上、朝着人掀开（灰盒里轴在靠人的那条边上，掀起来挡在人和盒子中间）。
		// 整个往中庭挪 10 cm：盖子竖起来的时候不蹭墙。
		const float Az = AzOfActor(Box);
		const FVector Out = DysisGB::PolarCm(Az, 1.0, 0.0), Shift = -Out * 10.0;
		UStaticMeshComponent* Src = Box->FindComponentByClass<UStaticMeshComponent>();
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (Src && Src->GetStaticMesh() && Cube)
		{
			// 2026-10-11 用户：盒子的高可以矮一点，现在看着不协调。——原来 50 cm 见方、38 cm 进深，像个立方块；矮到 32 cm（底不动，碎片 16 cm 高，放得下）
			const FBox B = Src->GetStaticMesh()->GetBoundingBox();
			const FVector C00 = B.GetCenter(), E0 = B.GetExtent();
			// 盖子落在盒口上（灰盒里盖子比盒口低 2 cm、和盒壁叠着，前沿那一条两个面抢着画，会花）
			const double NicheH = 32.0, LidDrop = 2.0 * E0.Z - NicheH - 2.0;
			const FVector C0(C00.X, C00.Y, C00.Z - E0.Z + NicheH * 0.5), E(E0.X, E0.Y, NicheH * 0.5);
			UMaterialInterface* Mat = Src->GetMaterial(0);
			const FTransform SrcT = Src->GetComponentTransform();
			HideLook(Box);
			const double T = 5.0;      // 壁厚
			const FVector At[5] = { FVector(C0.X - E.X + T * 0.5, C0.Y, C0.Z), FVector(C0.X + E.X - T * 0.5, C0.Y, C0.Z), FVector(C0.X, C0.Y - E.Y + T * 0.5, C0.Z), FVector(C0.X, C0.Y + E.Y - T * 0.5, C0.Z),
				FVector(C0.X, C0.Y, C0.Z - E.Z + T * 0.5) };
			const FVector Size[5] = { FVector(T, 2.0 * E.Y, 2.0 * E.Z), FVector(T, 2.0 * E.Y, 2.0 * E.Z), FVector(2.0 * E.X - 2.0 * T, T, 2.0 * E.Z), FVector(2.0 * E.X - 2.0 * T, T, 2.0 * E.Z),
				FVector(2.0 * E.X - 2.0 * T, 2.0 * E.Y - 2.0 * T, T) };
			for (int32 i = 0; i < 5; ++i)
			{
				UStaticMeshComponent* W = NewObject<UStaticMeshComponent>(Box);
				W->SetupAttachment(Src);
				W->SetMobility(EComponentMobility::Movable);
				W->SetStaticMesh(Cube);
				if (Mat) W->SetMaterial(0, Mat);
				W->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
				W->RegisterComponent();
				W->SetWorldTransform(FTransform(SrcT.GetRotation(), SrcT.TransformPosition(At[i]) + Shift, Size[i] / 100.0));
				MechPackComps.Add(W);
			}
			++MechPackReplaced;
			// 碎片：从盒子里面、正中升起来
			const FVector Mid = SrcT.TransformPosition(C0) + Shift;
			SunShardBase = FVector(Mid.X, Mid.Y, Mid.Z - E.Z + T + 12.0);
			if (AActor* Sh = SunShard.Get()) if (SunNicheT <= 0.0f) Sh->SetActorLocation(SunShardBase);
			if (AActor* Lid = Piece(TEXT("SM_Mech_SunNiche_Lid")))
				if (UStaticMeshComponent* LSrc = Lid->FindComponentByClass<UStaticMeshComponent>())
					if (LSrc->GetStaticMesh())
					{
						UStaticMeshComponent* V = NewObject<UStaticMeshComponent>(Box);
						V->SetupAttachment(Src);
						V->SetMobility(EComponentMobility::Movable);
						V->SetStaticMesh(LSrc->GetStaticMesh());
						for (int32 i = 0; i < LSrc->GetNumMaterials(); ++i) V->SetMaterial(i, LSrc->GetMaterial(i));
						V->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
						V->RegisterComponent();
						const FTransform LT = LSrc->GetComponentTransform();
						SunLidClosedLoc = LT.GetLocation() + Shift - FVector(0.0, 0.0, LidDrop);
						SunLidClosedRot = LT.GetRotation();
						V->SetWorldTransform(FTransform(SunLidClosedRot, SunLidClosedLoc, LT.GetScale3D()));
						// 轴：盖板最靠墙、最低的那条边
						const FBox LB = LSrc->GetStaticMesh()->GetBoundingBox();
						double Far = -1.0e9, Low = 1.0e9;
						for (int32 k = 0; k < 8; ++k)
						{
							const FVector P = LT.TransformPosition(FVector(k & 1 ? LB.Max.X : LB.Min.X, k & 2 ? LB.Max.Y : LB.Min.Y, k & 4 ? LB.Max.Z : LB.Min.Z)) + Shift - FVector(0.0, 0.0, LidDrop);
							Far = FMath::Max(Far, FVector::DotProduct(P, Out)); Low = FMath::Min(Low, P.Z);
						}
						SunLidHinge = Out * Far + FVector(0.0, 0.0, Low);
						SunLidAxis = FVector(-Out.Y, Out.X, 0.0);
						HideLook(Lid);
						V->SetVisibility(true);
						SunLidModel = V;
						MechPackComps.Add(V);
					}
		}
	}

	// ── 拉杆：底座和杆（杆挂在原来的杆身上，跟着它扳；转轴比原来低 8 cm） ──
	// 2026-10-10 用户：缩小一点、放到墙上——底座贴着墙，拉杆的方向是上下的。
	// 整件放倒：模型的“上”朝殿心（底座的底面贴在墙上），两个圆鼓的轴还是顺着墙；杆从墙上平着伸出来，扳的时候绕那根轴往下压。
	// 杆不再挂在原来那根杆身上（那根的轴在地上那个位置），单放在新的轴上，扳多少跟着滑块走（UpdateMechPack）。原来立在地上的那两件不再挡人（挡光照旧）。
	if (AActor* Base = Piece(TEXT("SM_Mech_LeverA_Base")))
	{
		HideLook(Base);
		const float Az = AzOfActor(Base);
		const FVector Out = DysisGB::PolarCm(Az, 1.0, 0.0), Tan(-Out.Y, Out.X, 0.0);
		const FVector At = DysisGB::PolarCm(Az, DysisGB::R_IN + MpLeverSinkCm, Base->GetActorLocation().Z + MpLeverUpCm);
		LeverMountRot = FRotationMatrix::MakeFromZX(-Out, Tan).ToQuat();
		if (UStaticMeshComponent* C = Put(Base, TEXT("LeverBase"), At, 0.0f, FVector(MpLeverScale), EMpLook::AsIs)) C->SetWorldRotation(LeverMountRot);
		if (AActor* Arm = LeverArm.Get())
		{
			HideLook(Arm);
			LeverArmModel = Put(nullptr, TEXT("LeverArm"), At - Out * (MpLeverPivotCm * MpLeverScale), 0.0f, FVector(MpLeverScale), EMpLook::AsIs);
		}
		for (AActor* A : { Base, LeverArm.Get() })
			if (A)
			{
				TInlineComponentArray<UPrimitiveComponent*> Prims(A);
				for (UPrimitiveComponent* P : Prims) { P->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); P->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
			}
	}

	// ── 水闸的开关：原来是一根标记用的小铜柱，换成拉杆（2026-10-11 用户：水没开的时候杆立着，开了以后杆推倒到一边，之后不能再碰）──
	// 底座平放在石沿上，两片圆鼓的轴顺着半径；杆立在轴上，开闸的时候顺着石沿倒下去（UpdateMechPack）——背着瀑布、朝台阶那一边
	// （头一版朝瀑布倒；2026-10-11 用户：扳手方向掉转一下）。开过以后“按 E”的那一项本来就没了。
	SluiceArmModel = nullptr;
	if (SluiceMarker && MpMesh(TEXT("LeverBase")))
	{
		const FVector At = SluiceMarker->GetComponentLocation() - FVector(0.0, 0.0, 45.0);      // 小铜柱的脚底 = 石沿的面
		const float Az = float(DysisGB::AzOf(At));
		SluiceMarker->SetVisibility(false);
		Put(nullptr, TEXT("LeverBase"), At, Az, FVector(MpSluiceLeverScale), EMpLook::AsIs);
		SluiceArmModel = Put(nullptr, TEXT("LeverArm"), At + FVector(0.0, 0.0, MpLeverPivotCm * MpSluiceLeverScale), Az, FVector(MpSluiceLeverScale), EMpLook::AsIs);
		SluiceArmRot0 = FRotator(0.0f, Az, 0.0f).Quaternion();
		SluiceArmAxis = DysisGB::PolarCm(Az, 1.0, 0.0);
	}

	// ── 棱镜：台座挂在铜柱身上（一起从石沿里升起来），水晶和手轮挂在棱镜身上（一起转）；原来的铜缝、转盘不画了 ──
	if (PrismParts.Num() >= 4)
	{
		AActor* Column = PrismParts[0].Actor.Get(); AActor* Glass = PrismParts[1].Actor.Get();
		for (const FPrismPart& P : PrismParts) HideLook(P.Actor.Get());
		if (Column && Glass)
		{
			// 水晶的中心 = 原来棱镜的中心（色光从这里出来）：水晶高 31 cm、底在台座顶上，台座高 42 cm
			const FVector Mid = Glass->GetActorLocation();
			const float Az = DysisGB::AzOf(Mid);
			UStaticMeshComponent* Parts[3] = {
				Put(Column, TEXT("PrismBase"), Mid - FVector(0.0, 0.0, 15.5 + 42.0), Az - 90.0f, One, EMpLook::AsIs),
				Put(Glass, TEXT("PrismSpin"), Mid - FVector(0.0, 0.0, 15.5), float(Glass->GetActorRotation().Yaw), One, EMpLook::AsIs),
				Put(Glass, TEXT("PrismCrystal"), Mid - FVector(0.0, 0.0, 15.5), float(Glass->GetActorRotation().Yaw), One, EMpLook::Stone) };
			if (Parts[2]) Parts[2]->SetCastShadow(false);
			// 它是从石沿里升起来的：还在石沿面（高 17.5 m）以下的那一截不画——石沿只有 35 cm 厚，不然底下会露出一截往上钻的台座
			// （2026-10-10 用户：棱柱升起来的时候穿模了）。材质里的 ClipZ = 这个高度以下不画。
			for (UStaticMeshComponent* C : Parts)
				if (C)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0));
					if (!MID) MID = C->CreateDynamicMaterialInstance(0);
					if (MID) MID->SetScalarParameterValue(TEXT("ClipZ"), 1749.5f);
				}
		}
	}

	// ── 金苹果 ──
	if (Apple)
		if (UStaticMesh* M = MpMesh(TEXT("Apple")))
		{
			Apple->SetStaticMesh(M);
			Apple->SetRelativeScale3D(FVector(1.0));
			// 2026-10-10 用户：小巧精致一些（高 14 cm、340 个面）、金色的表面试试镜面
			if (UMaterialInterface* Gold = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisAppleGold.M_DysisAppleGold"), nullptr, LOAD_NoWarn | LOAD_Quiet)) Apple->SetMaterial(0, Gold);
			else
			{
				Apple->SetMaterial(0, M->GetMaterial(0));
				if (UMaterialInstanceDynamic* MID = Apple->CreateDynamicMaterialInstance(0)) MID->SetScalarParameterValue(TEXT("StoneMix"), 0.0f);
			}
			// 人走近了，苹果周围有一圈淡淡的虹光：套在外面的一个球，只画边缘。（做出来以后用户又说去掉——MpAppleHalo 关着；想再要就打开）
			if (MpAppleHalo && !AppleHalo)
				if (UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
					if (UMaterialInterface* Halo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisAppleHalo.M_DysisAppleHalo"), nullptr, LOAD_NoWarn | LOAD_Quiet))
					{
						AppleHalo = NewObject<UStaticMeshComponent>(this);
						AppleHalo->SetupAttachment(Apple);
						AppleHalo->SetMobility(EComponentMobility::Movable);
						AppleHalo->SetStaticMesh(Ball);
						AppleHalo->SetCastShadow(false);
						AppleHalo->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
						AppleHalo->RegisterComponent();
						AppleHalo->SetRelativeLocation(FVector::ZeroVector);
						AppleHalo->SetWorldScale3D(FVector(0.36));      // 球的直径 36 cm：比苹果大一圈
						AppleHaloMID = UMaterialInstanceDynamic::Create(Halo, this);
						AppleHalo->SetMaterial(0, AppleHaloMID);
						AppleHalo->SetVisibility(false);
					}
			++MechPackReplaced;
		}

	// ── 外立面的 20 尊神像：背贴着外墙、脸朝外。新的比原来的厚，缩到四分之三（高 2.4 m）才不探出檐口太多；四层那一圈本来就矮一点 ──
	// 一尊：On = 原来的部件（没有就挂在机关总管身上）、方位、外墙面的半径、窗台面的高、这一层盲拱从窗台到拱顶多高、缩多少
	UStaticMesh* FacadeCube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* FacadeWarm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_WarmWhite.MI_MATCH_WarmWhite"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto Facade = [&](AActor* On, float Az, float R, float SillZ, float NicheH, float S)
	{
		const float FootZ = SillZ + 0.45f * (NicheH - MpFacadeModelH * S);
		if (UStaticMeshComponent* C = Put(On, TEXT("FacadeStatue"), DysisGB::PolarCm(Az, R - MpFacadeInsetCm + MpFacadeBackCm * S, FootZ), Az - 90.0f, FVector(S), EMpLook::AllStone))
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0))) MID->SetScalarParameterValue(TEXT("Detail"), 0.0f);
		// 脚下的小托石：一块从墙里伸出来的石板（宽比像的衣摆窄一点、伸出 30 cm、厚 9 cm），像的脚踩进去 2 cm
		AActor* Host = On ? On : this;
		if (FacadeCube && Host->GetRootComponent())
		{
			UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(Host);
			K->SetupAttachment(Host->GetRootComponent());
			K->SetMobility(EComponentMobility::Movable);
			K->SetStaticMesh(FacadeCube);
			if (FacadeWarm) K->SetMaterial(0, FacadeWarm);
			K->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
			K->RegisterComponent();
			K->SetWorldTransform(FTransform(FRotator(0.0f, Az, 0.0f), DysisGB::PolarCm(Az, R + 9.0f, FootZ + 2.0f - 4.5f), FVector(0.42, 0.86 * S, 0.09)));
			MechPackComps.Add(K);
		}
	};
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tag : It->Tags)
		{
			const FString Label = Tag.ToString();
			if (!Label.StartsWith(TEXT("SM_Facade_Statue_"))) continue;
			const bool bL3 = Label.Contains(TEXT("_L3_"));
			HideLook(*It);
			// 2026-10-10 用户：和神殿一个颜色、尽量融在一起；底座撤掉，整个往殿心收一点、不要那么凸出来。——
			// 模型去掉了脚下那块方底座，衣摆的后沿嵌进墙里；颜色全按墙的暖白，不留贴图上的深浅
			Facade(*It, float(AzOfActor(*It)), float(DysisGB::ROf(It->GetActorLocation())), float(It->GetActorLocation().Z) + MpFacadeSillCm, bL3 ? MpFacadeNicheL3 : MpFacadeNicheL2, bL3 ? MpFacadeScaleL3 : MpFacadeScaleL2);
			break;
		}
	// 二层外立面：关卡里没有这一圈的部件，直接挂在机关总管身上
	for (const float Az : MpFacadeL1Az) Facade(nullptr, Az, MpFacadeL1R, MpFacadeL1SillZ + MpFacadeSillCm, MpFacadeNicheL1, MpFacadeScaleL1);

	// ── 屋顶细桥的桥门：一座石拱立在桥头，门板装在拱洞里、挂在原来那扇门身上跟着它开关；原来那扇 4.35 m 长的铜栅不画了（它还拦着） ──
	// 2026-10-11 用户：往神殿外的方向挪一点（现在和叶片穿在一起）；拱的边缘弄平滑、或者干脆就是弧面，再缩窄一点，现在看着臃肿。
	// ——美术那座拱不用了，换成按尺寸现做的素石拱（两根 22 cm 的方柱 + 半圆的拱券，进深 34 cm；见 make_extra_shapes.py）和一扇拱顶的门板，
	// 都是做好的大小（不再放大 1.75 倍）：总宽从 2.5 m 收到 1.7 m、进深从 76 cm 收到 34 cm。
	// 往外挪 14 cm：拱最靠里的面离殿心 11.3 m，叶片盘的铜边在 11.06–11.18 m；看不见的那道门还在拱的进深里面（离殿心 11.36 m）。
	if (AActor* Door = DoorLeaf.Get())
	{
		HideLook(Door);
		const FVector Dir = Door->GetActorRightVector();                                  // 关着的时候门扇顺着哪边伸出去（门轴在 Actor 的原点）
		const float Yaw = float(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
		// 第二回（见 MpGateAz 那一行的说明）：拱立在平台正中；门轴（= 原来那扇门的原点）挪到“拱洞靠门轴那一侧的边上、拱朝外的那个面外头半个板厚”——
		// 门板挂在拱的外面，朝外推开以后整扇都在拱外，不再埋在柱子里。看不见的那扇拦人的门跟着门轴一起挪了（往外 30 来厘米）
		const FVector In(-Dir.Y, Dir.X, 0.0);                                             // 垂直于门扇、朝殿心的那一边
		const FVector Loc = DysisGB::PolarCm(MpGateAz, MpGateR, MpGateFootZ);
		FVector Hinge = Loc - Dir * MpGateCenterCm - In * (MpGateHalfDeepCm + MpGateDoorHalfCm);
		Hinge.Z = Door->GetActorLocation().Z;
		Door->SetActorLocation(Hinge);
		for (UStaticMeshComponent* C : { Put(nullptr, TEXT("GateArch"), Loc, Yaw, One, EMpLook::AllStone), Put(Door, TEXT("GateDoor"), Loc, Yaw, One, EMpLook::AllStone) })
			if (UMaterialInstanceDynamic* MID = C ? Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)) : nullptr) MID->SetScalarParameterValue(TEXT("Facet"), 0.0f);
	}
	// ── 虹门（金苹果那里的拱门）：2026-10-11 用户说不要了，喷泉里的虹也不要了（DysisDirectorFx.cpp）。原来的拱不画、不挡人（挡光照旧） ──
	if (AActor* Arch = Piece(TEXT("SM_Mech_RainbowArch_Arch")))
	{
		HideLook(Arch);
		TInlineComponentArray<UPrimitiveComponent*> Prims(Arch);
		for (UPrimitiveComponent* P : Prims) { P->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); P->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore); }
	}

	// ── 两座浑天仪：环和轴换成“只有环和轴”的（原来的模型把太阳、月亮 / 月托都焊死在环上，位置是导出那一刻的） ──
	// 2026-10-11 用户：金苹果出现的位置应该是浑天仪放太阳的地方（原来模型上另有一颗大黄球，苹果飘在别处）；两座的太阳都太大，和金苹果一样大就行；
	// 殿顶那座的太阳就是金苹果，不该另有一颗球；水亭那座的月托太厚，改成薄薄的。
	// ——殿顶：环上不再有球，金苹果自己就是太阳（UpdateCatch 把它放在朝着太阳的那一侧、环的半径上）；
	//   水亭：太阳是一颗和苹果一样大的金球（直径 14 cm），月托是一片薄碟，都跟着日月走（UpdateMechPack）；最后金苹果就是飞进这片碟子里。
	UMaterialInterface* ArmGold = nullptr; UMaterialInterface* ArmSilver = nullptr;
	for (const TCHAR* Which : { TEXT("Top"), TEXT("Pav") })
	{
		AActor* A = Piece(FCString::Strcmp(Which, TEXT("Top")) == 0 ? TEXT("SM_Mech_Armillary_Top") : TEXT("SM_Mech_Armillary_Pavilion"));
		UStaticMeshComponent* Src = A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		if (!Src || Src->GetNumMaterials() < 3) continue;
		if (FCString::Strcmp(Which, TEXT("Top")) == 0)
		{
			// 殿顶这座挪到最高一级的正中（用户要的；位置见 DysisDirectorRoof.cpp 的 RoofArmTop）。它是挂在最高一级身上的，挪完照样跟着升降
			if (USceneComponent* Root = A->GetRootComponent()) Root->SetMobility(EComponentMobility::Movable);
			const FVector Want = ArmTopCm();
			FVector L = A->GetActorLocation(); L.X = Want.X; L.Y = Want.Y;
			A->SetActorLocation(L);
		}
		UMaterialInterface* Mats[3] = { Src->GetMaterial(0), Src->GetMaterial(1), Src->GetMaterial(2) };      // 铜、金、银
		static const TCHAR* const Parts[3] = { TEXT("Bronze"), TEXT("Gold"), TEXT("Silver") };
		bool bAll = true;
		for (const TCHAR* Part : Parts) if (!MpMesh(*FString::Printf(TEXT("Arm%s%s"), Which, Part))) bAll = false;
		if (!bAll) continue;
		HideLook(A);
		for (int32 i = 0; i < 3; ++i)
			if (UStaticMeshComponent* C = Put(A, *FString::Printf(TEXT("Arm%s%s"), Which, Parts[i]), A->GetActorLocation(), float(A->GetActorRotation().Yaw), One, EMpLook::Stone))
				if (Mats[i]) C->SetMaterial(0, Mats[i]);
		ArmGold = Mats[1]; ArmSilver = Mats[2];
		if (FCString::Strcmp(Which, TEXT("Top")) == 0)
		{
			// 殿顶这座脚下原来有一根只管挡人的柱子（灰盒 pc：半径 45 cm、从最高一级的面往上 2.2 m），它是算在虹门那个部件里的。
			// 虹门不挡人以后这根柱子也跟着没了：人能一直走到浑天仪底下，头顶着铜环（环本身是挡人的），最高一级就迈不上去。
			// ——铜环不再挡人（免得顶头），另放一根同样大小、只挡人的圆柱在原来的位置，挂在浑天仪身上跟着最高一级升降。
			// （要圆的：这根柱子探出最高一级的边 20 cm，人顺着环道走过来是先碰到它、从旁边滑过去再迈上台阶的；换成方的会顶在平面上过不去。）
			Src->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Src->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			if (!ArmTopBlocker.IsValid())
			{
				UCapsuleComponent* Cap = NewObject<UCapsuleComponent>(A);
				Cap->SetupAttachment(A->GetRootComponent());
				Cap->SetMobility(EComponentMobility::Movable);
				Cap->InitCapsuleSize(45.0f, 155.0f);
				Cap->SetCollisionObjectType(ECC_WorldDynamic);
				Cap->SetCollisionResponseToAllChannels(ECR_Ignore);
				Cap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);      // 只挡人：不挡光、不挡镜头
				Cap->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Cap->SetCanEverAffectNavigation(false);
				Cap->SetHiddenInGame(true);
				Cap->RegisterComponent();
				Cap->SetWorldLocation(A->GetActorLocation() - FVector(0.0, 0.0, 155.0 - 110.0));      // 圆柱那一段：从最高一级的面到上面 2.2 m（胶囊两头的圆顶多出来的埋在台阶里、伸到头顶上）
				ArmTopBlocker = Cap;
			}
		}
	}
	PavSunBead = nullptr; PavMoonHolder = nullptr;
	if (UStaticMesh* Ball = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Ball);
		if (ArmGold) C->SetMaterial(0, ArmGold);
		C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		C->SetWorldScale3D(FVector(MpPavSunCm / 100.0));
		MechPackComps.Add(C);
		PavSunBead = C;
	}
	if (UStaticMeshComponent* C = Put(nullptr, TEXT("MoonHolder"), MpPavArm, 0.0f, One, EMpLook::Stone))
	{
		if (ArmSilver) C->SetMaterial(0, ArmSilver);
		PavMoonHolder = C;
	}

	// ── 屋顶的细桥：换成“夜里女神像伸出来的那道半桥”的样子（用户要的）——一整块白石板，没有栏杆（看不见的栏杆还拦着） ──
	if (AActor* Bridge = Piece(TEXT("SM_RoofBridge")))
		if (const AActor* Half = Piece(TEXT("SM_Mech_HalfBridge_Deck")))
			if (const UStaticMeshComponent* Src = Half->FindComponentByClass<UStaticMeshComponent>())
				if (UStaticMesh* Deck = Src->GetStaticMesh())
				{
					HideLook(Bridge);
					UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Bridge);
					C->SetupAttachment(Bridge->GetRootComponent());
					C->SetMobility(EComponentMobility::Movable);
					C->SetStaticMesh(Deck);
					// 颜色：和屋顶的地面一样的暖白（原来拷的是半桥的白石头；2026-10-11 用户：拱门那片地面和别处有色差）
					UMaterialInterface* Warm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_WarmWhite.MI_MATCH_WarmWhite"), nullptr, LOAD_NoWarn | LOAD_Quiet);
					C->SetMaterial(0, Warm ? Warm : Src->GetMaterial(0));
					C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->RegisterComponent();
					// 半桥的模型：宽 150（X）、长 435（Y，从原点往前）、顶面在 Z = 20
					// （2026-10-10 用户：和屋顶那一圈台阶的叶片有点穿在一起，往上抬一点点 → 抬 6 cm）
					// 桥头跟着桥门往外多伸 30 cm（门往外挪了）：伸出来的那一截比平台的面低 1 cm，埋在平台里看不见
					const FVector Along(FMath::Cos(FMath::DegreesToRadians(MpBridgeYaw)), FMath::Sin(FMath::DegreesToRadians(MpBridgeYaw)), 0.0);
					C->SetWorldTransform(FTransform(FRotator(0.0f, MpBridgeYaw - 90.0f, 0.0f), MpBridgeHead - Along * MpBridgeMoreCm - FVector(0.0, 0.0, 20.0 - 6.0),
						FVector(MpBridgeWideCm / 150.0, (MpBridgeLenCm + MpBridgeMoreCm) / 435.0, 1.0)));
					MechPackComps.Add(C);
					++MechPackReplaced;
				}

	// 虹之龛：门和碎片挪位置（它们每帧按“原位 + 石沿伸出来多少”摆，所以改的是各自的原位）
	if (SillParts.Num() >= 5)
		if (AActor* Niche = SillParts[1].Actor.Get())
			if (UStaticMeshComponent* Plate = Niche->FindComponentByClass<UStaticMeshComponent>())
				{
					// 头一版是往外做了一个 28 cm 深的小柜子（深色的四壁凸在墙外）。用户看了说：深色的那个嵌进墙里，外面只留两扇黄色的小门。
					// ——墙是实心的，嵌不进去也看不见：深色的背板不画了，两扇门直接贴在墙面上（门后面就是墙）；
					// 碎片平时沉在墙里看不见，门一开它从墙里出来（Rainbow.cpp 的 PlaceSill 按 IrisNicheSinkCm 算）。
					Plate->SetVisibility(false);
					const FVector In = -DysisGB::PolarCm(float(DysisGB::AzOf(SillParts[1].Base)), 1.0, 0.0);      // 从龛朝着中庭
					for (int32 i = 2; i <= 4; ++i)
					{
						// 门：贴在墙面上（门板 3 cm 厚）；碎片“在龛里”的位置：墙面前 14 cm
						const double Want = DysisGB::R_IN - (i == 4 ? 14.0 : 1.6);
						SillParts[i].Base += In * (DysisGB::ROf(SillParts[i].Base) - Want);
					}
					IrisNicheSinkCm = MpIrisNicheDeepCm;
					PlaceSill(SillK);

					// 2026-10-11 用户：龛是嵌进墙里了，可对应的墙体没有挖空（门一开，后面还是一面实墙）。
					// ——墙是一整个实心的网格，没法真去挖：把龛口那一块墙面不画（墙换成能挖洞的材质 M_DysisWallCut，颜色照原来的），
					// 洞后面另摆一个 28 cm 深的小龛（五块板，和墙一个颜色）。碎片平时就放在这个龛里，门开了再出来。
					// 洞只在石沿伸到头、两扇门到位以后才开（UpdateMechPack）：门是跟着石沿从墙里滑出来的，那一段墙还得是实的。
					const float NAz = float(DysisGB::AzOf(SillParts[1].Base)), NZ = float(SillParts[1].Base.Z);
					if (UMaterialInterface* Cut = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Fx/M_DysisWallCut.M_DysisWallCut"), nullptr, LOAD_NoWarn | LOAD_Quiet))
					{
						if (WallCutMIDs.Num() == 0)
							for (UStaticMeshComponent* S : SkinComps)
								if (S && S->GetStaticMesh() && S->GetStaticMesh()->GetName() == TEXT("LP_SM_Wall"))
									for (int32 Slot = 0; Slot < S->GetNumMaterials(); ++Slot)
									{
										UMaterialInterface* Cur = S->GetMaterial(Slot);
										FLinearColor Col(0.72f, 0.60f, 0.44f); float Rough = 0.92f;
										if (Cur) { Cur->GetVectorParameterValue(FName(TEXT("BaseColor")), Col); Cur->GetScalarParameterValue(FName(TEXT("Roughness")), Rough); }
										UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Cut, this);
										MID->SetVectorParameterValue(TEXT("BaseColor"), Col);
										MID->SetScalarParameterValue(TEXT("Roughness"), Rough);
										S->SetMaterial(Slot, MID);
										WallCutMIDs.Add(MID);
									}
						const FVector NOut = DysisGB::PolarCm(NAz, 1.0, 0.0);
						for (UMaterialInstanceDynamic* MID : WallCutMIDs)
						{
							MID->SetVectorParameterValue(TEXT("HoleCenter"), FLinearColor(DysisGB::PolarCm(NAz, DysisGB::R_IN, NZ)));
							MID->SetVectorParameterValue(TEXT("HoleOut"), FLinearColor(NOut));
							MID->SetVectorParameterValue(TEXT("HoleHalf"), FLinearColor(0.0f, 0.0f, 0.0f));
							// 第二个洞：波吕丢刻斯的龛（2026-10-11 用户：他在墙里还是穿模，把墙再挖空一点或者把他缩小一点）。
							// 龛是做在厚墙那个部件里的（里面有龛的四壁），可它的后半截在圆殿的墙面后面——墙面在离殿心 15.5 m 处把龛拦腰截断，
							// 像（中心在 15.65 m）只露出前面一小截。把这一块墙面（1.32 × 2.68 m）不画，龛就是通的；封着龛口的那块石头在更前面，没化开以前照样挡着。
							if (const AActor* Px = Pollux.Get())
							{
								const float PAz = float(DysisGB::AzOf(Px->GetActorLocation()));
								MID->SetVectorParameterValue(TEXT("Hole2Center"), FLinearColor(DysisGB::PolarCm(PAz, DysisGB::R_IN, float(Px->GetActorLocation().Z) + 135.0f)));
								MID->SetVectorParameterValue(TEXT("Hole2Out"), FLinearColor(DysisGB::PolarCm(PAz, 1.0, 0.0)));
								MID->SetVectorParameterValue(TEXT("Hole2Half"), FLinearColor(66.0f, 24.0f, 134.0f));
							}
						}
						bIrisHoleOn = false;
						IrisNicheWalls.Reset();
						if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
						{
							UMaterialInterface* Warm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_WarmWhite.MI_MATCH_WarmWhite"), nullptr, LOAD_NoWarn | LOAD_Quiet);
							// 龛自己的坐标：X 朝外（进深）、Y 顺着墙、Z 朝上。洞口 58 × 68；板厚 2 cm，贴在洞口的外圈上
							const float D = MpIrisNicheDeepCm, HW = MpIrisHoleHalfW, HH = MpIrisHoleHalfH, W = 2.0f;
							const FVector At[5] = { FVector(D + W * 0.5, 0.0, 0.0), FVector(D * 0.5, -HW - W * 0.5, 0.0), FVector(D * 0.5, HW + W * 0.5, 0.0), FVector(D * 0.5, 0.0, HH + W * 0.5), FVector(D * 0.5, 0.0, -HH - W * 0.5) };
							const FVector Size[5] = { FVector(W, 2.0 * HW + 2.0 * W, 2.0 * HH + 2.0 * W), FVector(D + 2.0, W, 2.0 * HH + 2.0 * W), FVector(D + 2.0, W, 2.0 * HH + 2.0 * W),
								FVector(D + 2.0, 2.0 * HW, W), FVector(D + 2.0, 2.0 * HW, W) };
							const FTransform Frame(FRotator(0.0f, NAz, 0.0f), DysisGB::PolarCm(NAz, DysisGB::R_IN - 1.0f, NZ));
							for (int32 i = 0; i < 5; ++i)
							{
								UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
								C->SetupAttachment(RootComponent);
								C->SetMobility(EComponentMobility::Movable);
								C->SetStaticMesh(Cube);
								if (Warm) C->SetMaterial(0, Warm);
								C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
								C->RegisterComponent();
								C->SetWorldTransform(FTransform(FQuat::Identity, At[i], Size[i] / 100.0) * Frame);
								C->SetVisibility(false);
								MechPackComps.Add(C);
								IrisNicheWalls.Add(C);
							}
						}
					}
				}

	// 从池沿伸出来的那道半桥：和周围的地面一个颜色（2026-10-10 用户；屋顶那道细桥用的是它的模型，上面已经照原来的白石头拷走了）
	if (AActor* Half = Piece(TEXT("SM_Mech_HalfBridge_Deck")))
		if (UMaterialInterface* Warm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_WarmWhite.MI_MATCH_WarmWhite"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			TInlineComponentArray<UStaticMeshComponent*> Comps(Half);
			for (UStaticMeshComponent* C : Comps) for (int32 i = 0; i < C->GetNumMaterials(); ++i) C->SetMaterial(i, Warm);
		}

	UpdateMechPack(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Dysis 机关模型：换上了 %d 件"), MechPackReplaced);
}

void ADysisDirector::UpdateMechPack(float Dt)
{
	// 三相像：现在是哪一相就画哪一件（MirrorW3 = 暗、日、月各占多少，换相时是渐变的：过半就换）
	int32 Best = 0;
	for (int32 i = 1; i < 3; ++i) if (MirrorW3[i] > MirrorW3[Best]) Best = i;
	for (int32 i = 0; i < 3; ++i)
		if (UStaticMeshComponent* C = MirrorForms[i].Get())
			if (C->IsVisible() != (i == Best)) C->SetVisibility(i == Best);
	if (UStaticMeshComponent* C = SeleneEyeGlow.Get())
		if (C->IsVisible() != bSeleneOn) C->SetVisibility(bSeleneOn);

	// 日之龛的盖：轴在靠墙的那条边上，朝着人掀开（1 秒掀到 80°，竖在墙前面，不蹭墙）
	if (UStaticMeshComponent* C = SunLidModel.Get())
	{
		const FQuat Q(SunLidAxis, MpNicheOpenRad * DysisGB::Smoothstep(0.0f, 1.0f, SunNicheT));
		C->SetWorldLocationAndRotation(SunLidHinge + Q.RotateVector(SunLidClosedLoc - SunLidHinge), Q * SunLidClosedRot);
	}
	// 墙上的拉杆：两种姿态——没扳的时候杆贴着墙竖直朝上，扳下去以后竖直朝下（绕“顺着墙的那根轴”转半圈，中间从墙面前划过去）。
	// 2026-10-11 用户：原来杆是垂直于墙面伸出来的（再往下压一点），太突兀。底座上那两片半圆的侧板正好是给它转这半圈的。
	if (UStaticMeshComponent* C = LeverArmModel.Get())
		C->SetWorldRotation(FQuat(LeverAxis, HALF_PI - PI * DysisGB::Smoothstep(0.0f, 1.0f, SliderPos)) * LeverMountRot);

	// 水闸的开关：没开的时候杆立着，开了以后顺着石沿朝瀑布那边推倒（不到一秒）
	if (UStaticMeshComponent* C = SluiceArmModel.Get())
	{
		const UDysisWorldState* WS = UDysisWorldState::Get(this);
		SluiceLeverK = DysisGB::Toward(SluiceLeverK, WS && WS->bSluiceOpen ? 1.0f : 0.0f, 1.3f, Dt);
		C->SetWorldRotation(FQuat(SluiceArmAxis, FMath::DegreesToRadians(MpSluiceTiltDeg) * DysisGB::Smoothstep(0.0f, 1.0f, SluiceLeverK)) * SluiceArmRot0);
	}

	// 水亭浑天仪上的太阳和月托：跟着日月走（灰盒：bead.position = sunV × R，moonBead.position = moonV × 0.92 R）
	if (const UDysisTimeComponent* T = PlayerTime())
	{
		if (UStaticMeshComponent* C = PavSunBead.Get()) C->SetWorldLocation(MpPavArm + UDysisSkyLibrary::DysisSunDir(T->H) * MpPavSunR);
		if (UStaticMeshComponent* C = PavMoonHolder.Get()) C->SetWorldLocation(MpPavArm + UDysisSkyLibrary::DysisMoonDir(T->H) * MpPavMoonR - FVector(0.0, 0.0, MpHolderBelowCm));
	}

	// 虹之龛在墙上的洞：石沿伸到头、两扇门到位了才开（门是跟着石沿从墙里滑出来的，那一段墙得是实的）
	{
		const bool bHole = SillK > 0.985f && WallCutMIDs.Num() > 0;
		if (bHole != bIrisHoleOn)
		{
			bIrisHoleOn = bHole;
			for (UMaterialInstanceDynamic* MID : WallCutMIDs)
				if (MID) MID->SetVectorParameterValue(TEXT("HoleHalf"), bHole ? FLinearColor(MpIrisHoleHalfW, 24.0f, MpIrisHoleHalfH) : FLinearColor(0.0f, 0.0f, 0.0f));
			for (const TWeakObjectPtr<UStaticMeshComponent>& C : IrisNicheWalls) if (C.IsValid()) C->SetVisibility(bHole);
		}
	}
	// 波吕丢刻斯还封在墙里的时候不画（新模型比灰盒那尊宽，会从封着他的那块墙里穿出来——2026-10-10 用户）：
	// 封墙的那块石头被月光化掉（或者已经把他拉出来了）才画
	if (UStaticMeshComponent* C = PolluxModel.Get())
	{
		const FMoonstone* Wall = FindMoonstone(TEXT("twinWall"));
		const bool bShow = TwinState >= 2 || !Wall || Wall->K >= 0.98f;
		if (C->IsVisible() != bShow) C->SetVisibility(bShow);
	}

	const UDysisTimeComponent* Time = PlayerTime();
	// 金苹果周围的虹光：人离它 7 m 以外没有，3 m 以内最亮（淡淡的一圈）
	if (AppleHalo && AppleHaloMID && Apple)
	{
		const float Dist = Time ? float(FVector::Dist(Time->FootCm + FVector(0.0, 0.0, 100.0), Apple->GetComponentLocation())) : 1.0e6f;
		AppleHaloK = DysisGB::Toward(AppleHaloK, Apple->IsVisible() ? 1.0f - DysisGB::Smoothstep(300.0f, 700.0f, Dist) : 0.0f, 1.5f, Dt);
		if (AppleHalo->IsVisible() != (AppleHaloK > 0.01f)) AppleHalo->SetVisibility(AppleHaloK > 0.01f);
		AppleHaloMID->SetScalarParameterValue(TEXT("Opacity"), 0.5f * AppleHaloK);
	}
	// 伊莉丝浮雕：
	//   · 窗光照着它的时候整片墙是曝白的，浮雕和墙一个颜色就什么都看不清（2026-10-10 用户）——照着的时候把它的颜色压暗到四成，没照着还是墙的颜色；
	//   · 下面那个空着的人形平时比墙深一档；影子的头靠近它就一点点亮起来（原来的提示是浮雕上显出虹的颜色，用户不要彩色了），对上了、解开以后是亮的。
	if (IrisReliefMID)
	{
		float Lit = 0.0f;
		if (Time && !bCaught)
		{
			const FVector Sun = UDysisSkyLibrary::DysisSunDir(Time->H);
			int32 Hits = 0;
			const FVector2D Pts[3] = { FVector2D(60.0, 150.0), FVector2D(-55.0, 120.0), FVector2D(5.0, 70.0) };      // 浮雕上的三个点：离中线多远、离底边多高
			if (Sun.Z > 0.02)
				for (const FVector2D& P : Pts)
				{
					const FVector At = DysisGB::PolarCm(78.7f + FMath::RadiansToDegrees(float(P.X) / DysisGB::R_IN), DysisGB::R_IN - 14.0f, 629.5f + float(P.Y));
					if (!UDysisWorldState::InIsleShadow(At, Sun) && RainbowSeesLight(At, Sun)) ++Hits;
				}
			Lit = Hits / 3.0f;
		}
		IrisLitK = DysisGB::Toward(IrisLitK, Lit, 2.5f, Dt);
		const float Dim = FMath::Lerp(1.0f, MpIrisLitDim, IrisLitK);
		IrisReliefMID->SetVectorParameterValue(TEXT("Stone"), FLinearColor(0.72f * Dim, 0.60f * Dim, 0.44f * Dim));
	}
	if (UMaterialInstanceDynamic* Spot = ShadowSpot ? Cast<UMaterialInstanceDynamic>(ShadowSpot->GetMaterial(0)) : nullptr)
	{
		const float Show = bReliefDone ? 1.0f : FMath::Clamp(FMath::Max(ReliefHint / 0.4f * 0.6f, ReliefAlign), 0.0f, 1.0f);
		Spot->SetVectorParameterValue(TEXT("BaseColor"), FMath::Lerp(FLinearColor(0.43f, 0.35f, 0.26f), FLinearColor(1.0f, 0.86f, 0.55f), Show));
	}
}

FString ADysisDirector::DescribeMechPack() const
{
	int32 Shown = 0, Form = -1;
	for (const UStaticMeshComponent* C : MechPackComps) if (C && C->IsVisible() && !(C->GetOwner() && C->GetOwner()->IsHidden())) ++Shown;
	for (int32 i = 0; i < 3; ++i) if (MirrorForms[i].IsValid() && MirrorForms[i]->IsVisible()) Form = i;
	const UStaticMeshComponent* Lid = SunLidModel.Get();
	const UStaticMeshComponent* Arm = LeverArmModel.Get();
	const FVector LidUp = Lid ? Lid->GetUpVector() : FVector::UpVector, ArmAt = Arm ? Arm->GetComponentLocation() : FVector::ZeroVector;
	return FString::Printf(TEXT("{\"replaced\":%d,\"comps\":%d,\"shown\":%d,\"mirrorForm\":%d,\"seleneEye\":%s,")
		TEXT("\"appleHalo\":%s,\"appleHaloK\":%.3f,\"shadowSpot\":%s,\"irisLit\":%.3f,\"hint\":%.3f,\"lidModel\":%s,\"lidTiltDeg\":%.1f,\"sunShardBase\":[%.1f,%.1f,%.1f],\"leverArm\":[%.1f,%.1f],\"bandsShown\":%s}"),
		MechPackReplaced, MechPackComps.Num(), Shown, Form, SeleneEyeGlow.IsValid() && SeleneEyeGlow->IsVisible() ? TEXT("true") : TEXT("false"),
		AppleHalo ? TEXT("true") : TEXT("false"), AppleHaloK, ShadowSpot ? TEXT("true") : TEXT("false"), IrisLitK, ReliefHint,
		Lid ? TEXT("true") : TEXT("false"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(float(LidUp.Z), -1.0f, 1.0f))),
		SunShardBase.X, SunShardBase.Y, SunShardBase.Z, DysisGB::ROf(ArmAt), ArmAt.Z, BandsPlane && BandsPlane->IsVisible() ? TEXT("true") : TEXT("false"));
}
