// 机关总管 · 夜里第一段：灰盒 v0.12 的 updateSwan / updateMoonstones（SWAN / TUNWIN / SEALS / MOONSTONES / MREL）。
//   · 天鹅（三层西边）：女神像站在回廊内侧，墙上是天鹅群浮雕。月光照满她全身时她变成黑天鹅；转动底座（8 格），
//     让天鹅头的影子落进浮雕中间的空白，停住 0.7 秒：浮雕连同后面的墙沉下去，露出墙里往下的楼梯（TS，到二层）。
//   · 墙里的楼梯：上面的门一开，朝外的一排窗从上往下一扇扇打开，下面封门的石板也沉下去。
//   · 月石：被月光照着时显形 / 隐去的石头。东南墙的月亮浮雕是一块：三相像的镜子转到夜里那一格，
//     反射的月光照到它（九个取样点里六个以上），它就永远隐去，后面是另一段楼梯（TR，到一层）。
// 标准答案：Tools/greybox/golden/greybox_night1.json；测试：Tools/tests/pie_night1.py。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "World/DysisWorldState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度；greybox_night1.json 里 consts 那一段） ─────
	// 天鹅：三层西边，方位 259.73°
	const FVector NgtSwanOrigin(-230.926, -1274.244, 2350.0);      // 像脚下（台座顶面）
	constexpr float NgtSwanSlots[8] = { 169.728f, 214.728f, 259.728f, 304.728f, 349.728f, 34.728f, 79.728f, 124.728f };
	constexpr int32 NgtSwanSlot0 = 3;                              // 模型摆的是开局那一格
	const FVector NgtSwanGap(-331.329, -1503.007, 2394.664);       // 浮雕上空着的那一只的位置
	const FVector NgtSwanFace(-274.258, -1513.349, 2300.0);        // 浮雕面的墙根
	const FVector NgtSwanFaceN(0.178321, 0.983972, 0.0);           // 浮雕朝着中庭的方向
	const FVector NgtSwanReliefView(-268.2, -1479.9, 2300.0);      // “看看浮雕”的位置：P(259.73°, R_IN − 0.35)
	constexpr float NgtSwanMissCm = 14.0f;
	// 月亮浮雕：二层东南墙（墙里楼梯 TR 的上门），方位 178.7°
	constexpr float NgtMoonRelAz = 178.70520f, NgtMoonRelHalf = 2.37970f;
	const FVector NgtMoonRelSamples[9] = {
		FVector(-1534.266, 72.961, 1500.0), FVector(-1534.266, 72.961, 1570.0), FVector(-1534.266, 72.961, 1640.0),
		FVector(-1535.608, 34.708, 1500.0), FVector(-1535.608, 34.708, 1570.0), FVector(-1535.608, 34.708, 1640.0),
		FVector(-1535.996, -3.566, 1500.0), FVector(-1535.996, -3.566, 1570.0), FVector(-1535.996, -3.566, 1640.0) };

	UStaticMeshComponent* NgtMesh(AActor* A) { return A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr; }
	void NgtMovable(AActor* A) { if (UStaticMeshComponent* C = NgtMesh(A)) { C->SetMobility(EComponentMobility::Movable); C->bUseDefaultCollision = false; } }
	/** 不挡光（光路用“可见性”射线），挡人不变。 */
	void NgtNoLight(AActor* A) { if (UStaticMeshComponent* C = NgtMesh(A)) { C->bUseDefaultCollision = false; C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); } }
}

// ───────────────────────── 开局 ─────────────────────────

void ADysisDirector::SetupNight()
{
	// 天鹅：女神像、黑天鹅（月光下才显出来）、浮雕、浮雕后面的门
	SwanGoddess = Piece(TEXT("SM_Mech_Swan_Goddess"));
	SwanBird = Piece(TEXT("SM_Mech_Swan_Swan"));
	SwanRelief = Piece(TEXT("SM_Mech_Swan_Relief"));
	SwanDoor = Piece(TEXT("SM_Mech_Swan_Door"));
	for (AActor* A : { SwanGoddess.Get(), SwanBird.Get(), SwanRelief.Get(), SwanDoor.Get() }) NgtMovable(A);
	// 灰盒里像和天鹅只是模型（挡人的是像身上一根柱子），浮雕只挡人：都不挡光
	NgtNoLight(SwanGoddess.Get());
	NgtNoLight(SwanRelief.Get());
	if (AActor* B = SwanBird.Get()) { B->SetActorEnableCollision(false); B->SetActorHiddenInGame(true); }
	if (AActor* G = SwanGoddess.Get()) SwanYaw0 = G->GetActorRotation().Yaw;
	if (AActor* R = SwanRelief.Get()) SwanReliefBase = R->GetActorLocation();
	if (AActor* D = SwanDoor.Get()) SwanDoorBase = D->GetActorLocation();
	if (!SwanGoddess.IsValid() || !SwanBird.IsValid() || !SwanRelief.IsValid() || !SwanDoor.IsValid()) UE_LOG(LogTemp, Warning, TEXT("Dysis 天鹅：关卡里找不到女神像、天鹅、浮雕或门的部件"));
	SwanSlot = NgtSwanSlot0; SwanYawNow = NgtSwanSlots[SwanSlot];
	SwanLit = SwanForm = SwanSolvedT = SwanOpenT = 0.0f; SwanMissCm = 900.0f; bSwanSolved = false;

	// 墙里两段楼梯：朝外的窗（白天用石块堵着）、下面封门的石板
	const TCHAR* WinNames[2][3] = { { TEXT("SM_Mech_StairWindows_TS_1"), TEXT("SM_Mech_StairWindows_TS_2"), TEXT("SM_Mech_StairWindows_TS_3") },
	                                { TEXT("SM_Mech_StairWindows_TR_1"), TEXT("SM_Mech_StairWindows_TR_2"), nullptr } };
	const TCHAR* SealNames[2] = { TEXT("SM_Mech_StairSeal_TS"), TEXT("SM_Mech_StairSeal_TR") };
	for (int32 s = 0; s < 2; ++s)
	{
		FStairSet& Set = StairSets[s];
		Set = FStairSet();
		for (const TCHAR* Name : WinNames[s])
		{
			if (!Name) continue;
			FStairWindow W;
			W.Actor = Piece(Name);
			if (AActor* A = W.Actor.Get()) { W.Base = A->GetActorLocation(); NgtMovable(A); }
			else UE_LOG(LogTemp, Warning, TEXT("Dysis 墙里楼梯：关卡里找不到部件 %s"), Name);
			Set.Windows.Add(W);
		}
		Set.Seal = Piece(SealNames[s]);
		if (AActor* A = Set.Seal.Get()) { Set.SealBase = A->GetActorLocation(); NgtMovable(A); }
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 墙里楼梯：关卡里找不到部件 %s"), SealNames[s]);
	}

	// 月石
	Moonstones.Reset();
	{
		FMoonstone Ms;
		Ms.Id = TEXT("moonRelief");
		Ms.Samples.Append(NgtMoonRelSamples, UE_ARRAY_COUNT(NgtMoonRelSamples));
		Ms.Source = 2; Ms.Need = 0.66f; Ms.bOneShot = true;
		Ms.bHasDoor = true; Ms.DoorAz = NgtMoonRelAz; Ms.DoorHalfDeg = NgtMoonRelHalf; Ms.DoorZ = 1450.0f;
		Ms.PermCopy = DysisCopy::FeedbackMoonGateLit;
		if (AActor* Block = Piece(TEXT("SM_Mech_MoonRelief_Block"))) { NgtMovable(Block); Ms.Blocks.Add(Block); }
		else UE_LOG(LogTemp, Warning, TEXT("Dysis 月亮浮雕：关卡里找不到 SM_Mech_MoonRelief_Block"));
		Moonstones.Add(MoveTemp(Ms));
	}
	MoonDisk = Piece(TEXT("SM_Mech_MoonRelief_Disk"));
	if (AActor* Disk = MoonDisk.Get()) { NgtMovable(Disk); Disk->SetActorEnableCollision(false); }
	for (FMoonstone& Ms : Moonstones) SetMoonstoneState(Ms, 0.0f);
	MoonstoneClock = 0.0f;
}

void ADysisDirector::AddNightInteracts()
{
	{
		// 女神像（黑天鹅）的底座：按一次转一格
		FDysisInteract I;
		I.Id = TEXT("swan");
		I.Pos = []() { return NgtSwanOrigin; };
		I.ZRange = []() { return FVector2D(2280.0, 2500.0); };
		I.RadiusCm = 220.0f;
		I.Label = []() { return FText::FromString(DysisCopy::PromptRotateStatue); };
		I.Act = [this]() { SwanSlot = (SwanSlot + 1) % 8; };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 天鹅群浮雕：解开以前可以看
		FDysisInteract I;
		I.Id = TEXT("swanRelief");
		I.Pos = []() { return NgtSwanReliefView; };
		I.ZRange = []() { return FVector2D(2280.0, 2500.0); };
		I.RadiusCm = 90.0f;
		I.When = [this]() { return !bSwanSolved; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptViewRelief); };
		I.Act = [this]() { ADysisHUD::Notify(GetWorld(), DysisCopy::ViewSwanRelief, 5.2f); };
		Interacts.Add(MoveTemp(I));
	}
}

// ───────────────────────── 天鹅 ─────────────────────────

FVector ADysisDirector::SwanHeadCm() const
{
	// 天鹅的头：朝着她面对的方向往前 0.58 m、高 1.38 m，跟着天鹅现在的大小缩放（灰盒 swan.scale = 0.4 + 0.6 × form）
	const float Y = FMath::DegreesToRadians(SwanYawNow), S = 0.4f + 0.6f * SwanForm;
	return NgtSwanOrigin + FVector(58.0 * FMath::Cos(Y), 58.0 * FMath::Sin(Y), 138.0) * S;
}

void ADysisDirector::UpdateSwan(float Dt)
{
	const UWorld* World = GetWorld();
	// 底座朝选中的那一格转过去（每秒 90°，走近路）
	const float D = float(DysisGB::AngDiff(NgtSwanSlots[SwanSlot], SwanYawNow));
	if (FMath::Abs(D) > 1.0e-3f) SwanYawNow += FMath::Clamp(D, -90.0f * Dt, 90.0f * Dt);

	// 月光照着她多少：身上三个点（腰、胸、头）朝月亮看过去，没被挡住的算照着
	const UDysisTimeComponent* Time = PlayerTime();
	const FVector Moon = UDysisSkyLibrary::DysisMoonDir(Time ? Time->H : 0.0f);
	float Lit = 0.0f;
	if (bCaught && Moon.Z > 0.01 && World)
	{
		const float Y = FMath::DegreesToRadians(SwanYawNow);
		const FVector Fwd(FMath::Cos(Y), FMath::Sin(Y), 0.0);
		const FVector Pts[3] = { NgtSwanOrigin + FVector(0.0, 0.0, 50.0), NgtSwanOrigin + Fwd * 40.0 + FVector(0.0, 0.0, 90.0), NgtSwanOrigin + Fwd * 58.0 + FVector(0.0, 0.0, 138.0) };
		FCollisionQueryParams Params(TEXT("DysisSwanLit"), /*bTraceComplex*/ true);
		if (const APlayerController* PC = World->GetFirstPlayerController())
			if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
		for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);
		for (const FVector& P : Pts)
			if (!World->LineTraceTestByChannel(P + Moon * 30.0, P + Moon * 40000.0, ECC_Visibility, Params)) Lit += 1.0f / 3.0f;
	}
	SwanLit = Lit;
	SwanForm = DysisGB::Toward(SwanForm, Lit > 0.6f ? 1.0f : 0.0f, 1.2f, Dt);
	// 像和天鹅：一个缩下去、一个长出来
	const FRotator Rot(0.0f, SwanYaw0 + (SwanYawNow - NgtSwanSlots[NgtSwanSlot0]), 0.0f);
	if (AActor* G = SwanGoddess.Get())
	{
		G->SetActorRotation(Rot);
		G->SetActorScale3D(FVector(1.0f - 0.6f * SwanForm));
		const bool bShow = SwanForm < 0.98f;
		if (G->IsHidden() == bShow) G->SetActorHiddenInGame(!bShow);
	}
	if (AActor* B = SwanBird.Get())
	{
		B->SetActorRotation(Rot);
		B->SetActorScale3D(FVector(0.4f + 0.6f * SwanForm));
		const bool bShow = SwanForm > 0.02f;
		if (B->IsHidden() == bShow) B->SetActorHiddenInGame(!bShow);
	}

	// 天鹅头的影子落在浮雕面上的哪一点，离空着的那一只多远
	SwanMissCm = 900.0f;
	if (Lit > 0.6f)
	{
		const FVector Lm = -Moon, Head = SwanHeadCm();
		const double Den = FVector::DotProduct(Lm, NgtSwanFaceN);
		if (FMath::Abs(Den) > 1.0e-6)
		{
			const double T = FVector::DotProduct(NgtSwanFace - Head, NgtSwanFaceN) / Den;
			SwanMissCm = float(FVector::Dist(Head + Lm * T, NgtSwanGap));
		}
	}
	const bool bOk = !bSwanSolved && SwanForm > 0.95f && SwanSlot == 0 && FMath::Abs(float(DysisGB::AngDiff(SwanYawNow, NgtSwanSlots[0]))) < 1.0f && SwanMissCm < NgtSwanMissCm;
	SwanSolvedT = bOk ? SwanSolvedT + Dt : 0.0f;
	if (SwanSolvedT > 0.7f && !bSwanSolved)
	{
		bSwanSolved = true;
		SwanOpenT = 0.001f;
		ADysisHUD::Notify(GetWorld(), DysisCopy::SwanPuzzleSolved, 5.2f);
	}
	// 浮雕和它后面的门一起沉下去；沉过一半就不挡了
	if (SwanOpenT > 0.0f && SwanOpenT < 3.0f)
	{
		SwanOpenT += Dt;
		const float K = DysisGB::Smoothstep(0.0f, 2.6f, SwanOpenT);
		if (AActor* Dr = SwanDoor.Get()) Dr->SetActorLocation(SwanDoorBase - FVector(0.0, 0.0, 260.0 * K));
		if (AActor* Rl = SwanRelief.Get()) Rl->SetActorLocation(SwanReliefBase - FVector(0.0, 0.0, 305.0 * K));
		if (K > 0.5f)
			for (AActor* A : { SwanDoor.Get(), SwanRelief.Get() })
				if (A && A->GetActorEnableCollision())
				{
					A->SetActorEnableCollision(false);
					if (UDysisWorldState* S = UDysisWorldState::Get(this)) S->BumpLight();
				}
	}
}

void ADysisDirector::DebugSetSwan(int32 Slot, float Form)
{
	SwanSlot = ((Slot % 8) + 8) % 8;
	SwanYawNow = NgtSwanSlots[SwanSlot];
	SwanForm = FMath::Clamp(Form, 0.0f, 1.0f);
	SwanSolvedT = 0.0f;
}

// ───────────────────────── 墙里的楼梯：窗和下门 ─────────────────────────

void ADysisDirector::UpdateStairs(float Dt)
{
	const FMoonstone* Rel = nullptr;
	for (const FMoonstone& Ms : Moonstones) if (Ms.Id == TEXT("moonRelief")) Rel = &Ms;
	const bool bWhen[2] = { bSwanSolved, Rel && Rel->K > 0.5f };
	for (int32 s = 0; s < 2; ++s)
	{
		FStairSet& Set = StairSets[s];
		// 朝外的窗：上门打开以后，从上往下一扇扇打开（石块往外退进墙里）
		if (Set.T < 0.0f && bWhen[s]) Set.T = 0.0f;
		if (Set.T >= 0.0f)
		{
			Set.T += Dt;
			for (int32 i = 0; i < Set.Windows.Num(); ++i)
			{
				FStairWindow& W = Set.Windows[i];
				if (W.Open >= 1.0f) continue;
				const float K = FMath::Clamp((Set.T - 0.25f * i) / 0.8f, 0.0f, 1.0f);
				if (K <= W.Open) continue;
				W.Open = K;
				if (AActor* A = W.Actor.Get())
				{
					A->SetActorLocation(W.Base + W.Base.GetSafeNormal2D() * (60.0 * K));
					if (K >= 0.99f) A->SetActorHiddenInGame(true);
					if (K > 0.3f && !W.bGone)
					{
						W.bGone = true;
						A->SetActorEnableCollision(false);
						if (UDysisWorldState* S = UDysisWorldState::Get(this)) S->BumpLight();
					}
				}
			}
		}
		// 下门：上面的门开了，下面的石板也沉下去
		if (bWhen[s] && Set.SealOpen < 1.0f)
		{
			Set.SealOpen = DysisGB::Toward(Set.SealOpen, 1.0f, 0.45f, Dt);
			if (AActor* A = Set.Seal.Get())
			{
				A->SetActorLocation(Set.SealBase - FVector(0.0, 0.0, 260.0 * DysisGB::Smoothstep(0.0f, 1.0f, Set.SealOpen)));
				if (Set.SealOpen > 0.5f && A->GetActorEnableCollision())
				{
					A->SetActorEnableCollision(false);
					if (UDysisWorldState* S = UDysisWorldState::Get(this)) S->BumpLight();
				}
			}
		}
	}
}

// ───────────────────────── 月石 ─────────────────────────

ADysisDirector::FMoonstone* ADysisDirector::FindMoonstone(FName Id)
{
	for (FMoonstone& Ms : Moonstones) if (Ms.Id == Id) return &Ms;
	return nullptr;
}

void ADysisDirector::SetMoonstoneState(FMoonstone& Ms, float K)
{
	// 灰盒 setMoonstoneState：过半就算显形——显形的部分能踩了，原来挡着的不挡了
	const bool bWasShown = Ms.K > 0.5f, bShown = K > 0.5f;
	Ms.K = K;
	for (const TWeakObjectPtr<AActor>& P : Ms.Parts)
		if (AActor* A = P.Get())
		{
			const bool bVisible = K > 0.02f;
			if (A->IsHidden() == bVisible) A->SetActorHiddenInGame(!bVisible);
			if (A->GetActorEnableCollision() != bShown) A->SetActorEnableCollision(bShown);
		}
	for (const TWeakObjectPtr<AActor>& B : Ms.Blocks)
		if (AActor* A = B.Get())
		{
			const bool bVisible = K < 0.98f;
			if (A->IsHidden() == bVisible) A->SetActorHiddenInGame(!bVisible);
			if (A->GetActorEnableCollision() == bShown) A->SetActorEnableCollision(!bShown);
		}
	if (bWasShown != bShown)
		if (UDysisWorldState* S = UDysisWorldState::Get(this)) S->BumpLight();
}

bool ADysisDirector::MoonSampleLit(const FMoonstone& Ms, const FVector& P, const TCHAR** OutWhy) const
{
	auto Why = [OutWhy](const TCHAR* W, bool bResult) { if (OutWhy) *OutWhy = W; return bResult; };
	const UWorld* World = GetWorld();
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	if (!World || !State) return Why(TEXT("none"), false);
	FCollisionQueryParams Params(TEXT("DysisMoonstone"), /*bTraceComplex*/ true);
	if (const APlayerController* PC = World->GetFirstPlayerController())
		if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);
	if (Ms.Source == 1)
	{
		// 月亮直射：朝月亮看过去没被挡住（它自己挡着的那几块不算）
		const UDysisTimeComponent* Time = PlayerTime();
		const FVector Moon = UDysisSkyLibrary::DysisMoonDir(Time ? Time->H : 0.0f);
		if (Moon.Z < 0.01) return Why(TEXT("down"), false);
		for (const TWeakObjectPtr<AActor>& B : Ms.Blocks) if (B.IsValid()) Params.AddIgnoredActor(B.Get());
		return World->LineTraceTestByChannel(P + Moon * 5.0, P + Moon * 40000.0, ECC_Visibility, Params) ? Why(TEXT("blk"), false) : Why(TEXT("lit"), true);
	}
	// 经三相像反射的月光：从取样点沿反射光倒推回镜面，看落点在不在镜子被照亮的那一块上、中间有没有东西挡着
	if (!MoonMirrorBeam.IsValid())
		for (TActorIterator<ADysisBeamActor> It(World); It; ++It)
			if (It->GreyboxId == TEXT("mmoon")) { MoonMirrorBeam = *It; break; }
	const ADysisBeamActor* Beam = MoonMirrorBeam.Get();
	const FDysisMirrorState& M = State->Mirror;
	if (!Beam || !Beam->HasLight() || !M.bValid || !M.bHasLitBox || M.LitBoxForm != 2) return Why(TEXT("nobeam"), false);
	const FVector R = Beam->GetTravelDir();
	const double Rn = FVector::DotProduct(R, M.N);
	if (Rn <= 0.01) return Why(TEXT("back"), false);
	const double S = FVector::DotProduct(P - M.Center, M.N) / Rn;
	if (S <= 30.0) return Why(TEXT("near"), false);
	const FVector Hit = P - R * S, Q = Hit - M.Center;
	const double U = FVector::DotProduct(Q, M.U) / M.WidthCm + 0.5, V = FVector::DotProduct(Q, M.V) / M.HeightCm + 0.5;
	if (U < M.LitBox[0] || U > M.LitBox[1] || V < M.LitBox[2] || V > M.LitBox[3]) return Why(TEXT("box"), false);
	for (const TWeakObjectPtr<AActor>& A : M.Self) if (A.IsValid()) Params.AddIgnoredActor(A.Get());
	return World->LineTraceTestByChannel(Hit + R * 12.0, Hit + R * (S - 28.0), ECC_Visibility, Params) ? Why(TEXT("blk"), false) : Why(TEXT("lit"), true);
}

bool ADysisDirector::PlayerOnMoonstone(const FMoonstone& Ms) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ACharacter* Char = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	const UCharacterMovementComponent* Move = Char ? Char->GetCharacterMovement() : nullptr;
	if (!Move) return false;
	// 脚下踩着它显形出来的那一块
	if (Move->IsMovingOnGround())
		if (const AActor* Ground = Move->CurrentFloor.HitResult.GetActor())
			for (const TWeakObjectPtr<AActor>& P : Ms.Parts) if (P.Get() == Ground) return true;
	// 或者人正站在它的门洞里
	if (Ms.bHasDoor)
	{
		const UDysisTimeComponent* Time = PlayerTime();
		const FVector Foot = Time ? Time->FootCm : Char->GetActorLocation();
		const double R = DysisGB::ROf(Foot);
		if (R > DysisGB::R_IN - 140.0 && R < DysisGB::R_OUT + 60.0 && FMath::Abs(DysisGB::AngDiff(DysisGB::AzOf(Foot), Ms.DoorAz)) < Ms.DoorHalfDeg + 1.5
			&& Foot.Z > Ms.DoorZ - 50.0 && Foot.Z < Ms.DoorZ + 250.0) return true;
	}
	return false;
}

void ADysisDirector::UpdateMoonstones(float Dt)
{
	// 照没照到每 0.08 秒算一次
	MoonstoneClock += Dt;
	const bool bEval = MoonstoneClock > 0.08f;
	if (bEval) MoonstoneClock = 0.0f;
	for (FMoonstone& Ms : Moonstones)
	{
		if (Ms.bDormant) continue;
		if (bEval && Ms.Source != 0)
		{
			int32 N = 0;
			if (bCaught) for (const FVector& P : Ms.Samples) if (MoonSampleLit(Ms, P)) ++N;
			Ms.LitFrac = Ms.Samples.Num() > 0 ? float(N) / float(Ms.Samples.Num()) : 0.0f;
		}
		Ms.bLit = Ms.LitFrac >= Ms.Need;
		if (Ms.bLit && Ms.bOneShot && !Ms.bPerm)
		{
			Ms.bPerm = true;
			if (Ms.PermCopy) ADysisHUD::Notify(GetWorld(), Ms.PermCopy, 5.2f);
		}
		const bool bOn = PlayerOnMoonstone(Ms);
		const float Target = (Ms.bPerm || Ms.bLit || (bOn && Ms.K > 0.3f)) ? 1.0f : 0.0f;
		const float K = DysisGB::Toward(Ms.K, Target, Target > 0.5f ? 2.5f : 0.5f, Dt);
		if (K != Ms.K) SetMoonstoneState(Ms, K);
	}
	// 月亮浮雕上那一轮银月亮跟着浮雕一起隐去
	if (AActor* Disk = MoonDisk.Get())
		if (const FMoonstone* Rel = FindMoonstone(TEXT("moonRelief")))
		{
			const bool bShow = Rel->K < 0.95f;
			if (Disk->IsHidden() == bShow) Disk->SetActorHiddenInGame(!bShow);
		}
}

// ───────────────────────── 测试用 ─────────────────────────

FString ADysisDirector::DebugMoonstoneLit(FName Id) const
{
	FString S;
	for (const FMoonstone& Ms : Moonstones)
		if (Ms.Id == Id)
			for (const FVector& P : Ms.Samples)
			{
				const TCHAR* Why = TEXT("?");
				MoonSampleLit(Ms, P, &Why);
				if (!S.IsEmpty()) S += TEXT(",");
				S += Why;
			}
	return S;
}

FString ADysisDirector::DescribeNight() const
{
	const FVector Head = SwanHeadCm();
	FString S = FString::Printf(TEXT("{\"swan\":{\"slot\":%d,\"yaw\":%.3f,\"lit\":%.3f,\"form\":%.3f,\"miss\":%.2f,\"solved\":%s,\"openT\":%.3f,\"head\":[%.2f,%.2f,%.2f],\"doorZ\":%.1f,\"reliefZ\":%.1f},\"stairs\":["),
		SwanSlot, SwanYawNow, SwanLit, SwanForm, SwanMissCm, bSwanSolved ? TEXT("true") : TEXT("false"), SwanOpenT, Head.X, Head.Y, Head.Z,
		SwanDoor.IsValid() ? SwanDoor->GetActorLocation().Z - SwanDoorBase.Z : 0.0, SwanRelief.IsValid() ? SwanRelief->GetActorLocation().Z - SwanReliefBase.Z : 0.0);
	for (int32 s = 0; s < 2; ++s)
	{
		const FStairSet& Set = StairSets[s];
		S += FString::Printf(TEXT("%s{\"t\":%.3f,\"seal\":%.3f,\"sealZ\":%.1f,\"win\":["), s ? TEXT(",") : TEXT(""), Set.T, Set.SealOpen, Set.Seal.IsValid() ? Set.Seal->GetActorLocation().Z - Set.SealBase.Z : 0.0);
		for (int32 i = 0; i < Set.Windows.Num(); ++i) S += FString::Printf(TEXT("%s%.3f"), i ? TEXT(",") : TEXT(""), Set.Windows[i].Open);
		S += TEXT("]}");
	}
	S += TEXT("],\"stones\":{");
	for (int32 i = 0; i < Moonstones.Num(); ++i)
	{
		const FMoonstone& Ms = Moonstones[i];
		S += FString::Printf(TEXT("%s\"%s\":{\"k\":%.3f,\"lit\":%.3f,\"perm\":%s}"), i ? TEXT(",") : TEXT(""), *Ms.Id.ToString(), Ms.K, Ms.LitFrac, Ms.bPerm ? TEXT("true") : TEXT("false"));
	}
	return S + TEXT("}}");
}
