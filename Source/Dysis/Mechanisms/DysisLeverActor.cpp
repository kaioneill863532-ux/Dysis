#include "DysisLeverActor.h"
#include "Save/DysisSaveSubsystem.h"
#include "Surfaces/DysisMoonSurface.h"
#include "Sky/DysisMPCComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

ADysisLeverActor::ADysisLeverActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;   // 只在扳动动画期间 Tick（§18.1 tick 纪律）
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisLeverActor::BeginPlay()
{
	Super::BeginPlay();
	BaseQuat = GetActorQuat();
	bPulled = bStartPulled;
	CurrentAngleRad = bPulled ? SwingAngleRad : -SwingAngleRad;
	SetActorRotation(BaseQuat * FQuat(SwingAxis.GetSafeNormal(), CurrentAngleRad));
}

void ADysisLeverActor::Interact(APawn* Player, bool bFromFront)
{
	bPulled = !bPulled;
	OnToggled.Broadcast(bPulled);
	SetActorTickEnabled(true);
	// 水闸语义（存档四口③④）：拉下 = 闸开——写进存档（只进不退），过节点写自动档（原子+备份）。
	if (bPulled && GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UDysisSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UDysisSaveSubsystem>())
		{
			if (!SluiceName.IsNone()) Save->SetSluiceOpen(SluiceName);
			if (bAutosaveOnPull) Save->SaveNow(true);
		}
	}

	// 水闸的世界响应（设计 §5）：拉下切水面态 + 写 MPC 雾参数（光束可见度跟着变——§15.1）。
	if (bPulled && !WaterStateOnChange.IsNone() && GetWorld())
	{
		EDysisWaterState NewState = EDysisWaterState::Calm;
		if (WaterStateOnChange == TEXT("FlowOut")) NewState = EDysisWaterState::FlowOut;
		else if (WaterStateOnChange == TEXT("Ripple")) NewState = EDysisWaterState::Ripple;
		{
			TActorIterator<ADysisMoonSurface> It(GetWorld());
			if (It) It->SetState(NewState);   // 水面就一个（§13.2）；Mac clang：迭代器判一次
		}
		// 雾浓度（日1 开闸 → 雾起=光束显形；月4 关闸 → 雾散）。
		const float Mist = (NewState == EDysisWaterState::FlowOut) ? 1.0f : 0.0f;
		{
			TActorIterator<APawn> ItP(GetWorld());
			if (ItP)
				if (UDysisMPCComponent* MPC = ItP->FindComponentByClass<UDysisMPCComponent>())
					MPC->SetMist(Mist);
		}

		// 文案表·解谜后反馈：日1 开闸 → "水雾溋溋升起，一条光路出现在你眼前。"
		if (NewState == EDysisWaterState::FlowOut)
			ADysisHUD::Notify(GetWorld(), DysisCopy::WaterGateOpened);
		// 文案表·机关反馈：奇/偶交替 → "西边的窗打开了/关上了"（机关 AB 的双窗语义）。
		static int32 ToggleCount = 0;
		ADysisHUD::Notify(GetWorld(), (++ToggleCount % 2 == 1) ? DysisCopy::MechOddTrigger : DysisCopy::MechEvenTrigger);
	}
	// TODO(M6)：扳动的石机声（音 concurrency 走 §18.3 的宽松规则）。
}

void ADysisLeverActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 恒速扳到目标角（crypt-raider 的恒速门范式：简单、可跳过、不弹）。
	const float Target = bPulled ? SwingAngleRad : -SwingAngleRad;
	CurrentAngleRad = FMath::FInterpConstantTo(CurrentAngleRad, Target, DeltaTime, FMath::DegreesToRadians(SwingDegPerSec));
	SetActorRotation(BaseQuat * FQuat(SwingAxis.GetSafeNormal(), CurrentAngleRad));
	if (FMath::IsNearlyEqual(CurrentAngleRad, Target, 1e-3f))
		SetActorTickEnabled(false);   // 到位即休眠
}

FText ADysisLeverActor::GetInteractPrompt() const
{
	// 文案表·交互显示：水闸 → "打开水闸"；非水闸的机关拉杆用通用"转动雕像"。
	return SluiceName.IsNone() ? FText::FromString(TEXT("转动雕像"))
	       : FText::FromString(DysisCopy::PromptWaterGate);
}
