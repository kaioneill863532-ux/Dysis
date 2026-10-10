// 机关总管 · 美术的机关模型（2026-10-10 的《打包.zip》，转换和导入见 Art/Models/mech-pack-20261010/）。
// 只换“看起来的样子”：原来的部件不画了，碰撞、挡光、会动的逻辑都还是它；新模型挂在原部件身上，跟着它转、跟着它沉、跟着它藏。
// 模型的规矩（转换时摆好的）：+Y 是正面、+Z 朝上、原点在脚底中心（浮雕：原点在墙面上、底边中点，+Y 朝殿心，背板已经顺着圆墙弯好）。
// 颜色都在顶点色里：材质 M_DysisMechPack 把没什么颜色的（石头、石膏）统一成墙的那种暖白，金边、镜面、彩虹这些点缀照原样。
// 哪个模型换哪个部件，有四处是问过用户的（三相像上日中月下暗；推的是“双子_右”；拱门给屋顶细桥的桥门，拱还给虹门；月门给月亮浮雕、不要深紫色的门板）。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
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
	// Dysis.GoddessShift <度> <厘米>：把瀑布后面的女神像（连判定月光的点）从灰盒的位置绕殿心转几度、往墙那边挪几厘米（试位置用）
	FAutoConsoleCommandWithWorldAndArgs GDysisGoddessShift(TEXT("Dysis.GoddessShift"), TEXT("瀑布后面的女神像：Dysis.GoddessShift <绕殿心转几度> <往墙那边挪几厘米>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2) return;
			for (TActorIterator<ADysisDirector> It(World); It; ++It) It->DebugMoveGoddess(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
		}));
	// 瀑布后面的女神像（2026-10-10 用户：没在瀑布里的正中，要挪到正中、连底座往墙那边靠；怀里的月亮、底座都要正圆）：
	//   · 模型里她怀里那面圆盘的圆心在身子的轴线右边 63.8 cm、前面 48.5 cm、离脚 395.3 cm；灰盒判定月光的那一点在“右 35.1、前 96.8、高 381.5”。
	//     模型相对原来的部件往殿心挪 48.3、往左挪 28.7、往下沉 13.8：圆盘的圆心就正好是那一点（月光照到的就是她怀里的月亮）；
	//   · 整件（像、挡人的部分、判定月光的点一起）从灰盒的位置绕殿心转 MpGodShiftAz 度、往墙那边挪 MpGodShiftR 厘米。
	//     用户想要的是瀑布正中（方位 128°，要转 −6.5°）。在游戏里把月桥上每一步、三相像的八个格都量了一遍（Dysis.GoddessShift + DebugGoddessShift）：
	//     放到正中的话她怀里的月亮五个点最多照到一个，哪一格都不行；转到 −3° 还剩四个，−2.5° 以内才五个全照到。
	//     所以定在 −2°、往墙那边 68 cm：身子的轴线在方位 132.5°（原来 135.7°，往中间挪了 0.8 m）、离殿心 14.4 m。
	//     月光全照到她的时刻是 159.6–160.0，月桥上是第 58 步（灰盒原位是第 61 步）——pie_night3 按这个核。
	constexpr float MpGodFwdCm = 48.3f, MpGodLeftCm = 28.7f, MpGodDownCm = 13.8f;
	constexpr float MpGodShiftAz = -2.0f, MpGodShiftR = 68.3f;
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
	constexpr float MpMirrorScale = 1.4f, MpPlinthXY = 0.62f, MpPlinthH = 40.0f, MpPlinthWasH = 90.0f;
	// 日之龛：盒子的中心比原来靠墙 12 cm（盖子朝着人掀开、竖起来时不蹭墙）；盖子最多掀 80°
	constexpr float MpNicheBackCm = 12.0f, MpNicheTopCm = 43.0f, MpNicheLidHalfCm = 31.0f, MpNicheOpenRad = 1.40f;
	// 拉杆：缩到 0.62、装到墙上——底座的底面贴着墙（嵌进去 1.5 cm），杆从墙上伸出来、上下扳；离地 1.05 m。模型里转轴在底座的底面上面 37.2 cm
	constexpr float MpLeverScale = 0.62f, MpLeverUpCm = 105.0f, MpLeverPivotCm = 37.2f, MpLeverSinkCm = 1.5f;
	// 外立面神像：去了底座以后人直接站在窗台上（窗台面比部件的原点高 15 cm），往墙里收：衣摆的后沿嵌进墙里 12 cm
	constexpr float MpFacadeBackCm = 56.0f, MpFacadeInsetCm = 12.0f, MpFacadeSillCm = 15.0f;
	constexpr bool MpAppleHalo = false;
	// 虹之龛（三层虹的窗旁边墙上那个带两扇小门的龛）。灰盒里它只有 5 cm 深，碎片有 22 cm 大——关着门的时候碎片穿在门和背板外面
	// （2026-10-10 用户：龛里挖一点空间出来放碎片）。墙是实心的挖不进去，改成往外做：四面加上龛壁，做成一个 28 cm 深的小柜子，
	// 两扇门挪到柜子的前口上，碎片放在柜子正中。
	constexpr float MpIrisNicheDeepCm = 28.0f, MpIrisNicheWallCm = 2.5f;
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
	constexpr float MpGateScale = 1.75f, MpGateCenterCm = 58.0f, MpGateFootZ = 3030.0f;
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
					const float Half = 172.0f * MpPlinthXY * 0.9f;
					Wall->SetBoxExtent(FVector(Half, Half, MpPlinthH * 0.5f));
					Wall->SetWorldLocationAndRotation(Pl->GetActorLocation() + FVector(0.0, 0.0, MpPlinthH * 0.5f), FRotator(0.0f, AzOfActor(Pl), 0.0f));
				}
				HideLook(Pl);
				UStaticMeshComponent* V = NewObject<UStaticMeshComponent>(Pl);
				V->SetupAttachment(Src);
				V->SetMobility(EComponentMobility::Movable);
				V->SetStaticMesh(Src->GetStaticMesh());
				for (int32 i = 0; i < Src->GetNumMaterials(); ++i) V->SetMaterial(i, Src->GetMaterial(i));
				V->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
				V->RegisterComponent();
				V->SetRelativeScale3D(FVector(MpPlinthXY, MpPlinthXY, MpPlinthH / MpPlinthWasH));
				MechPackComps.Add(V);
				Drop = MpPlinthWasH - MpPlinthH;
			}
		const FVector Loc = S->GetActorLocation() - FVector(0.0, 0.0, Drop);      // 站在底座正中（转的时候绕这根轴转）
		const TCHAR* Names[3] = { TEXT("MirrorDark"), TEXT("MirrorSun"), TEXT("MirrorMoon") };      // 和 MirrorW3 一个顺序：暗、日、月
		for (int32 i = 0; i < 3; ++i) MirrorForms[i] = Put(S, Names[i], Loc, MirrorYawNow - 90.0f, FVector(MpMirrorScale), EMpLook::Stone);
	}

	// ── 瀑布后面的女神、双子：都朝殿心站 ──
	if (AActor* G = Piece(TEXT("SM_Mech_Goddess_Statue")))
	{
		DebugMoveGoddess(MpGodShiftAz, MpGodShiftR);
		HideLook(G); HideLook(Piece(TEXT("SM_Mech_Goddess_Plinth")));      // 模型自己带着三级圆台座；原来那根方墩子不画了
		const float Az = AzOfActor(G);
		const FVector Out = DysisGB::PolarCm(Az, 1.0, 0.0), Tan(-Out.Y, Out.X, 0.0);
		Put(G, TEXT("Goddess"), G->GetActorLocation() - Out * MpGodFwdCm - Tan * MpGodLeftCm - FVector(0.0, 0.0, MpGodDownCm), Az + 90.0f, One, EMpLook::Stone);
	}
	if (AActor* A = Castor.Get()) { HideLook(A); Put(A, TEXT("TwinCastor"), A->GetActorLocation(), AzOfActor(A) + 90.0f, One, EMpLook::Stone); }
	if (AActor* A = Pollux.Get()) { HideLook(A); Put(A, TEXT("TwinPollux"), A->GetActorLocation(), AzOfActor(A) + 90.0f, One, EMpLook::Stone); }

	// ── 天鹅和它的圆台座（女神像没有新模型，还是原来那个；月光照到她变天鹅的时候两件一个缩、一个长，新天鹅挂在原来的天鹅身上跟着长） ──
	if (AActor* P = Piece(TEXT("SM_Mech_Swan_Plinth"))) { HideLook(P); Put(P, TEXT("SwanPlinth"), P->GetActorLocation(), AzOfActor(P) + 90.0f, FVector(0.92, 0.92, 1.0), EMpLook::Stone); }
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
		if (UStaticMeshComponent* C = Put(A, TEXT("IrisRelief"), DysisGB::PolarCm(78.7f, DysisGB::R_IN - MpReliefProud, 740.0f - 110.5f), 78.7f + 90.0f, One, EMpLook::AllStone))
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
			const float R = DysisGB::R_IN - 0.25f;
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
		}
	}
	if (AActor* A = SwanRelief.Get())
	{
		// 四只小天鹅（两对，中间空着一只的位置）。它挂在原来的浮雕身上，解开以后一起沉下去。
		// 2026-10-10 用户：天鹅像的影子和浮雕一高一低。——判定用的是“天鹅头的影子落到灰盒定的那一点”（方位 257.57°、离四层地面 0.95 m）：
		// 浮雕原来摆得比这一点高半米多（影子的头只到浮雕上天鹅的肚子）、中间的空位也偏右 46 cm。现在往下、往左挪：
		// 中间的空位正对那一点，浮雕上天鹅的头和影子的头一样高（天鹅的身子落到地面上，像浮在水面）。
		HideLook(A);
		Relief(A, TEXT("SwanRelief"), 257.39f, 2300.0f - 40.0f);
	}

	// ── 月亮浮雕 → 月门：门板填在门洞里（和墙一个颜色）、银边的门框压在洞口外面、月牙在门板上。月光照够了它们跟着原来的两件一起隐去 ──
	{
		AActor* Block = Piece(TEXT("SM_Mech_MoonRelief_Block")); AActor* Disk = Piece(TEXT("SM_Mech_MoonRelief_Disk"));
		const float Az = 178.7f;
		const FVector Loc = DysisGB::PolarCm(Az, DysisGB::R_IN - 0.2f, 1450.0f);
		if (Block) { HideLook(Block); Put(Block, TEXT("MoonGateSlab"), Loc, Az + 90.0f, One, EMpLook::AllStone); Put(Block, TEXT("MoonGateFrame"), Loc, Az + 90.0f, One, EMpLook::AllStone); }
		// 月牙在模型里是嵌在门板里面的（比门板的面还靠里 2–5 cm），门板换成墙色的实心板以后它就被盖住了（2026-10-10 用户：月门上的月亮不见了）——往外挪 6.5 cm，浮在门板面上
		// 颜色和门一样（用户后来要的）：全按墙的石头色，不留模型自己的银色
		if (Disk) { HideLook(Disk); Put(Disk, TEXT("MoonGateCrescent"), DysisGB::PolarCm(Az, DysisGB::R_IN - 0.2f - 6.5f, 1450.0f), Az + 90.0f, One, EMpLook::AllStone); }
	}

	// ── 日之龛：盒和盖。新的比原来大（0.8 m 宽），盒往墙那边挪了 17 cm，让盖的轴还在盒朝殿心的那条边上 ──
	if (AActor* Box = Piece(TEXT("SM_Mech_SunNiche_Box")))
	{
		// 2026-10-10 用户：现在是朝着墙开的，翻过来。——盒子转过 180°（有拱窗的那一面朝着人）；盖子不再挂在原来的盖子身上
		// （灰盒里盖子的轴在靠人的那条边上，掀起来挡在人和盒子中间），改成轴在靠墙的那条边上、朝着人掀开（UpdateMechPack 里按开到第几秒转）。
		const float Az = AzOfActor(Box), BoxZ = float(Box->GetActorLocation().Z), R = float(DysisGB::ROf(Box->GetActorLocation())) + MpNicheBackCm;
		HideLook(Box);
		Put(Box, TEXT("NicheBox"), DysisGB::PolarCm(Az, R, BoxZ), Az - 90.0f, One, EMpLook::AsIs);
		if (AActor* Lid = Piece(TEXT("SM_Mech_SunNiche_Lid")))
		{
			HideLook(Lid);
			SunLidClosedLoc = DysisGB::PolarCm(Az, R, BoxZ + MpNicheTopCm);
			SunLidClosedRot = FRotator(0.0f, Az - 90.0f, 0.0f).Quaternion();
			SunLidHinge = DysisGB::PolarCm(Az, R + MpNicheLidHalfCm, BoxZ + MpNicheTopCm);
			const FVector Out = DysisGB::PolarCm(Az, 1.0, 0.0);
			SunLidAxis = FVector(-Out.Y, Out.X, 0.0);
			SunLidModel = Put(Box, TEXT("NicheLid"), SunLidClosedLoc, Az - 90.0f, One, EMpLook::AsIs);
		}
		// 碎片从盒子正中、盒面上面升起来（原来的位置是按灰盒那个小盒子定的，偏在新盒子的前沿里面）
		SunShardBase = DysisGB::PolarCm(Az, R, BoxZ + MpNicheTopCm + 11.0f);
		if (AActor* Sh = SunShard.Get()) if (SunNicheT <= 0.0f) Sh->SetActorLocation(SunShardBase);
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
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		for (const FName& Tag : It->Tags)
		{
			const FString Label = Tag.ToString();
			if (!Label.StartsWith(TEXT("SM_Facade_Statue_"))) continue;
			const float S = Label.Contains(TEXT("_L3_")) ? 0.69f : 0.75f, Az = AzOfActor(*It);
			HideLook(*It);
			// 2026-10-10 用户：和神殿一个颜色、尽量融在一起；底座撤掉，整个往殿心收一点、不要那么凸出来。——
			// 模型去掉了脚下那块方底座（人直接站在窗台上、脚沉进去 2 cm），衣摆的后沿嵌进墙里；颜色全按墙的暖白，不留贴图上的深浅
			if (UStaticMeshComponent* C = Put(*It, TEXT("FacadeStatue"), DysisGB::PolarCm(Az, DysisGB::ROf(It->GetActorLocation()) - MpFacadeInsetCm + MpFacadeBackCm * S, It->GetActorLocation().Z + MpFacadeSillCm - 2.0),
				Az - 90.0f, FVector(S), EMpLook::AllStone))
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0))) MID->SetScalarParameterValue(TEXT("Detail"), 0.0f);
			break;
		}

	// ── 屋顶细桥的桥门：一座石拱立在桥头，门板装在拱洞里、挂在原来那扇门身上跟着它开关；原来那扇 4.35 m 长的铜栅不画了（它还拦着） ──
	if (AActor* Door = DoorLeaf.Get())
	{
		HideLook(Door);
		const FVector Dir = Door->GetActorRightVector();                                  // 关着的时候门扇顺着哪边伸出去（门轴在 Actor 的原点）
		const float Yaw = float(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
		FVector Loc = Door->GetActorLocation() + Dir * MpGateCenterCm; Loc.Z = MpGateFootZ;
		Put(nullptr, TEXT("GateArch"), Loc, Yaw, FVector(MpGateScale), EMpLook::AllStone);
		Put(Door, TEXT("GateDoor"), Loc, Yaw, FVector(MpGateScale), EMpLook::AllStone);
	}
	// ── 虹门：同一座石拱，放大到 2.3 倍（宽 3.3 m、高 4.1 m），立在原来的位置上 ──
	if (AActor* Arch = Piece(TEXT("SM_Mech_RainbowArch_Arch")))
	{
		HideLook(Arch);
		Put(Arch, TEXT("GateArch"), Arch->GetActorLocation(), float(Arch->GetActorRotation().Yaw), FVector(2.3), EMpLook::AllStone);
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
					C->SetMaterial(0, Src->GetMaterial(0));
					C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
					C->RegisterComponent();
					// 半桥的模型：宽 150（X）、长 435（Y，从原点往前）、顶面在 Z = 20
					// （2026-10-10 用户：和屋顶那一圈台阶的叶片有点穿在一起，往上抬一点点 → 抬 6 cm）
					C->SetWorldTransform(FTransform(FRotator(0.0f, MpBridgeYaw - 90.0f, 0.0f), MpBridgeHead - FVector(0.0, 0.0, 20.0 - 6.0), FVector(MpBridgeWideCm / 150.0, MpBridgeLenCm / 435.0, 1.0)));
					MechPackComps.Add(C);
					++MechPackReplaced;
				}

	// 虹之龛：加龛壁、门和碎片挪位置（它们每帧按“原位 + 石沿伸出来多少”摆，所以改的是各自的原位）
	if (SillParts.Num() >= 5)
		if (AActor* Niche = SillParts[1].Actor.Get())
			if (UStaticMeshComponent* Plate = Niche->FindComponentByClass<UStaticMeshComponent>())
				if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
				{
					// 龛自己的坐标：X 顺着墙（宽 60）、+Y 朝中庭、Z 朝上（高 70），背板的面在 Y = 0
					const float D = MpIrisNicheDeepCm, W = MpIrisNicheWallCm;
					const FVector At[4] = { FVector(-30.0 - W * 0.5, D * 0.5, 0.0), FVector(30.0 + W * 0.5, D * 0.5, 0.0), FVector(0.0, D * 0.5, 35.0 + W * 0.5), FVector(0.0, D * 0.5, -35.0 - W * 0.5) };
					const FVector Size[4] = { FVector(W, D, 70.0 + 2.0 * W), FVector(W, D, 70.0 + 2.0 * W), FVector(60.0, D, W), FVector(60.0, D, W) };
					for (int32 i = 0; i < 4; ++i)
					{
						UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Niche);
						C->SetupAttachment(Plate);
						C->SetMobility(EComponentMobility::Movable);
						C->SetStaticMesh(Cube);
						C->SetMaterial(0, Plate->GetMaterial(0));
						C->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
						C->RegisterComponent();
						C->SetRelativeTransform(FTransform(FQuat::Identity, At[i], Size[i] / 100.0));
						MechPackComps.Add(C);
					}
					const FVector In = -DysisGB::PolarCm(float(DysisGB::AzOf(SillParts[1].Base)), 1.0, 0.0);      // 从龛朝着中庭
					const double PlateR = DysisGB::ROf(SillParts[1].Base);
					for (int32 i = 2; i <= 4; ++i)
					{
						// 门：挪到柜子的前口（离背板 D）；碎片：柜子正中（离背板 D / 2）
						const double Now = PlateR - DysisGB::ROf(SillParts[i].Base), Want = i == 4 ? D * 0.5 : D;
						SillParts[i].Base += In * (Want - Now);
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
	// 墙上的拉杆：杆从墙上平着伸出来，扳的时候绕“顺着墙的那根轴”往下压（扳多少跟着滑块走）
	if (UStaticMeshComponent* C = LeverArmModel.Get())
		C->SetWorldRotation(FQuat(LeverAxis, -SliderPos * 1.0f) * LeverMountRot);

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
