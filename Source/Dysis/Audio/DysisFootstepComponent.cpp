// 狄西斯的日落回廊 · 玩家脚步、起跳、落地、掉落、回到落脚点的声音
#include "Audio/DysisFootstepComponent.h"

#include "Audio/DysisSfxSubsystem.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisTimeComponent.h"
#include "Surfaces/DysisLuminousStone.h"
#include "Surfaces/DysisMoonPath.h"
#include "Surfaces/DysisMoonSurface.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const FName GDysisFallLoopId(TEXT("Player.Fall"));
}

UDysisFootstepComponent::UDysisFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;   // 移动组件算完这一帧以后再看
}

void UDysisFootstepComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ACharacter>(GetOwner());
	if (!Character.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DysisFootstep: 挂在 %s 上，但它不是 Character，脚步声不工作"), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
	}
}

void UDysisFootstepComponent::SetWetSource(bool bFlowing, const FVector& LocationCm)
{
	bWetFlowing = bFlowing;
	WetLocation = LocationCm;
}

UDysisTimeComponent* UDysisFootstepComponent::ResolveTime() const
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetOwner())
	{
		Time = GetOwner()->FindComponentByClass<UDysisTimeComponent>();
		CachedTime = Time;
	}
	return Time;
}

FString UDysisFootstepComponent::DescribeFloor(const UPrimitiveComponent* Comp) const
{
	if (!Comp) return FString();
	if (CachedFloorComp.Get() == Comp) return CachedFloorNames;

	// 名字来源：Actor 名、大纲标签（编辑器里）、Actor Tag、组件名、网格资产名（打包后也在）。
	FString Names;
	if (const AActor* A = Comp->GetOwner())
	{
		Names += A->GetName();
#if WITH_EDITOR
		Names += TEXT("|") + A->GetActorLabel();
#endif
		for (const FName& Tag : A->Tags) Names += TEXT("|") + Tag.ToString();
	}
	Names += TEXT("|") + Comp->GetName();
	for (const FName& Tag : Comp->ComponentTags) Names += TEXT("|") + Tag.ToString();
	if (const UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
		if (const UStaticMesh* Mesh = SMC->GetStaticMesh())
			Names += TEXT("|") + Mesh->GetName();

	CachedFloorComp = Comp;
	CachedFloorNames = Names;
	return Names;
}

EDysisFootSurface UDysisFootstepComponent::ClassifySurface(const ACharacter* InCharacter) const
{
	const UCharacterMovementComponent* Move = InCharacter->GetCharacterMovement();
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	const FVector Loc = InCharacter->GetActorLocation();
	const float HalfHeight = InCharacter->GetCapsuleComponent() ? InCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const float FootZ = static_cast<float>(Loc.Z) - HalfHeight;

	// ① Dysis 自己的地面类型。
	const FHitResult& Hit = Move->CurrentFloor.HitResult;
	const AActor* FloorActor = Hit.GetActor();
	if (Cast<ADysisBeamActor>(FloorActor)) return EDysisFootSurface::Light;
	if (Cast<ADysisLuminousStone>(FloorActor)) return EDysisFootSurface::Moon;
	if (Cast<ADysisMoonSurface>(FloorActor) || Cast<ADysisMoonPath>(FloorActor)) return EDysisFootSurface::Shadow;

	// 夜光石本身没有碰撞（踩的是它下面的石墩网格）：离哪块夜光石很近就算月石。
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADysisLuminousStone> It(World); It; ++It)
		{
			const FVector P = It->GetActorLocation();
			if (FVector::DistSquared2D(P, Loc) < FMath::Square(80.0f) && FMath::Abs(P.Z - FootZ) < 200.0f)
				return EDysisFootSurface::Moon;
		}
	}

	// ② 区域名（时间系统报的，光柱/月桥/影桥/月光大道/虹桥）。
	if (const UDysisTimeComponent* Time = ResolveTime())
	{
		const FString Zone = Time->GetZoneOverride().IsEmpty() ? Time->Zone : Time->GetZoneOverride();
		for (const FDysisZoneSurfaceRule& R : S->ZoneSurfaceRules)
		{
			if (!R.ZonePrefix.IsEmpty() && Zone.StartsWith(R.ZonePrefix, ESearchCase::IgnoreCase)) return R.Surface;
		}
	}

	// ③ 地面的名字。
	const FString Names = DescribeFloor(Hit.GetComponent());
	if (!Names.IsEmpty())
	{
		for (const FDysisSurfaceRule& R : S->NameSurfaceRules)
		{
			if (!R.NameContains.IsEmpty() && Names.Contains(R.NameContains, ESearchCase::IgnoreCase)) return R.Surface;
		}
	}

	// ④ 水面高度以下：夜里是月光踏片/大道（水面上的月影），白天是浅水。
	if (FootZ < S->ShallowWaterTopCm)
	{
		const UDysisTimeComponent* Time = ResolveTime();
		return (Time && Time->bNight) ? EDysisFootSurface::Shadow : EDysisFootSurface::Water;
	}

	// ⑤ 瀑布流着时，近处的石面是湿的。
	if (bWetFlowing && FVector::DistSquared2D(Loc, WetLocation) < FMath::Square(S->WetStoneRadiusCm))
		return EDysisFootSurface::WetStone;

	return EDysisFootSurface::Stone;
}

FName UDysisFootstepComponent::StepKey(EDysisFootSurface InSurface, bool bRun) const
{
	switch (InSurface)
	{
	case EDysisFootSurface::Light:    return bRun ? FName(TEXT("Foot.Light.Run")) : FName(TEXT("Foot.Light.Walk"));
	case EDysisFootSurface::Moon:     return bRun ? FName(TEXT("Foot.Moon.Run")) : FName(TEXT("Foot.Moon.Walk"));
	case EDysisFootSurface::Shadow:   return bRun ? FName(TEXT("Foot.Shadow.Run")) : FName(TEXT("Foot.Shadow.Walk"));
	case EDysisFootSurface::Bronze:   return bRun ? FName(TEXT("Foot.Bronze.Run")) : FName(TEXT("Foot.Bronze.Walk"));
	case EDysisFootSurface::Water:    return bRun ? FName(TEXT("Foot.Water.Run")) : FName(TEXT("Foot.Water.Walk"));
	case EDysisFootSurface::WetStone: return FName(TEXT("Foot.WetStone.Walk"));
	case EDysisFootSurface::Stone:    break;
	}
	return bRun ? FName(TEXT("Foot.Stone.Run")) : FName(TEXT("Foot.Stone.Walk"));
}

FName UDysisFootstepComponent::LandKey(EDysisFootSurface InSurface) const
{
	switch (InSurface)
	{
	case EDysisFootSurface::Light:
	case EDysisFootSurface::Moon:
	case EDysisFootSurface::Shadow:   return FName(TEXT("Player.Land.Light"));
	case EDysisFootSurface::Bronze:   return FName(TEXT("Player.Land.Bronze"));
	case EDysisFootSurface::Water:    return FName(TEXT("Foot.Water.Run"));
	case EDysisFootSurface::WetStone:
	case EDysisFootSurface::Stone:    break;
	}
	return FName(TEXT("Player.Land.Stone"));
}

void UDysisFootstepComponent::OnTeleported(UDysisSfxSubsystem* Sfx, const FVector& Location, double Now)
{
	// 回档（掉下去/掉水）或光被挡退回原位：都是“被送回落脚点”。
	if (bFallSoundPlayed) Sfx->StopLoop(GDysisFallLoopId, 0.15f);
	bFallSoundPlayed = false;
	AirSeconds = 0.0f;
	SuppressLandUntil = Now + 0.8;
	const UDysisTimeComponent* Time = ResolveTime();
	Sfx->Play((Time && Time->bNight) ? FName(TEXT("Player.Respawn.Night")) : FName(TEXT("Player.Respawn.Day")), Location);
}

void UDysisFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* C = Character.Get();
	UWorld* World = GetWorld();
	UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(this);
	if (!C || !World || !Sfx || DeltaTime <= 0.0f) return;
	const UCharacterMovementComponent* Move = C->GetCharacterMovement();
	if (!Move) return;

	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	const FVector Loc = C->GetActorLocation();
	const double Now = World->GetTimeSeconds();

	// ── 传送（回档、退回原位）：一帧里挪得比速度能走的远得多 ──
	if (bHavePrev)
	{
		const float Moved = static_cast<float>(FVector::Dist(Loc, PrevLocation));
		const float Expected = static_cast<float>(Move->Velocity.Size()) * DeltaTime;
		if (Moved > FMath::Max(300.0f, Expected * 3.0f + 150.0f))
		{
			OnTeleported(Sfx, Loc, Now);
		}
	}
	PrevLocation = Loc;
	bHavePrev = true;

	if (!bEnabled)
	{
		bWasOnGround = Move->IsMovingOnGround();
		return;
	}

	const bool bGround = Move->IsMovingOnGround();
	const bool bFalling = Move->IsFalling();

	// ── 起跳（含 coyote 跳）──
	if (C->JumpCurrentCount > LastJumpCount)
	{
		Sfx->Play(FName(TEXT("Player.Jump")), Loc);
	}
	LastJumpCount = C->JumpCurrentCount;

	// ── 空中：计时；掉得久、掉得快就播“掉落”，接循环 ──
	if (bFalling)
	{
		if (bWasOnGround)
		{
			AirSeconds = 0.0f;
			AirTopZ = static_cast<float>(Loc.Z);
			bFallSoundPlayed = false;
		}
		AirSeconds += DeltaTime;
		AirTopZ = FMath::Max(AirTopZ, static_cast<float>(Loc.Z));
		if (!bFallSoundPlayed && AirSeconds > S->FallSoundMinAirSeconds && Move->Velocity.Z < -S->FallSoundMinSpeed && AirTopZ - Loc.Z > 250.0f)
		{
			bFallSoundPlayed = true;
			Sfx->Play(FName(TEXT("Player.Fall.Start")), Loc);
			Sfx->StartLoop(GDysisFallLoopId, FName(TEXT("Player.Fall.Loop")), Loc, 0.8f);
		}
	}

	// ── 落地 ──
	if (bGround && !bWasOnGround)
	{
		if (bFallSoundPlayed) Sfx->StopLoop(GDysisFallLoopId, 0.15f);
		bFallSoundPlayed = false;
		Surface = ClassifySurface(C);
		if (Now >= SuppressLandUntil && AirSeconds > 0.12f)
		{
			const float Vol = AirSeconds > S->HeavyLandingAirSeconds ? S->HeavyLandingVolume : 1.0f;
			Sfx->Play(LandKey(Surface), Loc, -1, Vol);
		}
		StepDistance = 0.0f;
	}
	bWasOnGround = bGround;

	// ── 走路：有移动输入、在地上、够快 → 按距离计步 ──
	if (bGround)
	{
		const float Speed2D = static_cast<float>(FVector(Move->Velocity.X, Move->Velocity.Y, 0.0).Size());
		const bool bInput = Move->GetCurrentAcceleration().SizeSquared() > 1.0f;
		if (bInput && Speed2D > S->MinStepSpeed)
		{
			const bool bRun = Speed2D > S->RunSpeedThreshold;
			const float StepLen = FMath::Max(bRun ? S->RunStepCm : S->WalkStepCm, 10.0f);
			if (!bWasMoving) StepDistance = StepLen * 0.55f;   // 一起步很快就是第一步
			StepDistance += Speed2D * DeltaTime;
			if (StepDistance >= StepLen)
			{
				StepDistance = FMath::Fmod(StepDistance, StepLen);
				Surface = ClassifySurface(C);
				const float Pitch = (Surface == EDysisFootSurface::WetStone && bRun) ? 1.04f : 1.0f;
				Sfx->Play(StepKey(Surface, bRun), Loc, -1, 1.0f, Pitch);
			}
			bWasMoving = true;
			LastMoveSpeed = Speed2D;
		}
		else
		{
			// 快走里急停：石头上偶尔蹭一下地。
			if (bWasMoving && LastMoveSpeed > S->RunSpeedThreshold * 0.6f && Surface == EDysisFootSurface::Stone && FMath::FRand() < 0.5f)
			{
				Sfx->Play(FName(TEXT("Foot.Stone.Scuff")), Loc);
			}
			bWasMoving = false;
			StepDistance = 0.0f;
		}
	}
	else
	{
		bWasMoving = false;
	}
}
