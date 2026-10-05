#include "DysisCharacterMovement.h"
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
	BeamPin = Beam->WorldToParam(FootCm());
	PrePinFootCm = FootCm();   // §4 规格书"光被挡住时人退回原位"——记下站上光那一刻的位置
	Beam->NotifyStanding(true);          // 站上：坡度放宽到 38°、雾淡也不消失（灰盒"已站上"锁存）
}

void UDysisCharacterMovement::UnpinBeam()
{
	if (ADysisBeamActor* Old = StandingBeam.Get()) Old->NotifyStanding(false);
	StandingBeam = nullptr;
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
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	HandleDysisFloors(DeltaTime);
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
				if (!Pinned->IsWalkableNow())
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
	if (ADysisBeamActor* Beam = StandingBeam.Get())
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

	// ④ §7 回档：掉落超限 / 掉水 → 回上一个安全落脚点；稳固落地时记录新落脚点。
	if (IsMovingOnGround())
	{
		if (bFalling)
		{
			// 落地了：掉落距离超限 → 回档；否则重置状态开始记新落脚点。
			const float FallDist = FallStartZ - FootCm().Z;
			if (FallDist > FallRespawnThresholdCm)
			{
				RespawnAtFoothold();
			}
			bFalling = false;
		}
		// 稳固地面（非光/月石/桥——规格书 §7"在光上、月石上和几座桥上不记落脚点"）。
		const bool bOnSolidGround = !StandingBeam.IsValid() && !StandingSurface.IsValid();
		if (bOnSolidGround)
		{
			++FramesOnGround;
			if (FramesOnGround >= 3) RecordFoothold();   // 站稳 3 帧才记（防走过时闪记）
		}
	}
	else if (IsFalling())
	{
		if (!bFalling) { bFalling = true; FallStartZ = FootCm().Z; }

		// §7 掉水检测（规格书原话"掉进水池、海里就回到上一个安全落脚点"）：
		// 掉落中脚下 Z 低于水面线 且 不在任何虚拟面上（踏片/大道/光柱）→ 立即回档。
		if (FootCm().Z < WaterLevelCm && !StandingSurface.IsValid() && !StandingBeam.IsValid())
		{
			RespawnAtFoothold();
			bFalling = false;
		}
	}
	else
	{
		FramesOnGround = 0;
	}
}

void UDysisCharacterMovement::RecordFoothold()
{
	LastSafeFootholdCm = FootCm();
}

void UDysisCharacterMovement::RespawnAtFoothold()
{
	if (LastSafeFootholdCm.IsZero()) return;
	if (ACharacter* C = CharacterOwner)
	{
		const float HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		C->SetActorLocation(LastSafeFootholdCm + FVector(0, 0, HalfHeight + 2.0), false, nullptr, ETeleportType::TeleportPhysics);
		SetMovementMode(MOVE_Falling);
		Velocity = FVector::ZeroVector;
		// 文案表·游戏提示：坠落后回档 → "作为十二时辰之一，狄西斯轻松回到了上一个落脚点……"
		ADysisHUD::Notify(GetWorld(), DysisCopy::RespawnHint, 5.0f);
	}
}
