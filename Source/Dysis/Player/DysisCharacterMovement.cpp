#include "DysisCharacterMovement.h"
#include "DysisGreybox.h"
#include "Mechanisms/DysisDirector.h"
#include "Optics/DysisVirtualSurface.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
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
	UpdateOneWayFloors(DeltaTime);
	ApplyGreyboxCarry();   // 光挪了，人先跟着挪，再处理这一帧的走动
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Velocity.Z < -MaxFallSpeedCm) Velocity.Z = -MaxFallSpeedCm;
	HandleDysisFloors(DeltaTime);
	UpdateBeamHeadroom();
	UpdateRespawn(DeltaTime);
}

void UDysisCharacterMovement::UpdateOneWayFloors(float DeltaTime)
{
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(UpdatedComponent);
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(UpdatedComponent);
	UWorld* World = GetWorld();
	if (!Body || !World) return;
	// 每秒重新找一遍带 Tag 的组件（光是开局后才生成的，有的板后来才出现）：
	//   DysisOneWay = 能踩的薄板（只托脚）；DysisNoTrap = 会忽然出现的护栏、挡块
	OneWayClock -= DeltaTime;
	if (OneWayClock <= 0.0f)
	{
		OneWayClock = 1.0f;
		OneWayFloors.Reset();
		for (TActorIterator<AActor> It(World); It; ++It)
			It->ForEachComponent<UPrimitiveComponent>(false, [this](UPrimitiveComponent* C) { if (C->ComponentHasTag(TEXT("DysisOneWay")) || C->ComponentHasTag(TEXT("DysisNoTrap"))) OneWayFloors.Add(C); });
	}
	const FVector Foot = FootCm();
	const double Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 30.0, HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0;
	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : OneWayFloors)
	{
		UPrimitiveComponent* C = Weak.Get();
		if (!C) continue;
		const bool bOn = C->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		bool bIgnore = false, bTouch = false;
		if (bOn)
		{
			// 组件在自己坐标里的半尺寸：盒子组件直接问；引擎方块（100 cm）按缩放算
			const UBoxComponent* Box = Cast<UBoxComponent>(C);
			const FVector Half = Box ? Box->GetScaledBoxExtent() : C->GetComponentScale().GetAbs() * 50.0;
			const FTransform& T = C->GetComponentTransform();
			const FVector AX = T.GetUnitAxis(EAxis::X), AY = T.GetUnitAxis(EAxis::Y), AZ = T.GetUnitAxis(EAxis::Z);
			const FVector D = Foot - T.GetLocation();
			// 脚的正上方（正下方）这块板的某一个面离脚有多高（沿竖直方向量；板可以是斜的、很短的一段，比如虹桥是 90 段接起来的）。
			// 脚不在这一面的正下方（四周放宽 45 cm）就不算。
			auto GapAbove = [&](double FaceLocalZ, double& OutGap)
			{
				if (FMath::Abs(AZ.Z) < 0.2) return false;   // 竖着的板不这么量
				OutGap = (FaceLocalZ - FVector::DotProduct(D, AZ)) / AZ.Z;
				const FVector Q = D + FVector(0.0, 0.0, OutGap);
				return FMath::Abs(FVector::DotProduct(Q, AX)) <= Half.X + 45.0 && FMath::Abs(FVector::DotProduct(Q, AY)) <= Half.Y + 45.0;
			};
			double Gap = 0.0;
			// ① 能踩的板：它的顶面比脚高出一步迈不上去的高度（人在它下面，或者贴着它的边）——不挡
			if (C->ComponentHasTag(TEXT("DysisOneWay"))) { if (GapAbove(Half.Z, Gap)) bIgnore = Gap > 50.0; }
			// ①′ 桥边的护栏：它是立在桥面上的，底边就是桥面。底边比脚高出那么多 = 人不在桥上（在桥底下、桥旁边）——也不挡，
			//     不然人站在桥头底下时桥一出现，两边是护栏、前面是压下来的桥面，就被圈在里面了
			else if (GapAbove(-Half.Z, Gap)) bIgnore = Gap > 50.0;
			// 人的身子（胶囊）现在是不是插在这个组件里：沿胶囊的中轴取 5 个点，看离盒子有没有一个半径那么近
			for (int32 i = 0; i < 5 && !bTouch; ++i)
			{
				const FVector P = D + FVector(0.0, 0.0, Radius + (HalfHeight - Radius) * 2.0 * i / 4.0);
				const double QX = FMath::Max(FMath::Abs(FVector::DotProduct(P, AX)) - Half.X, 0.0), QY = FMath::Max(FMath::Abs(FVector::DotProduct(P, AY)) - Half.Y, 0.0), QZ = FMath::Max(FMath::Abs(FVector::DotProduct(P, AZ)) - Half.Z, 0.0);
				bTouch = QX * QX + QY * QY + QZ * QZ < FMath::Square(Radius - 1.5);
			}
		}
		// ② 它开始挡人的那一刻（刚出现：虹桥长出来、护栏立起来；或者人从桥底下往桥头走，桥面低到“脚够得着”了）
		//    人的身子正好插在里面：先不挡，等人走出来了再算数——不然人会被卡死在里面
		const bool bBlocks = bOn && !bIgnore;
		bool& bWasBlocking = OneWayWasOn.FindOrAdd(Weak, bBlocks);
		if (bBlocks && !bWasBlocking && bTouch) OneWayTrapped.Add(Weak);
		else if (OneWayTrapped.Contains(Weak) && (!bOn || !bTouch)) OneWayTrapped.Remove(Weak);
		bWasBlocking = bBlocks;
		bIgnore = bIgnore || OneWayTrapped.Contains(Weak);
		if (bIgnore != OneWayIgnored.Contains(Weak))
		{
			Body->IgnoreComponentWhenMoving(C, bIgnore);
			if (bIgnore) OneWayIgnored.Add(Weak); else OneWayIgnored.Remove(Weak);
		}
	}
}

void UDysisCharacterMovement::UpdateBeamHeadroom()
{
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!Body) return;
	const ADysisBeamActor* Beam = StandingBeam.Get();
	const bool bOnMirror = Beam && Beam->GreyboxId == TEXT("mirror");
	if (bOnMirror)
	{
		if (HeadroomSlab.IsValid()) return;
		const ADysisDirector* Director = ADysisDirector::Get(this);
		if (AActor* Slab = Director ? Director->Piece(TEXT("SM_Floor_L3")) : nullptr)
		{
			Body->IgnoreActorWhenMoving(Slab, true);
			HeadroomSlab = Slab;
		}
	}
	else if (HeadroomSlab.IsValid() && IsMovingOnGround())
	{
		// 跳起来、掉下去的那一会儿先不恢复（人可能还有一截在楼板里），等踩到别的地面再说
		Body->IgnoreActorWhenMoving(HeadroomSlab.Get(), false);
		HeadroomSlab = nullptr;
	}
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
