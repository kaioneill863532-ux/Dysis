// 机关总管 · 夜里第三段：灰盒 v0.12 的 updateNymph（GOD / SHADOWBR）和 placeApple。
//   · 瀑布后面的女神：站在水槽里的矮台上，胸口左侧抱着一轮月亮。三相像反射的月光随着月亮往下扫，
//     人走在月桥上的某一步时，月光正好整个罩住她怀里的月亮；在那一步站住 1 秒，她就亮起来。
//   · 半桥：她亮起来以后，水池东北边的池沿伸出一段半桥，朝着水亭。
//   · 影桥：屋顶细桥的月影落在水面上；人站上半桥时，影子正好从半桥的尽头接到水亭，亮起来，能踩。
//   · 放苹果：走到水亭，把金苹果放上浑天仪的月托；之后是结局的对话（集齐三片碎片是另一段），然后回到主界面。
// 标准答案：Tools/greybox/golden/greybox_night3.json；测试：Tools/tests/pie_night3.py。
// 灰盒里瀑布被月光打到的那一块会透开（只是画面效果），这里还没做；月光本来就不被瀑布挡着。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisNightData.generated.h"
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
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度；greybox_night3.json 里 consts 那一段） ─────
	constexpr float FinHalfAz = 47.21297f, FinHalfR0 = 1055.0f, FinHalfR1 = 620.0f, FinHalfW = 75.0f;   // 半桥：池沿（低）→ 尽头（高，和水亭一样高）
	constexpr float FinPavTop = 20.0f;                                                                 // 水亭的地面
	// 屋顶细桥（它的月影就是影桥）：两头、桥面高度、半宽
	const FVector FinRoofBridgeS(-346.331, -271.394, 3030.0), FinRoofBridgeEnd(-824.137, -787.612, 3030.0);
	const FVector FinRoofBridgeSide(0.733884, -0.679275, 0.0);
	constexpr float FinRoofBridgeHalfW = 60.0f;
	// 水亭浑天仪：中心高 1.8 m，月托在朝着月亮的那一侧
	const FVector FinArmillary(0.0, 0.0, 180.0);
	constexpr float FinMoonBeadR = 47.84f;

	UStaticMeshComponent* FinMesh(AActor* A) { return A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr; }
}

// ───────────────────────── 开局 ─────────────────────────

void ADysisDirector::SetupFinale()
{
	UWorld* World = GetWorld();
	// 女神、瀑布：灰盒里都只是模型，不挡光（挡人的是女神身上一根柱子）
	if (UStaticMeshComponent* C = FinMesh(Piece(TEXT("SM_Mech_Goddess_Statue")))) { C->bUseDefaultCollision = false; C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); }
	if (AActor* Fall = Piece(TEXT("SM_Mech_Water_Waterfall"))) Fall->SetActorEnableCollision(false);

	// 半桥：先缩在池沿里看不见；能踩的面是一块薄板（挂在半桥这个 Actor 上，它带着 “DysisZone=gbridge” 的 Tag）
	HalfDeck = Piece(TEXT("SM_Mech_HalfBridge_Deck"));
	HalfFloor = nullptr; HalfRails.Reset();
	if (AActor* Deck = HalfDeck.Get())
	{
		if (UStaticMeshComponent* C = FinMesh(Deck)) { C->SetMobility(EComponentMobility::Movable); C->bUseDefaultCollision = false; C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		const FVector A = DysisGB::PolarCm(FinHalfAz, FinHalfR0, 0.0f), B = DysisGB::PolarCm(FinHalfAz, FinHalfR1, FinPavTop);
		const FVector Dir = (B - A).GetSafeNormal(), Side(-Dir.Y, Dir.X, 0.0);
		const FQuat Rot = FRotationMatrix::MakeFromXY(Dir, Side.GetSafeNormal()).ToQuat();
		UBoxComponent* Box = NewObject<UBoxComponent>(Deck);
		Box->SetupAttachment(Deck->GetRootComponent());
		Box->SetMobility(EComponentMobility::Movable);
		Box->SetCollisionObjectType(ECC_WorldDynamic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetCanEverAffectNavigation(false);
		Box->SetHiddenInGame(true);
		Box->SetBoxExtent(FVector(FVector::Dist(A, B) * 0.5, FinHalfW, 2.0));
		Box->RegisterComponent();
		Deck->AddInstanceComponent(Box);
		Box->SetWorldLocationAndRotation((A + B) * 0.5 - Rot.GetUpVector() * 2.0, Rot);
		HalfFloor = Box;
		Deck->SetActorHiddenInGame(true);
	}
	else UE_LOG(LogTemp, Warning, TEXT("Dysis 半桥：关卡里找不到 SM_Mech_HalfBridge_Deck"));
	// 半桥两边看不见的护栏：两道沿半径的薄墙
	{
		const float RMid = (FinHalfR0 + FinHalfR1) * 0.5f, Off = FMath::RadiansToDegrees((FinHalfW + 10.0f) / RMid);
		for (const float Sg : { -1.0f, 1.0f })
		{
			UBoxComponent* Box = MakeWall();
			Box->SetBoxExtent(FVector((FinHalfR0 - FinHalfR1 + 40.0f) * 0.5f, 4.0f, 135.0f));
			Box->SetWorldLocationAndRotation(DysisGB::PolarCm(FinHalfAz + Sg * Off, (FinHalfR0 + 10.0f + FinHalfR1 - 30.0f) * 0.5f, 85.0f), FRotator(0.0f, FinHalfAz + Sg * Off, 0.0f));
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			HalfRails.Add(Box);
		}
	}

	// 影桥：水面上的一片影子（看得见的）和它上面一块看不见的地面（站上去算 “shadowbr”）
	if (PlaneMesh)
	{
		ShadowPlane = NewObject<UStaticMeshComponent>(this);
		ShadowPlane->SetupAttachment(RootComponent);
		ShadowPlane->SetMobility(EComponentMobility::Movable);
		ShadowPlane->SetStaticMesh(PlaneMesh);
		ShadowPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ShadowPlane->SetCastShadow(false);
		ShadowPlane->SetVisibility(false);
		ShadowPlane->RegisterComponent();
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Night/M_DysisShadowBridge.M_DysisShadowBridge")))
		{
			ShadowMID = UMaterialInstanceDynamic::Create(Mat, this);
			ShadowPlane->SetMaterial(0, ShadowMID);
		}
	}
	if (World)
		if (ADysisInvisibleWall* Floor = World->SpawnActorDeferred<ADysisInvisibleWall>(ADysisInvisibleWall::StaticClass(), FTransform::Identity))
		{
			Floor->ExtentCm = FVector(100.0, 60.0, 2.0);
			Floor->Group = TEXT("ShadowBridge");
			Floor->Tags.Add(TEXT("DysisZone=shadowbr"));
			Floor->FinishSpawning(FTransform::Identity);
			if (Floor->Box) { Floor->Box->SetMobility(EComponentMobility::Movable); Floor->Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
			ShadowFloor = Floor;
		}

	// 结局的对话：放上苹果以后再定是哪一段
	EndingDialogue = NewObject<UDysisDialogueComponent>(this);
	EndingDialogue->RegisterComponent();

	GoddessLit = GoddessSweep = GoddessDwell = HalfBridgeExt = ShadowOn = FinaleT = 0.0f;
	bGoddessLit = bGoddessGlint = bGoddessHinted = bShadowTold = bApplePlaced = bEndingStarted = bEndingDone = false;
}

void ADysisDirector::AddFinaleInteracts()
{
	// 把金苹果放上水亭浑天仪的月托
	FDysisInteract I;
	I.Id = TEXT("placeApple");
	I.Pos = []() { return FVector(0.0, 0.0, FinPavTop); };
	I.ZRange = []() { return FVector2D(FinPavTop - 30.0, FinPavTop + 160.0); };
	I.RadiusCm = 280.0f;
	I.When = [this]() { return bCaught && !bApplePlaced; };
	I.Label = []() { return FText::FromString(DysisCopy::PromptPlaceApple); };
	I.Act = [this]()
	{
		bApplePlaced = true;
		FinaleT = 0.0f;
		ADysisHUD::Notify(GetWorld(), DysisCopy::ApplePlaced, 6.0f);
	};
	Interacts.Add(MoveTemp(I));
}

// ───────────────────────── 女神、半桥 ─────────────────────────

FString ADysisDirector::DebugGoddessLit() const
{
	FMoonstone Probe; Probe.Source = 2;
	FString S;
	for (int32 Set = 0; Set < 2; ++Set)
	{
		if (Set) S += TEXT("|");
		const FVector* Pts = Set ? GDysisGoddessSweep : GDysisGoddessSamples;
		for (int32 i = 0; i < 5; ++i)
		{
			const TCHAR* Why = TEXT("?");
			MoonSampleLit(Probe, Pts[i], &Why);
			if (i) S += TEXT(",");
			S += Why;
		}
	}
	return S;
}

void ADysisDirector::UpdateGoddess(float Dt)
{
	// 月光落在她怀里的月亮上多少（五个点）、扫过她身上多少（五个高度）
	float Lit = 0.0f, Sweep = 0.0f;
	if (bCaught)
	{
		FMoonstone Probe; Probe.Source = 2;
		for (const FVector& P : GDysisGoddessSamples) if (MoonSampleLit(Probe, P)) Lit += 0.2f;
		for (const FVector& P : GDysisGoddessSweep) if (MoonSampleLit(Probe, P)) Sweep += 0.2f;
	}
	GoddessLit = Lit; GoddessSweep = Sweep;
	// 要让月光整个罩住那轮月亮，并且停在那里 1 秒（走过去的时候只会亮一下）：人得在月桥上找到那一步、站住
	GoddessDwell = Lit > 0.99f ? GoddessDwell + Dt : FMath::Max(0.0f, GoddessDwell - 2.0f * Dt);
	if (!bGoddessLit && Lit > 0.3f && !bGoddessGlint)
	{
		bGoddessGlint = true;
		ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackGoddessFirstLit, 3.2f);
	}
	const UDysisTimeComponent* Time = PlayerTime();
	if (!bGoddessLit && bGoddessGlint && !bGoddessHinted && bCaught && Time && Time->Zone == TEXT("L0"))
	{
		bGoddessHinted = true;   // 已经下到水庭了：月光是在月桥上的某一步照到她的
		ADysisHUD::Notify(GetWorld(), DysisCopy::MoonbridgeHint, 5.0f);
	}
	if (GoddessDwell >= 1.0f && !bGoddessLit)
	{
		bGoddessLit = true;
		ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackGoddessSecondLit, 5.6f);
	}

	// 半桥：从池沿伸出来（2 秒）；伸到头才能踩
	const float Before = HalfBridgeExt;
	HalfBridgeExt = DysisGB::Toward(HalfBridgeExt, bGoddessLit ? 1.0f : 0.0f, 0.5f, Dt);
	if (AActor* Deck = HalfDeck.Get())
	{
		const bool bShow = HalfBridgeExt > 0.01f;
		if (Deck->IsHidden() == bShow) Deck->SetActorHiddenInGame(!bShow);
		if (HalfBridgeExt != Before || Before == 0.0f)
		{
			const float Sc = FMath::Max(0.001f, DysisGB::Smoothstep(0.0f, 1.0f, HalfBridgeExt));
			Deck->SetActorScale3D(FVector(Sc, Sc, 1.0f));   // 模型的原点就在池沿上：从那里往水亭长出去
		}
	}
	const ECollisionEnabled::Type Mode = HalfBridgeExt > 0.97f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
	if (HalfFloor.IsValid() && HalfFloor->GetCollisionEnabled() != Mode)
	{
		HalfFloor->SetCollisionEnabled(Mode);
		for (const TWeakObjectPtr<UBoxComponent>& R : HalfRails) if (R.IsValid()) R->SetCollisionEnabled(Mode);
	}
}

// ───────────────────────── 影桥 ─────────────────────────

bool ADysisDirector::RoofBridgeShadow(float H, FVector OutQuad[4]) const
{
	// 灰盒 roofBridgeShadow：细桥桥面的四个角顺着月光投到水亭地面那么高的平面上
	const FVector M = UDysisSkyLibrary::DysisMoonDir(H);
	if (M.Z <= 0.05) return false;
	auto Proj = [&M](const FVector& P) { return P - M * ((P.Z - FinPavTop) / M.Z); };
	OutQuad[0] = Proj(FinRoofBridgeS - FinRoofBridgeSide * FinRoofBridgeHalfW);
	OutQuad[1] = Proj(FinRoofBridgeEnd - FinRoofBridgeSide * FinRoofBridgeHalfW);
	OutQuad[2] = Proj(FinRoofBridgeEnd + FinRoofBridgeSide * FinRoofBridgeHalfW);
	OutQuad[3] = Proj(FinRoofBridgeS + FinRoofBridgeSide * FinRoofBridgeHalfW);
	return true;
}

FString ADysisDirector::DebugBridgeShadow(float H) const
{
	FVector Q[4];
	if (!RoofBridgeShadow(H, Q)) return TEXT("null");
	return FString::Printf(TEXT("[[%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f],[%.2f,%.2f,%.2f]]"), Q[0].X, Q[0].Y, Q[0].Z, Q[1].X, Q[1].Y, Q[1].Z, Q[2].X, Q[2].Y, Q[2].Z, Q[3].X, Q[3].Y, Q[3].Z);
}

void ADysisDirector::UpdateShadowBridge(float Dt)
{
	const UDysisTimeComponent* Time = PlayerTime();
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const float Iris = State ? State->IrisACm : 0.0f;
	FVector Q[4];
	const bool bHas = bCaught && Time && Iris > 700.0f && RoofBridgeShadow(Time->H, Q);
	if (!bHas)
	{
		ShadowOn = DysisGB::Toward(ShadowOn, 0.0f, 1.5f, Dt);
		if (ShadowPlane) ShadowPlane->SetVisibility(false);
		if (ShadowFloor.IsValid() && ShadowFloor->Box) ShadowFloor->Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}
	// 影子的中线：从细桥靠光圈那头的影子（再往回延 2 m）到另一头的影子
	const FVector C = (Q[0] + Q[3]) * 0.5, E = (Q[1] + Q[2]) * 0.5, Dir = (E - C).GetSafeNormal2D(), Side(-Dir.Y, Dir.X, 0.0);
	const double HalfW = FVector::Dist(Q[0], Q[3]) * 0.5;
	const FVector From = FVector(C.X, C.Y, FinPavTop - 2.0) - Dir * 200.0, To(E.X, E.Y, FinPavTop - 2.0);
	// 接上了吗：影子的中线穿过半桥的尽头和水亭的中心，人已经在半桥上（或者已经接上了）
	auto ToLine = [&](const FVector& P) { return FMath::Abs(FVector::DotProduct(FVector(P.X - C.X, P.Y - C.Y, 0.0), Side)); };
	const bool bOnIt = Time->Zone == TEXT("gbridge") || Time->Zone == TEXT("shadowbr") || ShadowOn > 0.5f;
	const bool bJoined = HalfBridgeExt > 0.97f && ToLine(DysisGB::PolarCm(FinHalfAz, FinHalfR1, 0.0f)) < 45.0 && ToLine(FVector::ZeroVector) < 60.0 && bOnIt;
	ShadowOn = DysisGB::Toward(ShadowOn, bJoined ? 1.0f : 0.0f, bJoined ? 0.7f : 1.5f, Dt);

	const FVector Mid = (From + To) * 0.5;
	const double Len = FVector::Dist(From, To);
	const FQuat Rot = FRotationMatrix::MakeFromXY(Dir, Side).ToQuat();
	if (ShadowPlane)
	{
		ShadowPlane->SetVisibility(true);
		ShadowPlane->SetWorldLocationAndRotation(Mid + FVector(0.0, 0.0, 1.0), Rot);
		ShadowPlane->SetWorldScale3D(FVector(Len / 100.0, HalfW * 2.0 / 100.0, 1.0));
		if (ShadowMID)
		{
			// 没接上是一片暗影；接上了亮成月白色
			ShadowMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(FMath::Lerp(0.03f, 0.72f, ShadowOn), FMath::Lerp(0.04f, 0.8f, ShadowOn), FMath::Lerp(0.07f, 1.0f, ShadowOn)));
			ShadowMID->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(0.55f, 0.85f, ShadowOn) * DysisGB::Smoothstep(700.0f, 900.0f, Iris));
			ShadowMID->SetScalarParameterValue(TEXT("Glow"), FMath::Lerp(0.0f, 1.2f, ShadowOn));
		}
	}
	if (ShadowFloor.IsValid() && ShadowFloor->Box)
	{
		UBoxComponent* Box = ShadowFloor->Box;
		Box->SetBoxExtent(FVector(Len * 0.5, HalfW, 2.0));
		ShadowFloor->SetActorLocationAndRotation(Mid - FVector(0.0, 0.0, 2.0), Rot);
		const ECollisionEnabled::Type Mode = ShadowOn > 0.5f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
		if (Box->GetCollisionEnabled() != Mode) Box->SetCollisionEnabled(Mode);
	}
	if (ShadowOn > 0.99f && !bShadowTold)
	{
		bShadowTold = true;
		ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackShadowBridge, 4.6f);
	}
}

// ───────────────────────── 放苹果、结局 ─────────────────────────

void ADysisDirector::UpdateFinale(float Dt)
{
	if (!bApplePlaced) return;
	const float Before = FinaleT;
	FinaleT += Dt;
	// 金苹果飘到月托上（浑天仪上朝着月亮的那一侧）
	if (Apple)
	{
		const UDysisTimeComponent* Time = PlayerTime();
		const FVector Holder = FinArmillary + UDysisSkyLibrary::DysisMoonDir(Time ? Time->H : 0.0f) * FinMoonBeadR;
		Apple->SetWorldLocation(FMath::Lerp(Apple->GetComponentLocation(), Holder, FMath::Min(1.0f, Dt * 3.0f)));
	}
	// 4 秒后：结局的对话（三片碎片都集齐了是另一段）
	if (Before < 4.0f && FinaleT >= 4.0f && EndingDialogue && !bEndingStarted)
	{
		bEndingStarted = true;
		const bool bAll = bSunNicheOpen && bIrisNicheOpen && bMoonShard;
		const TCHAR* const* Lines = bAll ? DysisCopy::EndingB : DysisCopy::EndingA;
		const int32 Num = bAll ? DysisCopy::EndingBCount : DysisCopy::EndingACount;
		EndingDialogue->Lines.SetNum(Num);
		for (int32 i = 0; i < Num; ++i) EndingDialogue->Lines[i] = FText::FromString(Lines[i]);
		EndingDialogue->Play();
	}
	// 对话放完：黑场，回到主界面
	if (bEndingStarted && !bEndingDone && EndingDialogue && !EndingDialogue->IsPlaying() && FinaleT > 5.0f)
	{
		bEndingDone = true;
		FinaleT = 100.0f;
		if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->FadeTo(1.0f, 2.0f);
	}
	if (bEndingDone && Before < 102.5f && FinaleT >= 102.5f)
		if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->ReturnToMainMenu();
}

void ADysisDirector::DebugShowMoonBridge()
{
	if (FMoonstone* Br = FindMoonstone(TEXT("moonBridge"))) { Br->bDormant = false; Br->bPerm = true; }
}

FString ADysisDirector::DescribeFinale() const
{
	return FString::Printf(TEXT("{\"lit\":%.2f,\"sweep\":%.2f,\"dwell\":%.2f,\"glint\":%s,\"hinted\":%s,\"goddess\":%s,\"halfExt\":%.3f,\"halfOn\":%s,\"shadowOn\":%.3f,\"shadowFloor\":%s,\"placed\":%s,\"finT\":%.2f,\"ending\":%s,\"endingLines\":%d,\"talking\":%s,\"done\":%s}"),
		GoddessLit, GoddessSweep, GoddessDwell, bGoddessGlint ? TEXT("true") : TEXT("false"), bGoddessHinted ? TEXT("true") : TEXT("false"), bGoddessLit ? TEXT("true") : TEXT("false"),
		HalfBridgeExt, HalfFloor.IsValid() && HalfFloor->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? TEXT("true") : TEXT("false"), ShadowOn,
		ShadowFloor.IsValid() && ShadowFloor->Box && ShadowFloor->Box->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? TEXT("true") : TEXT("false"),
		bApplePlaced ? TEXT("true") : TEXT("false"), FinaleT, bEndingStarted ? TEXT("true") : TEXT("false"), EndingDialogue ? EndingDialogue->Lines.Num() : 0,
		EndingDialogue && EndingDialogue->IsPlaying() ? TEXT("true") : TEXT("false"), bEndingDone ? TEXT("true") : TEXT("false"));
}
