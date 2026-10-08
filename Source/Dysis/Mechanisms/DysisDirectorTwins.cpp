// 机关总管 · 夜里第二段：灰盒 v0.12 的 MSHRINE / updateTwins / MOONBRIDGE。
//   · 月之龛（二层北墙上）：三相像的镜子转过第 0 格的一瞬，反射的月光落在那面墙上，墙透开，里面是月亮碎片。
//   · 双子：卡斯托耳站在 L1 西边。L1 另一头（东南，挨着瀑布）有一段厚墙，正面是月石：镜子反射的月光扫到它，
//     墙面隐去，龛里站着波吕丢刻斯。把他拉出来，站到他逆时针一侧，顺时针一路推回卡斯托耳身边
//     （往回走，月亮也跟着倒回去）。推到头时两尊像都在月光里，就并肩了。
//   · 月桥：并肩以后从他们脚下伸出去，绕过水亭北边，落到水庭东边。桥头原来拦着的铜链收起来。
// 标准答案：Tools/greybox/golden/greybox_night2.json；测试：Tools/tests/pie_night2.py。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisNightData.generated.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "World/DysisWorldState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度；greybox_night2.json 里 consts 那一段） ─────
	constexpr float TwnWallAz = 146.5f, TwnEndAz = 250.6f, TwnROut = 1420.0f, TwnFloorZ = 600.0f;
	const FVector TwnNiche(-1305.031, 863.781, 600.0), TwnOut(-1184.118, 783.751, 600.0);   // 龛里、拉出来以后站的地方
	constexpr float TwnHeadArc0 = 253.8f, TwnHeadArc1 = 259.738f;                           // 月桥桥头在 L1 栏杆上的那一段（合拢前铜链拦着）
	constexpr float TwnBridgeHalfW = 75.0f;
	// 月之龛：二层北墙，方位 6.58°，离地 1.55 m
	const FVector TwnShrinePos(1564.634, 180.405, 1639.706);
	constexpr float TwnShrineZ = 1604.706f;

	enum { TwnHidden = 0, TwnInNiche = 1, TwnPulling = 2, TwnIsOut = 3, TwnJoined = 4 };

	UStaticMeshComponent* TwnMesh(AActor* A) { return A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr; }
	void TwnMovable(AActor* A) { if (UStaticMeshComponent* C = TwnMesh(A)) { C->SetMobility(EComponentMobility::Movable); C->bUseDefaultCollision = false; } }
	UBoxComponent* TwnFloorBox(AActor* Owner, const FVector& A, const FVector& B, const FVector& Side, double HalfW)
	{
		// 一段 4 cm 厚的薄板，顶面过 A、B 两点。只挡人。挂在 Owner 上（脚下的区域跟它的 Tag 走）
		const FQuat Rot = FRotationMatrix::MakeFromXY((B - A).GetSafeNormal(), Side).ToQuat();
		UBoxComponent* Box = NewObject<UBoxComponent>(Owner);
		Box->SetupAttachment(Owner->GetRootComponent());
		Box->SetMobility(EComponentMobility::Movable);
		Box->SetCollisionObjectType(ECC_WorldDynamic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCanEverAffectNavigation(false);
		Box->SetHiddenInGame(true);
		Box->SetBoxExtent(FVector(FVector::Dist(A, B) * 0.5 + 1.5, HalfW, 2.0));
		Box->ComponentTags.Add(TEXT("DysisOneWay"));
		Box->RegisterComponent();
		Owner->AddInstanceComponent(Box);
		Box->SetWorldLocationAndRotation((A + B) * 0.5 - Rot.GetUpVector() * 2.0, Rot);
		return Box;
	}
}

// ───────────────────────── 开局 ─────────────────────────

void ADysisDirector::SetupTwins()
{
	// 月石：厚墙的墙面、月之龛的墙面、月桥
	{
		FMoonstone Ms;
		Ms.Id = TEXT("twinWall");
		Ms.Samples.Append(GDysisTwinWallSamples, UE_ARRAY_COUNT(GDysisTwinWallSamples));
		Ms.Source = 2; Ms.Need = 0.66f;
		Ms.bHasDoor = true; Ms.DoorAz = TwnWallAz; Ms.DoorHalfDeg = 2.6f; Ms.DoorZ = TwnFloorZ;
		if (AActor* Block = Piece(TEXT("SM_Mech_Twins_WallBlock"))) { TwnMovable(Block); Ms.Blocks.Add(Block); }
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 双子：关卡里找不到 SM_Mech_Twins_WallBlock"));
		Moonstones.Add(MoveTemp(Ms));
	}
	{
		FMoonstone Ms;
		Ms.Id = TEXT("moonShrine");
		Ms.Samples.Append(GDysisMoonShrineSamples, UE_ARRAY_COUNT(GDysisMoonShrineSamples));
		Ms.Source = 2; Ms.Need = 0.55f; Ms.bOneShot = true;
		if (AActor* Block = Piece(TEXT("SM_Mech_MoonShrine_Block"))) { TwnMovable(Block); Ms.Blocks.Add(Block); }
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 月之龛：关卡里找不到 SM_Mech_MoonShrine_Block"));
		Moonstones.Add(MoveTemp(Ms));
	}
	MoonDeck = Piece(TEXT("SM_Mech_MoonBridge_Deck"));
	{
		FMoonstone Ms;
		Ms.Id = TEXT("moonBridge");
		Ms.Source = 0; Ms.bDormant = true;
		if (AActor* Deck = MoonDeck.Get())
		{
			TwnMovable(Deck);
			if (UStaticMeshComponent* C = TwnMesh(Deck)) { C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetCastShadow(false); }   // 模型只管看
			// 能踩的面：沿桥 72 段薄板，挂在桥面这个 Actor 上（它带着 “DysisZone=moonbr” 的 Tag）
			const int32 N = UE_ARRAY_COUNT(GDysisMoonBridgePts) - 1;
			for (int32 i = 0; i < N; ++i)
			{
				const FVector A = GDysisMoonBridgePts[i], B = GDysisMoonBridgePts[i + 1];
				const FVector D = GDysisMoonBridgePts[FMath::Min(N, i + 1)] - GDysisMoonBridgePts[FMath::Max(0, i)];
				TwnFloorBox(Deck, A, B, FVector(-D.Y, D.X, 0.0).GetSafeNormal(), TwnBridgeHalfW);
			}
			Ms.Parts.Add(Deck);
		}
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 月桥：关卡里找不到 SM_Mech_MoonBridge_Deck"));
		Moonstones.Add(MoveTemp(Ms));
	}
	for (FMoonstone& Ms : Moonstones) SetMoonstoneState(Ms, 0.0f);

	// 月桥两边看不见的护栏（灰盒 MOONBRIDGE.rails）：只在中庭上空那一段，两头留出上下桥的地方
	MoonRails.Reset();
	{
		const int32 N = UE_ARRAY_COUNT(GDysisMoonBridgePts) - 1;
		for (int32 i = 3; i < N - 3; i += 2)
		{
			const FVector A = GDysisMoonBridgePts[i], B = GDysisMoonBridgePts[i + 2];
			if (DysisGB::ROf(B) > DysisGB::R_POOL - 30.0 && i > N / 2) continue;
			const FVector D = B - A, Sn = FVector(-D.Y, D.X, 0.0).GetSafeNormal();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			for (const double Sg : { -1.0, 1.0 })
			{
				UBoxComponent* Box = MakeWall();
				Box->SetBoxExtent(FVector((D.Size() + 15.0) * 0.5, 5.0, 110.0));
				Box->SetWorldLocationAndRotation((A + B) * 0.5 + Sn * (Sg * (TwnBridgeHalfW + 10.0)) + FVector(0.0, 0.0, 110.0), FRotator(0.0f, Yaw, 0.0f));
				Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				MoonRails.Add(Box);
			}
		}
	}

	// 两尊像：灰盒里只是模型；卡斯托耳身上有一根挡人的柱子，波吕丢刻斯出来以后才有
	Pollux = Piece(TEXT("SM_Mech_Twins_Pollux"));
	Castor = Piece(TEXT("SM_Mech_Twins_Castor"));
	TwinChain = Piece(TEXT("SM_Mech_Twins_Chain"));
	if (AActor* P = Pollux.Get()) { TwnMovable(P); P->SetActorEnableCollision(false); PolluxYaw0 = P->GetActorRotation().Yaw; }
	else UE_LOG(LogTemp, Warning, TEXT("Dysis 双子：关卡里找不到 SM_Mech_Twins_Pollux"));
	if (UStaticMeshComponent* C = TwnMesh(Castor.Get())) { C->bUseDefaultCollision = false; C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); }
	if (AActor* Ch = TwinChain.Get()) Ch->SetActorEnableCollision(false);
	if (UBoxComponent* Box = MakeWall())
	{
		Box->SetBoxExtent(FVector(40.0, 40.0, 110.0));
		Box->SetWorldLocation(TwnNiche + FVector(0.0, 0.0, 110.0));
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PolluxBlock = Box;
	}
	// 铜链后面看不见的挡板：桥头那一段栏杆口，合拢前过不去
	if (UBoxComponent* Box = MakeWall())
	{
		const float Mid = (TwnHeadArc0 + TwnHeadArc1) * 0.5f, R = 1233.0f;
		Box->SetBoxExtent(FVector(14.0, R * FMath::Tan(FMath::DegreesToRadians((TwnHeadArc1 - TwnHeadArc0) * 0.5f)), 115.0));
		Box->SetWorldLocationAndRotation(DysisGB::PolarCm(Mid, R, TwnFloorZ + 115.0f), FRotator(0.0f, Mid, 0.0f));
		ChainBlock = Box;
	}
	MoonShrineShard = Piece(TEXT("SM_Mech_MoonShrine_Shard"));
	if (AActor* Sh = MoonShrineShard.Get()) { TwnMovable(Sh); Sh->SetActorEnableCollision(false); Sh->SetActorHiddenInGame(true); }

	TwinState = TwnHidden; TwinT = 0.0f; PolluxAz = TwnWallAz; PolluxLit = CastorLit = 0.0f; TwinLitClock = 0.0f;
	bTwinsJoined = bMoonShard = bShrineTold = false;
}

void ADysisDirector::AddTwinsInteracts()
{
	{
		// 把龛里的像拉出来：墙透开着的时候才行
		FDysisInteract I;
		I.Id = TEXT("pullPollux");
		I.Pos = []() { return TwnNiche; };
		I.ZRange = []() { return FVector2D(580.0, 800.0); };
		I.RadiusCm = 220.0f;
		I.When = [this]() { const FMoonstone* W = FindMoonstone(TEXT("twinWall")); return TwinState == TwnInNiche && W && W->K > 0.6f; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptPullPollux); };
		I.Act = [this]() { TwinState = TwnPulling; TwinT = 0.0f; };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 月之龛：墙透开以后取出月亮碎片
		FDysisInteract I;
		I.Id = TEXT("moonShrine");
		I.Pos = []() { return TwnShrinePos; };
		I.ZRange = []() { return FVector2D(TwnShrineZ - 160.0, TwnShrineZ + 40.0); };
		I.RadiusCm = 160.0f;
		I.When = [this]() { const FMoonstone* S = FindMoonstone(TEXT("moonShrine")); return !bMoonShard && S && S->K > 0.6f; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptOpenMoonNiche); };
		I.Act = [this]()
		{
			bMoonShard = true;
			if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->SetShard(2, true);
			ADysisHUD::Notify(GetWorld(), DysisCopy::MoonNicheOpened, 4.6f);
		};
		Interacts.Add(MoveTemp(I));
	}
}

// ───────────────────────── 双子、月桥、月之龛 ─────────────────────────

float ADysisDirector::StatueMoonLit(const FVector& FootCm) const
{
	// 灰盒 litOf：腰、胸、头三个点朝月亮看过去，没被挡住的算照着（厚墙那块月石的墙面不算）
	const UWorld* World = GetWorld();
	const UDysisTimeComponent* Time = PlayerTime();
	const FVector Moon = UDysisSkyLibrary::DysisMoonDir(Time ? Time->H : 0.0f);
	if (!World || !bCaught || Moon.Z < 0.05) return 0.0f;
	FCollisionQueryParams Params(TEXT("DysisTwinLit"), /*bTraceComplex*/ true);
	if (const APlayerController* PC = World->GetFirstPlayerController())
		if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);
	for (const FMoonstone& Ms : Moonstones)
		if (Ms.Id == TEXT("twinWall"))
			for (const TWeakObjectPtr<AActor>& B : Ms.Blocks) if (B.IsValid()) Params.AddIgnoredActor(B.Get());
	int32 N = 0;
	for (const double Z : { 90.0, 150.0, 200.0 })
	{
		const FVector P = FootCm + FVector(0.0, 0.0, Z);
		if (!World->LineTraceTestByChannel(P + Moon * 50.0, P + Moon * 40000.0, ECC_Visibility, Params)) ++N;
	}
	return float(N) / 3.0f;
}

float ADysisDirector::DebugStatueMoonLit(FVector FootCm) const { return StatueMoonLit(FootCm); }

void ADysisDirector::UpdateTwins(float Dt)
{
	const UWorld* World = GetWorld();
	const FMoonstone* Wall = FindMoonstone(TEXT("twinWall"));
	AActor* P = Pollux.Get();
	FVector Pos = P ? P->GetActorLocation() : TwnNiche;

	if (TwinState == TwnHidden && Wall && Wall->K > 0.6f)
	{
		TwinState = TwnInNiche;
		ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackPolluxRevealed, 4.6f);
	}
	if (TwinState == TwnPulling)
	{
		TwinT += Dt;
		const float K = DysisGB::Smoothstep(0.0f, 1.6f, TwinT);
		Pos = FMath::Lerp(TwnNiche, TwnOut, double(K));
		if (K >= 1.0f) { TwinState = TwnIsOut; PolluxAz = TwnWallAz; }
	}
	else if (TwinState == TwnIsOut)
	{
		// 推回去：人在他逆时针一侧、贴着他、往顺时针走
		const UDysisTimeComponent* Time = PlayerTime();
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ACharacter* Char = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		const UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;
		if (Time && Move)
		{
			const FVector Foot = Time->FootCm;
			const float Dz = float(DysisGB::AngDiff(DysisGB::AzOf(Foot), PolluxAz));
			// 人想往哪边走、走多快（灰盒 player.move：按键的方向 × 速度，不管有没有被挡住）
			const FVector Want = Move->GetCurrentAcceleration().GetSafeNormal2D() * Move->GetMaxSpeed();
			if (!Want.IsNearlyZero() && FVector::Dist2D(Foot, Pos) < 130.0 && Dz < -0.5f && FMath::Abs(DysisGB::ROf(Foot) - TwnROut) < 120.0 && FMath::Abs(Foot.Z - TwnFloorZ) < 50.0)
			{
				const float A = FMath::DegreesToRadians(PolluxAz);
				const double V = FVector::DotProduct(Want, FVector(-FMath::Sin(A), FMath::Cos(A), 0.0));   // 厘米每秒，顺时针为正
				if (V > 30.0) PolluxAz = FMath::Min(TwnEndAz, PolluxAz + FMath::RadiansToDegrees(float(V * Dt / TwnROut)));
			}
		}
		Pos = DysisGB::PolarCm(PolluxAz, TwnROut, TwnFloorZ);
		if (PolluxAz >= TwnEndAz - 0.05f && PolluxLit > 0.6f)
		{
			// 两尊像并肩站在月光里：月桥出现，铜链收起来
			TwinState = TwnJoined; bTwinsJoined = true;
			ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackTwinsUnited, 5.2f);
			if (FMoonstone* Br = FindMoonstone(TEXT("moonBridge"))) { Br->bDormant = false; Br->bPerm = true; }
			if (ChainBlock.IsValid()) ChainBlock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (AActor* Ch = TwinChain.Get()) Ch->SetActorHiddenInGame(true);
		}
	}
	if (P)
	{
		const float Az = TwinState >= TwnIsOut ? PolluxAz : TwnWallAz;
		P->SetActorLocationAndRotation(Pos, FRotator(0.0f, PolluxYaw0 + (Az - TwnWallAz), 0.0f));
	}
	if (PolluxBlock.IsValid())
	{
		PolluxBlock->SetWorldLocation(Pos + FVector(0.0, 0.0, 110.0));
		const ECollisionEnabled::Type Mode = TwinState >= TwnIsOut ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
		if (PolluxBlock->GetCollisionEnabled() != Mode) PolluxBlock->SetCollisionEnabled(Mode);
	}
	// 两尊像被月光照着多少（每 0.1 秒算一次）
	TwinLitClock += Dt;
	if (TwinLitClock > 0.1f)
	{
		TwinLitClock = 0.0f;
		PolluxLit = TwinState == TwnHidden ? 0.0f : StatueMoonLit(Pos);
		CastorLit = Castor.IsValid() ? StatueMoonLit(Castor->GetActorLocation()) : 0.0f;
	}

	// 月桥：护栏跟着桥一起出现
	if (const FMoonstone* Br = FindMoonstone(TEXT("moonBridge")))
	{
		const ECollisionEnabled::Type Mode = Br->K > 0.5f ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
		if (MoonRails.Num() > 0 && MoonRails[0].IsValid() && MoonRails[0]->GetCollisionEnabled() != Mode)
			for (const TWeakObjectPtr<UBoxComponent>& R : MoonRails) if (R.IsValid()) R->SetCollisionEnabled(Mode);
	}

	// 月之龛：墙透开了能看见里面的碎片（拿走以后就没有了）
	if (const FMoonstone* Shrine = FindMoonstone(TEXT("moonShrine")))
	{
		if (AActor* Sh = MoonShrineShard.Get())
		{
			const bool bShow = !bMoonShard && Shrine->K > 0.2f;
			if (Sh->IsHidden() == bShow) Sh->SetActorHiddenInGame(!bShow);
			Sh->AddActorWorldRotation(FRotator(0.0f, FMath::RadiansToDegrees(Dt), 0.0f));
		}
		if (Shrine->K > 0.6f && !bShrineTold)
		{
			bShrineTold = true;
			ADysisHUD::Notify(GetWorld(), DysisCopy::FeedbackMoonNicheLit, 4.8f);
		}
	}
}

FString ADysisDirector::DescribeTwins() const
{
	const FVector Pos = Pollux.IsValid() ? Pollux->GetActorLocation() : FVector::ZeroVector;
	float MoonBrK = 0.0f, WallK = 0.0f, WallLit = 0.0f, ShrineK = 0.0f, ShrineLit = 0.0f;
	bool bDormant = true, bShrinePerm = false;
	for (const FMoonstone& Ms : Moonstones)
	{
		if (Ms.Id == TEXT("moonBridge")) { MoonBrK = Ms.K; bDormant = Ms.bDormant; }
		else if (Ms.Id == TEXT("twinWall")) { WallK = Ms.K; WallLit = Ms.LitFrac; }
		else if (Ms.Id == TEXT("moonShrine")) { ShrineK = Ms.K; ShrineLit = Ms.LitFrac; bShrinePerm = Ms.bPerm; }
	}
	int32 RailsOn = 0;
	for (const TWeakObjectPtr<UBoxComponent>& R : MoonRails) if (R.IsValid() && R->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++RailsOn;
	return FString::Printf(TEXT("{\"state\":%d,\"t\":%.3f,\"polluxAz\":%.3f,\"pollux\":[%.2f,%.2f,%.2f],\"lit\":%.3f,\"litC\":%.3f,\"joined\":%s,\"wallK\":%.3f,\"wallLit\":%.3f,")
		TEXT("\"bridgeK\":%.3f,\"dormant\":%s,\"rails\":%d,\"railsOn\":%d,\"chainOn\":%s,\"chainShown\":%s,\"shrineK\":%.3f,\"shrineLit\":%.3f,\"shrinePerm\":%s,\"moonShard\":%s}"),
		TwinState, TwinT, PolluxAz, Pos.X, Pos.Y, Pos.Z, PolluxLit, CastorLit, bTwinsJoined ? TEXT("true") : TEXT("false"), WallK, WallLit,
		MoonBrK, bDormant ? TEXT("true") : TEXT("false"), MoonRails.Num(), RailsOn,
		ChainBlock.IsValid() && ChainBlock->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? TEXT("true") : TEXT("false"),
		TwinChain.IsValid() && !TwinChain->IsHidden() ? TEXT("true") : TEXT("false"), ShrineK, ShrineLit, bShrinePerm ? TEXT("true") : TEXT("false"), bMoonShard ? TEXT("true") : TEXT("false"));
}
