#include "DysisMoonSurface.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisMPCComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ADysisMoonSurface::ADysisMoonSurface()
{
	PrimaryActorTick.bCanEverTick = false;   // 纯被问：不 Tick（§18.1 tick 纪律）
}

void ADysisMoonSurface::SetState(EDysisWaterState NewState)
{
	State = NewState;

	// 写 MPC 标量驱动水面材质/雾源（运行时 MPC 已由 DysisMPCFactory 创建——零编辑器依赖）。
	// 参数名见 DysisMPCFactory：Ripple=波纹态 / Flow=流动瀑布态。
	if (GetWorld())
	{
		// 找玩家身上的 MPC 组件（自动创建过运行时 Collection）。
		TActorIterator<APawn> ItPawn(GetWorld());
		if (ItPawn)
			if (UDysisMPCComponent* MPC = ItPawn->FindComponentByClass<UDysisMPCComponent>())
			{
				if (MPC->Collection)
				{
					UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), MPC->Collection,
						TEXT("Ripple"), NewState == EDysisWaterState::Ripple ? 1.0f : 0.0f);
					UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), MPC->Collection,
						TEXT("Flow"), NewState == EDysisWaterState::FlowOut ? 1.0f : 0.0f);
				}
			}
	}
}

FVector ADysisMoonSurface::MoonDirNow() const
{
	ADysisSkyActor* Sky = CachedSky.Get();
	if (!Sky && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) { CachedSky = *It; Sky = *It; }
	}
	return Sky ? UDysisSkyLibrary::DysisMoonDir(Sky->GetTime()) : FVector::ZeroVector;
}

bool ADysisMoonSurface::IsMoonLitTile(const FVector& FootCm) const
{
	// 三重判定：点在水面盒内 + 月亮在地平线上 + **遮挡 trace**（从格点沿月亮方向打，被墙/井壁/屋檐
	// 挡住=月光照不到这格——月3"石格窗把月光切成一排踏片"正是这个：窗棂间的格子亮、窗棂下的暗）。
	// 性能：只在移动组件问到脚下格时调（每帧 1-2 条 trace），不是全水面逐格。
	if (FootCm.X < BoxMinCm.X || FootCm.X > BoxMaxCm.X
	 || FootCm.Y < BoxMinCm.Y || FootCm.Y > BoxMaxCm.Y
	 || FootCm.Z < BoxMinCm.Z || FootCm.Z > BoxMaxCm.Z) return false;

	const FVector M = MoonDirNow();
	if (M.Z <= 0.0) return false;

	// 遮挡 trace：从格心沿月亮方向打到天上。打不到任何东西 = 月光直射这格。
	if (GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(TEXT("DysisMoonLit"), true, const_cast<ADysisMoonSurface*>(this));
		const FVector Start = FootCm + FVector(0, 0, 10.0);
		return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + M * 10000.0, ECC_Visibility, Params);
	}
	return true;
}

bool ADysisMoonSurface::IsWalkableAt(const FVector& FootCm) const
{
	// 保留格优先（灰盒 2.3："脚下那一片始终保留，直到离开"——不检查视线，只查还站着）：
	if (bHasKeepTile && State == EDysisWaterState::Calm && IsMoonLitTile(KeepTileCm)
	    && FVector::DistSquared2D(FootCm, KeepTileCm) < FMath::Square(TileSizeCm))
		return true;

	// 三条件（灰盒 2.3）：水面静 + 月照到这格 + 视线面向月亮（<80°，用相机朝向，避免扭头掉水）。
	if (State != EDysisWaterState::Calm) return false;        // 波纹/流动：无踏片（大道走 MoonPathActor）
	if (!IsMoonLitTile(FootCm)) return false;

	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(const_cast<ADysisMoonSurface*>(this), 0))
	{
		const FVector ViewDir = Cam->GetCameraRotation().Vector();
		const double AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ViewDir.GetSafeNormal(), MoonDirNow()), -1.0, 1.0)));
		if (AngleDeg > ViewMoonMaxDeg) return false;
	}
	return true;
}

void ADysisMoonSurface::NoteWalkableAt(const FVector& FootCm)
{
	KeepTileCm = FootCm;      // 每帧刷新：跟脚走，离开前的最后一格就是"保留的那片"
	bHasKeepTile = true;
}

void ADysisMoonSurface::NoteDeparted()
{
	bHasKeepTile = false;     // 灰盒："直到离开它"
}
