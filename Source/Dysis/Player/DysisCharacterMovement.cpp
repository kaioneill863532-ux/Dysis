#include "DysisCharacterMovement.h"
#include "DysisGreybox.h"
#include "Optics/DysisVirtualSurface.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UDysisCharacterMovement::UDysisCharacterMovement()
{
	// 数值纪律：JumpZ=480 / AirControl=0.35 是灰盒现役值（DysisCharacter 构造器里设的），这里不动。
}

FVector UDysisCharacterMovement::FootCm() const
{
	// 和 DysisTimeComponent::ComputeFoot 同口径：胶囊中心 − 半高（判定用中心投影，见调研 §16.3-②）。
	if (const ACharacter* C = CharacterOwner)
		if (const UCapsuleComponent* Capsule = C->GetCapsuleComponent())
			return C->GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	return UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
}

void UDysisCharacterMovement::PinToBeam(ADysisBeamActor* Beam)
{
	if (!Beam) return;
	if (ADysisBeamActor* Old = StandingBeam.Get()) Old->NotifyStanding(false);
	StandingBeam = Beam;
	bCarryValid = false;
	BeamPin = Beam->WorldToParam(FootCm());
	PrePinFootCm = FootCm();   // §4 规格书"光被挡住时人退回原位"——记下站上光那一刻的位置
	Beam->NotifyStanding(true);          // 站上：坡度放宽到 38°、雾淡也不消失（灰盒"已站上"锁存）
}

void UDysisCharacterMovement::UnpinBeam()
{
	if (ADysisBeamActor* Old = StandingBeam.Get()) Old->NotifyStanding(false);
	StandingBeam = nullptr;
	bCarryValid = false;
}

void UDysisCharacterMovement::RetreatFromBeam()
{
	// §4 规格书原话"光被挡住时人退回原位"：beam 在脚下塌了（IsWalkableNow()=false 且宽限也过了），
	// 把人传回站上光之前的位置——不是掉下去，是"光把你放回了原位"。
	if (PrePinFootCm.IsZero()) return;
	if (ACharacter* C = CharacterOwner)
	{
		const float HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		C->SetActorLocation(PrePinFootCm + FVector(0, 0, HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
		SetMovementMode(MOVE_Falling);
		Velocity = FVector::ZeroVector;
		UnpinBeam();
		UE_LOG(LogTemp, Display, TEXT("Dysis 光被挡 → 退回原位 (%.0f, %.0f, %.0f)"), PrePinFootCm.X, PrePinFootCm.Y, PrePinFootCm.Z);
	}
}

void UDysisCharacterMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (bRespawning)
	{
		// 回落脚点的黑场里人不动（灰盒：respawning 期间不跑 updatePlayer）
		ConsumeInputVector();
		Velocity = FVector::ZeroVector;
		RespawnTimer -= DeltaTime;
		if (RespawnTimer <= 0.0f) FinishRespawn();
		return;
	}
	ApplyGreyboxCarry();   // 光挪了，人先跟着挪，再处理这一帧的走动
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Velocity.Z < -MaxFallSpeedCm) Velocity.Z = -MaxFallSpeedCm;
	HandleDysisFloors(DeltaTime);
	UpdateRespawn(DeltaTime);
}

void UDysisCharacterMovement::ApplyGreyboxCarry()
{
	// 灰盒 applyCarry：人站在光上时记着自己在光上的位置（横向 A、沿光 S）；时间一变光就挪了，人挪到新的同一个位置上。
	ADysisBeamActor* Beam = StandingBeam.Get();
	if (!Beam || !Beam->IsGreybox() || !bCarryValid || !IsMovingOnGround() || !UpdatedComponent) return;
	if (Beam->GetFrameSerial() == CarrySerial || !Beam->IsWalkableNow()) return;
	CarrySerial = Beam->GetFrameSerial();
	const FVector Delta = Beam->StripToWorld(CarryA, CarryS) - FootCm();
	if (Delta.SizeSquared() < 0.01) return;
	const FVector Before = UpdatedComponent->GetComponentLocation();
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	// 光摆进了墙里、人跟不过去（水平差 5 cm 以上）：这一帧不挪
	const FVector Now = UpdatedComponent->GetComponentLocation();
	if (FVector::Dist2D(Now, Before + Delta) > 5.0)
		UpdatedComponent->SetWorldLocation(Before, false, nullptr, ETeleportType::TeleportPhysics);
}

UDysisTimeComponent* UDysisCharacterMovement::ResolveTime()
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && CharacterOwner)
		if ((Time = CharacterOwner->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}

void UDysisCharacterMovement::HandleDysisFloors(float DeltaTime)
{
	if (!CharacterOwner) return;

	// ① 虚拟地面：脚下这格不亮 = 没有地（水面物理本体 NoCollision，§2.3）。
	//    同时做回写（M5）：站稳的面 NoteWalkableAt（踏片记 KeepTile）、离开的面 NoteDeparted；
	//    面上报区域名（月光大道 gbridge 等）→ 时间系统 SetZoneOverride，离开时清。
	AActor* FloorActor = CurrentFloor.HitResult.GetActor();
	IDysisVirtualSurface* Surface = (FloorActor && FloorActor->GetClass()->ImplementsInterface(UDysisVirtualSurface::StaticClass()))
	                              ? Cast<IDysisVirtualSurface>(FloorActor) : nullptr;

	if (IsMovingOnGround() && Surface)
	{
		if (!Surface->IsWalkableAt(FootCm()))
		{
			SetMovementMode(MOVE_Falling);   // 掉水 → 回档逻辑由掉落系统接（§2.3，M5）；Departed 由下帧的离面分支收尾
		}
		else
		{
			if (StandingSurface.Get() != FloorActor)
			{
				if (AActor* Old = StandingSurface.Get())
					if (IDysisVirtualSurface* OldVS = Cast<IDysisVirtualSurface>(Old)) OldVS->NoteDeparted();
				StandingSurface = FloorActor;
			}
			Surface->NoteWalkableAt(FootCm());
			FName Zone;
			if (UDysisTimeComponent* Time = ResolveTime())
			{
				if (Surface->GetZoneName(Zone)) { Time->SetZoneOverride(Zone.ToString()); bZoneFromSurface = true; }
				else if (bZoneFromSurface) { Time->ClearZoneOverride(); bZoneFromSurface = false; }
			}
		}
	}
	else if (StandingSurface.IsValid())
	{
		if (AActor* Old = StandingSurface.Get())
			if (IDysisVirtualSurface* OldVS = Cast<IDysisVirtualSurface>(Old)) OldVS->NoteDeparted();   // 起跳/换到物理地面：离开虚拟面
		StandingSurface = nullptr;
		if (bZoneFromSurface) { if (UDysisTimeComponent* Time = ResolveTime()) Time->ClearZoneOverride(); bZoneFromSurface = false; }
	}

	// ② 光路钉人：地面是光 → 钉；钉着但地面不再是它 → 解钉；光被挡（不可走）→ 退回原位（§4）。
	if (IsMovingOnGround())
	{
		ADysisBeamActor* FloorBeam = Cast<ADysisBeamActor>(CurrentFloor.HitResult.GetActor());
		if (FloorBeam && FloorBeam->IsWalkableNow())
		{
			if (StandingBeam.Get() != FloorBeam) PinToBeam(FloorBeam);
		}
		else if (StandingBeam.IsValid())
		{
			// §4 规格书"光被挡住时人退回原位"——beam 塌了但人还站着：
			// 如果 beam 已不可走（不是走出了光，是光本身塌了），退回原位而不是掉落。
			if (ADysisBeamActor* Pinned = StandingBeam.Get())
			{
				// 窗光照灰盒：光没了就是没了，人掉下去（掉得深会回落脚点）。旧算法的镜光、圆眼光还是退回原位。
				if (!Pinned->IsWalkableNow() && !Pinned->IsGreybox())
					RetreatFromBeam();   // 光塌了 → 退回站上光之前的位置
				else
					UnpinBeam();       // 光还好，只是走出了范围 → 正常解钉
			}
			else
				UnpinBeam();
		}
	}
	else if (StandingBeam.IsValid())
	{
		UnpinBeam();   // 起跳/坠落：解钉。"踏空 0.14 s 还站得住"由 beam 侧碰撞宽限 + Character coyote 兜住
	}

	// ③ 携带：钉着且在地上 → 按参数反算新世界位（灰盒"光路会带着人走"）。beam 在本帧晚些时候（PostPhysics）
	//    才按新 H 重摆，所以这里用的是上一帧摆好的变换——脚位与参数取自同一变换，几何自洽。
	if (ADysisBeamActor* Beam = StandingBeam.Get(); Beam && Beam->IsGreybox())
	{
		if (IsMovingOnGround())
		{
			// 护栏：两侧是深渊时往边上走会顺着光滑过去，而不是掉下去（灰盒 beamRail）
			FVector Foot = FootCm();
			if (Beam->ClampToRail(Foot))
			{
				FHitResult Hit;
				SafeMoveUpdatedComponent(Foot - FootCm(), UpdatedComponent->GetComponentQuat(), true, Hit);
			}
			// 记下人在光上的位置，下一帧光挪了好跟着挪
			Beam->WorldToStrip(FootCm(), CarryA, CarryS);
			CarrySerial = Beam->GetFrameSerial();
			bCarryValid = true;
		}
	}
	else if (Beam)
	{
		if (IsMovingOnGround())
		{
			const FVector NewFoot = Beam->ParamToWorld(BeamPin);
			const FVector Delta = NewFoot - FootCm();
			if (Delta.SizeSquared() > 1.0)
			{
				FHitResult Hit;
				SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
				if (Hit.IsValidBlockingHit())
					SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);  // 光摆进墙：顺墙滑（灰盒同款）
				Velocity = Delta / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);          // 注入平台速度：跳离保留惯性
			}
		}
	}
}

// ───────────────────────── 落脚点和回档（灰盒 updatePlayer 后半段 + respawn） ─────────────────────────

bool UDysisCharacterMovement::IsOnSafeGround()
{
	// 在光上、月石上和几座桥上不记落脚点
	if (StandingBeam.IsValid() || StandingSurface.IsValid()) return false;
	if (Cast<ADysisBeamActor>(CurrentFloor.HitResult.GetActor())) return false;
	if (UDysisTimeComponent* Time = ResolveTime())
	{
		const FString& Zone = Time->Zone;
		if (Zone.StartsWith(TEXT("beam:")) || Zone.StartsWith(TEXT("ms:"))
			|| Zone == TEXT("shadowbr") || Zone == TEXT("gbridge") || Zone == TEXT("rbridge") || Zone == TEXT("ledge"))
			return false;
	}
	return true;
}

void UDysisCharacterMovement::UpdateRespawn(float DeltaTime)
{
	if (!CharacterOwner || bRespawning) return;
	const FVector Foot = FootCm();
	if (!bHasFoothold) { LastSafeFootholdCm = Foot; bHasFoothold = true; }   // 开局站的地方先当落脚点

	if (IsMovingOnGround())
	{
		if (bAirborne)
		{
			// 落地：这一次掉了多高
			bAirborne = false;
			if (FallFromZ - Foot.Z > FallRespawnThresholdCm) { RespawnNow(); return; }
		}
		SafeSeconds += DeltaTime;
		if (SafeSeconds > 0.4f && IsOnSafeGround()) { LastSafeFootholdCm = Foot; SafeSeconds = 0.0f; }
	}
	else if (!bAirborne)
	{
		bAirborne = true;
		FallFromZ = Foot.Z;
	}

	// 掉进水池（或瀑布那一段的水里）、掉进海里
	const float R = DysisGB::ROf(Foot);
	const bool bOverWater = R < DysisGB::R_POOL || (R < DysisGB::R_IN && DysisGB::InArc(DysisGB::AzOf(Foot), DysisGB::SEAM0, DysisGB::SEAM1));
	if ((bOverWater && Foot.Z < DysisGB::WATER_Z - 50.0f) || Foot.Z < DysisGB::SEA_Z + 150.0f) RespawnNow();
}

void UDysisCharacterMovement::RespawnNow()
{
	if (bRespawning || !bHasFoothold || !CharacterOwner) return;
	bRespawning = true;
	RespawnTimer = RespawnFadeSeconds;
	Velocity = FVector::ZeroVector;
	if (ADysisHUD* Hud = ADysisHUD::Get(this)) Hud->FadeTo(1.0f, RespawnFadeSeconds * 0.9f);
}

void UDysisCharacterMovement::FinishRespawn()
{
	bRespawning = false;
	++RespawnCount;
	ACharacter* C = CharacterOwner;
	if (!C) return;
	UnpinBeam();
	const float HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	C->SetActorLocation(LastSafeFootholdCm + FVector(0, 0, HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Falling);   // 落 2 厘米站稳
	bAirborne = false;
	FallFromZ = LastSafeFootholdCm.Z;
	SafeSeconds = 0.0f;
	if (ADysisHUD* Hud = ADysisHUD::Get(this))
	{
		Hud->FadeTo(0.0f, RespawnFadeSeconds);
		Hud->ShowNotification(FText::FromString(DysisCopy::RespawnHint), 5.0f);
	}
}
