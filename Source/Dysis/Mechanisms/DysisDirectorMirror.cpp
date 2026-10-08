// 机关总管 · 三相像和日之龛：灰盒 v0.12 的 mirrorStatue / placeMirror / updateMirrors / updateNiches。
//   · 三相像双手托一面铜镜，站在三层（L2）西边的高台上。照在她身上的光决定她是哪一相：
//     日相举镜（把阳光反射到对面日之龛的石台，镜光能踩）、月相递镜（夜里反射月光）、暗相垂镜（没被照到）。
//   · 底座边上有个绞盘，按一次转一格（6 格）。
//   · 镜面的位置、朝向、被照亮的那一块写进世界状态（UDysisWorldState::Mirror），镜光和月光反射从那里读。
//   · 日之龛：顺着镜光走到石台上，按 E 打开，得到太阳碎片。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "World/DysisWorldState.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// ───── 灰盒 v0.12 的数（厘米 / 度） ─────
	const FVector MirStatueTop(372.11, -1297.70, 1540.0);   // 雕像脚下（高台顶面）：方位 286°、半径 13.5 m
	constexpr float MirSlots[6] = { 68.89978f, 122.89978f, 153.191f, 206.191f, 259.191f, 312.191f };   // 底座能停的 6 个朝向（方位角）
	constexpr float MirTiltSun = 30.9004f, MirTiltMoon = 21.29284f, MirTiltDark = -35.0f;           // 三相各自的镜面仰角
	constexpr float MirOffsetCm = 55.0f, MirHeightCm = 95.0f;                                       // 镜子在身前多远、离脚多高
	constexpr float MirLevelYaw = 153.191f, MirLevelTilt = 30.9004f;                                // 模型摆的姿势：白天那一格、日相
	const FVector MirCrankPos(421.72, -1470.73, 1450.0);    // 绞盘：方位 286°、半径 15.3 m
	// 日之龛：三层东边挑出去的石台上（方位 96°）
	const FVector MirSunNichePos(-155.75, 1481.84, 2000.0);

	/** 镜面的三条轴（UE 坐标）：朝向 Yaw（方位角）、仰角 Tilt。 */
	void MirFrame(float YawDeg, float TiltDeg, FVector& U, FVector& V, FVector& N)
	{
		const float Y = FMath::DegreesToRadians(YawDeg), T = FMath::DegreesToRadians(TiltDeg);
		N = FVector(FMath::Cos(Y) * FMath::Cos(T), FMath::Sin(Y) * FMath::Cos(T), FMath::Sin(T));
		U = FVector(-FMath::Sin(Y), FMath::Cos(Y), 0.0);
		V = FVector::CrossProduct(N, U).GetSafeNormal();
		if (V.Z < 0.0) V = -V;
	}
	FQuat MirFrameQuat(const FVector& U, const FVector& V, const FVector& N)
	{
		return FQuat(FMatrix(U, V, N, FVector::ZeroVector));
	}
	void MirMakeMovable(AActor* A)
	{
		if (!A) return;
		if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>()) C->SetMobility(EComponentMobility::Movable);
	}
	/** 不挡光（光路用的是“可见性”射线），挡人、挡镜头不变。要在 MirMakeMovable 之后调：改可动性会让部件重新按网格资产的碰撞设置来一遍。 */
	void MirNoLight(AActor* A)
	{
		if (!A) return;
		if (UStaticMeshComponent* C = A->FindComponentByClass<UStaticMeshComponent>())
		{
			C->bUseDefaultCollision = false;
			C->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		}
	}
}

void ADysisDirector::SetupMirror()
{
	MirrorStatue = Piece(TEXT("SM_Mech_Mirror_Statue"));
	MirrorPlate = Piece(TEXT("SM_Mech_Mirror_Mirror"));
	// 灰盒里雕像、镜子、绞盘都只是模型：不挡光（西边那扇窗的光是穿过她照进来的），挡人的只有雕像的身子。
	// 镜子连人也不挡——人要从镜子跟前踏上镜光。
	if (AActor* S = MirrorStatue.Get()) { MirrorStatueYaw0 = S->GetActorRotation().Yaw; MirMakeMovable(S); MirNoLight(S); }
	if (AActor* P = MirrorPlate.Get()) { MirrorPlateBase = P->GetActorQuat(); MirMakeMovable(P); P->SetActorEnableCollision(false); }
	MirNoLight(Piece(TEXT("SM_Mech_Mirror_Crank")));
	if (!MirrorStatue.IsValid() || !MirrorPlate.IsValid()) UE_LOG(LogTemp, Warning, TEXT("Dysis 三相像：关卡里找不到雕像或镜子的部件"));

	if (UDysisWorldState* State = UDysisWorldState::Get(this))
	{
		State->Mirror.Self.Reset();
		for (const TCHAR* Name : { TEXT("SM_Mech_Mirror_Statue"), TEXT("SM_Mech_Mirror_Mirror"), TEXT("SM_Mech_Mirror_Crank") })
			if (AActor* A = Piece(Name)) State->Mirror.Self.Add(A);
	}
	MirrorSlot = 2;
	MirrorYawNow = MirSlots[MirrorSlot];
	MirrorW3[0] = 1.0f; MirrorW3[1] = MirrorW3[2] = 0.0f;
	MirrorForm = 0;
	PlaceMirror();

	// 日之龛：盖子、碎片
	SunLid = Piece(TEXT("SM_Mech_SunNiche_Lid"));
	SunShard = Piece(TEXT("SM_Mech_SunNiche_Shard"));
	// 盖子和碎片会动，灰盒里它们也没有碰撞：不挡人（不然掀盖子时会把站在跟前的人顶开）
	if (AActor* L = SunLid.Get()) { SunLidBase = L->GetActorQuat(); MirMakeMovable(L); L->SetActorEnableCollision(false); }
	if (AActor* Sh = SunShard.Get()) { SunShardBase = Sh->GetActorLocation(); MirMakeMovable(Sh); Sh->SetActorEnableCollision(false); Sh->SetActorHiddenInGame(true); }
}

void ADysisDirector::AddMirrorInteracts()
{
	{
		// 三相像底座边上的绞盘：按一次转一格
		FDysisInteract I;
		I.Id = TEXT("mirrorCrank");
		I.Pos = []() { return MirCrankPos; };
		I.ZRange = []() { return FVector2D(1420.0, 1630.0); };
		I.RadiusCm = 150.0f;
		I.Label = []() { return FText::FromString(DysisCopy::PromptRotateStatue); };
		I.Act = [this]() { MirrorSlot = (MirrorSlot + 1) % 6; };
		Interacts.Add(MoveTemp(I));
	}
	{
		// 日之龛：只能打开一次
		FDysisInteract I;
		I.Id = TEXT("sunNiche");
		I.Pos = []() { return MirSunNichePos; };
		I.ZRange = []() { return FVector2D(1960.0, 2160.0); };
		I.RadiusCm = 190.0f;
		I.When = [this]() { return !bSunNicheOpen; };
		I.Label = []() { return FText::FromString(DysisCopy::PromptOpenSunNiche); };
		I.Act = [this]() { bSunNicheOpen = true; SunNicheT = 0.001f; };
		Interacts.Add(MoveTemp(I));
	}
}

void ADysisDirector::PlaceMirror()
{
	// 灰盒 placeMirror：仰角按三相各占多少混出来；暗相时镜子垂下去 0.35 m
	const float Tilt = MirrorW3[1] * MirTiltSun + MirrorW3[2] * MirTiltMoon + MirrorW3[0] * MirTiltDark;
	FVector U, V, N;
	MirFrame(MirrorYawNow, Tilt, U, V, N);
	const float Y = FMath::DegreesToRadians(MirrorYawNow);
	const FVector Center = MirStatueTop + FVector(FMath::Cos(Y), FMath::Sin(Y), 0.0) * MirOffsetCm + FVector(0.0, 0.0, MirHeightCm - 35.0f * MirrorW3[0]);

	if (AActor* S = MirrorStatue.Get()) S->SetActorRotation(FRotator(0.0f, MirrorStatueYaw0 + (MirrorYawNow - MirLevelYaw), 0.0f));
	if (AActor* P = MirrorPlate.Get())
	{
		// 模型摆的是“白天那一格、日相”的姿势：从那个镜面坐标架转到现在的
		FVector U0, V0, N0;
		MirFrame(MirLevelYaw, MirLevelTilt, U0, V0, N0);
		const FQuat Delta = MirFrameQuat(U, V, N) * MirFrameQuat(U0, V0, N0).Inverse();
		P->SetActorLocationAndRotation(Center, Delta * MirrorPlateBase);
	}
	if (UDysisWorldState* State = UDysisWorldState::Get(this))
	{
		FDysisMirrorState& M = State->Mirror;
		M.bValid = true;
		M.Center = Center; M.U = U; M.V = V; M.N = N;
		M.Form = MirrorForm;
		for (int32 i = 0; i < 3; ++i) M.W3[i] = MirrorW3[i];
	}
}

bool ADysisDirector::MirrorSeesLight(const FVector& Start, const FVector& Dir) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;
	FCollisionQueryParams Params(TEXT("DysisMirrorLit"), /*bTraceComplex*/ true);
	if (const APlayerController* PC = World->GetFirstPlayerController())
		if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
	if (const UDysisWorldState* State = UDysisWorldState::Get(this))
		for (const TWeakObjectPtr<AActor>& A : State->Mirror.Self) if (A.IsValid()) Params.AddIgnoredActor(A.Get());
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It) Params.AddIgnoredActor(*It);
	return !World->LineTraceTestByChannel(Start + Dir * 5.0, Start + Dir * 40000.0, ECC_Visibility, Params);
}

void ADysisDirector::UpdateMirrors(float Dt)
{
	const UDysisTimeComponent* Time = PlayerTime();
	const float H = Time ? Time->H : 0.0f;
	const FVector Sun = UDysisSkyLibrary::DysisSunDir(H), Moon = UDysisSkyLibrary::DysisMoonDir(H);
	const bool bDay = !bCaught && Sun.Z > 0.01, bNight = bCaught && Moon.Z > 0.01;

	// 她面朝着光（水平方向相差不到 100°）、镜子中心照得到 → 是那一相
	const float Y = FMath::DegreesToRadians(MirrorYawNow);
	const FVector Face(FMath::Cos(Y), FMath::Sin(Y), 0.0);
	UDysisWorldState* State = UDysisWorldState::Get(this);
	const FVector Center = State ? State->Mirror.Center : MirStatueTop;
	auto Facing = [&](const FVector& Dir)
	{
		const double Hz = FMath::Sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y);
		return Dir.Z > 0.01 && (Face.X * Dir.X + Face.Y * Dir.Y) / FMath::Max(1.0e-6, Hz) > FMath::Cos(FMath::DegreesToRadians(100.0));
	};
	auto CenterLit = [&](const FVector& Dir) { return MirrorSeesLight(Center + (Face + FVector(0.0, 0.0, 0.3)) * 15.0, Dir); };
	int32 Form = 0;
	if (bDay && Facing(Sun) && !UDysisWorldState::InIsleShadow(Center, Sun) && CenterLit(Sun)) Form = 1;
	else if (bNight && Facing(Moon) && CenterLit(Moon)) Form = 2;

	bool bChanged = Form != MirrorForm;
	MirrorForm = Form;
	for (int32 i = 0; i < 3; ++i)
	{
		const float V = DysisGB::Toward(MirrorW3[i], i == Form ? 1.0f : 0.0f, 1.6f, Dt);
		if (V != MirrorW3[i]) { MirrorW3[i] = V; bChanged = true; }
	}
	// 底座朝着选中的那一格转过去（每秒 90°，走近路）
	const float D = float(DysisGB::AngDiff(MirSlots[MirrorSlot], MirrorYawNow));
	if (FMath::Abs(D) > 1.0e-3f) { MirrorYawNow += FMath::Clamp(D, -90.0f * Dt, 90.0f * Dt); bChanged = true; }

	if (bChanged)
	{
		PlaceMirror();
		if (State) State->BumpLight();
	}
}

void ADysisDirector::UpdateNiches(float Dt)
{
	// 日之龛（灰盒 updateNiches 的 SUNN 那一段）：盖子掀开，碎片升起来转两圈，2.6 秒时到手
	if (SunNicheT <= 0.0f) return;
	const float Before = SunNicheT;
	SunNicheT += Dt;
	const float T = SunNicheT;
	if (AActor* Lid = SunLid.Get())
	{
		const FVector Out = MirSunNichePos.GetSafeNormal2D();
		const FVector Axis(-Out.Y, Out.X, 0.0);   // 贴着墙的那条水平轴
		Lid->SetActorRotation(FQuat(Axis, -1.9f * DysisGB::Smoothstep(0.0f, 1.0f, T)) * SunLidBase);
	}
	if (AActor* Shard = SunShard.Get())
	{
		const bool bShow = T > 0.5f && T < 2.8f;
		Shard->SetActorHiddenInGame(!bShow);
		Shard->SetActorLocation(SunShardBase + FVector(0.0, 0.0, 40.0f * DysisGB::Smoothstep(1.0f, 2.0f, T)));
		Shard->AddActorWorldRotation(FRotator(0.0f, FMath::RadiansToDegrees(2.0f * Dt), 0.0f));
	}
	if (Before < 2.6f && T >= 2.6f)
	{
		if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->SetShard(0, true);
		ADysisHUD::Notify(GetWorld(), DysisCopy::SunNicheOpened, 4.0f);
	}
}

void ADysisDirector::DebugSetMirrorSlot(int32 Slot)
{
	MirrorSlot = ((Slot % 6) + 6) % 6;
}

void ADysisDirector::DebugSetNight(bool bNight)
{
	bCaught = bNight;
	if (UDysisTimeComponent* Time = PlayerTime()) Time->SetNight(bNight);
}

FString ADysisDirector::DescribeMirror() const
{
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const FDysisMirrorState* M = State ? &State->Mirror : nullptr;
	const float Tilt = MirrorW3[1] * MirTiltSun + MirrorW3[2] * MirTiltMoon + MirrorW3[0] * MirTiltDark;
	return FString::Printf(TEXT("{\"slot\":%d,\"yaw\":%.4f,\"tilt\":%.4f,\"form\":%d,\"w3\":[%.3f,%.3f,%.3f],\"center\":[%.2f,%.2f,%.2f],\"n\":[%.5f,%.5f,%.5f],\"sunNicheOpen\":%s,\"sunNicheT\":%.2f}"),
		MirrorSlot, MirrorYawNow, Tilt, MirrorForm, MirrorW3[0], MirrorW3[1], MirrorW3[2],
		M ? M->Center.X : 0.0, M ? M->Center.Y : 0.0, M ? M->Center.Z : 0.0, M ? M->N.X : 0.0, M ? M->N.Y : 0.0, M ? M->N.Z : 0.0,
		bSunNicheOpen ? TEXT("true") : TEXT("false"), SunNicheT);
}
