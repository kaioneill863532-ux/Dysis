#include "DysisRainbowAlign.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisMPCComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Optics/DysisOpticsLibrary.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

ADysisRainbowAlign::ADysisRainbowAlign()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;   // 20 Hz：站位判定够用（§18.1 tick 纪律）
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ADysisSkyActor* ADysisRainbowAlign::ResolveSky() const
{
	ADysisSkyActor* Sky = CachedSky.Get();
	if (!Sky && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) { CachedSky = *It; Sky = *It; }
	}
	return Sky;
}

void ADysisRainbowAlign::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const ADysisSkyActor* Sky = ResolveSky();
	APawn* Pawn = nullptr;
	if (APlayerController* PC = (GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
		Pawn = PC->GetPawn();
	if (!Sky || !Pawn) { bAligned = false; return; }

	// 站位门：玩家在虹台站位附近才判（站在别处影子乱飞不判）。
	if (FVector::DistSquared2D(Pawn->GetActorLocation(), StandPointCm) > FMath::Square(StandRadiusCm))
	{
		bAligned = false;
		return;
	}

	// 影头点（§8.8）：相机（这里用 Pawn 眼高近似）沿 −SunDir 投到浮雕平面。
	const FVector SunDir = UDysisSkyLibrary::DysisSunDir(Sky->GetTime());
	ShadowHeadCm = UDysisOpticsLibrary::ShadowHeadPoint(Pawn->GetActorLocation(), SunDir, ReliefHeadCm, PlaneNormal);

	const double Dist = FVector::Dist(ShadowHeadCm, ReliefHeadCm);
	const bool bNow = Dist <= AlignToleranceCm;

	if (bNow && !bFired)
	{
		bAligned = true;
		bFired = true;
		OnAligned.Broadcast();   // 虹醒：虹桥/龛链挂这
		// 文案表·解谜后反馈：伊莉丝浮雕 → "虹从浮雕中走下，在你眼前铺展开。"
		ADysisHUD::Notify(GetWorld(), DysisCopy::IrisPuzzleSolved);

		// MPC 写 RainbowOn=1 驱动水帘材质整圈虹（运行时 MPC 已由 MPCFactory 创建）。
		if (GetWorld())
		{
			TActorIterator<APawn> ItPawn(GetWorld());
			if (ItPawn)
				if (UDysisMPCComponent* MPC = ItPawn->FindComponentByClass<UDysisMPCComponent>())
					if (MPC->Collection)
						UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), MPC->Collection, TEXT("RainbowOn"), 1.0f);
		}
		// MPC RainbowOn=1 已接（代码侧完成）；美术步骤：水帘材质图加 Collection Parameter "RainbowOn" 节点。
	}
	else if (!bNow && bReArmable)
	{
		bAligned = false;       // 可重触发模式：影头离开即灭（调试用）
		if (bFired) bFired = false;
	}

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
	// 调试：站位圈（绿）、人形中心（黄球）、影头（红球，对齐时变绿）。
	DrawDebugSphere(GetWorld(), StandPointCm, 15.0f, 8, FColor::Green, false, PrimaryActorTick.TickInterval, 0, 1.5f);
	DrawDebugSphere(GetWorld(), ReliefHeadCm, 12.0f, 8, FColor::Yellow, false, PrimaryActorTick.TickInterval, 0, 1.5f);
	DrawDebugSphere(GetWorld(), ShadowHeadCm, 10.0f, 8, bAligned ? FColor::Green : FColor::Red, false, PrimaryActorTick.TickInterval, 0, 2.0f);
#endif
}
