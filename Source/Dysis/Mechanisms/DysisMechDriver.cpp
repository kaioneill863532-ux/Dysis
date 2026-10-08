#include "DysisMechDriver.h"
#include "Mechanisms/DysisLeverActor.h"
#include "Mechanisms/DysisMirrorSource.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Surfaces/DysisMoonPath.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ADysisMechDriver::ADysisMechDriver()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;   // 需要每帧查触发条件；但速度极低（一个 bool 检查）
	PrimaryActorTick.TickInterval = 0.1f;             // 10 Hz 查触发够用；动画开始时切 0
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisMechDriver::BeginPlay()
{
	Super::BeginPlay();
	// 初始姿态：机关清单"摆的状态"列——开局在 From/隐藏位。
	if (TargetMesh)
	{
		BaseQuat = TargetMesh->GetActorQuat();
		ApplyAlpha();
	}
	// 拉杆绑触发。
	if (TriggerLever) TriggerLever->OnToggled.AddDynamic(this, &ADysisMechDriver::OnLeverToggled);
}

void ADysisMechDriver::OnLeverToggled(bool bPulled)
{
	if (bPulled) Activate();
}

void ADysisMechDriver::Activate()
{
	if (bActivated) return;   // 单调（§14.2）：激活过就不再触发
	bActivated = true;
	PrimaryActorTick.TickInterval = 0.0f;
	bTicked = false;

	// 文案表·解谜后反馈（自动匹配——天鹅浮雕解谜 → "白天鹅静立在月光里……"；其余不播避免噪音）。
	// 只有 Reveal 类机关才播（天鹅隐去/月石透开/月桥显形是解谜时刻）。
	if (Motion == EDysisMechMotion::Reveal && GetWorld())
	{
		// 天鹅特殊：隐去的是"天鹅浮雕"（bRevealToVisible=false 且物体名含 Swan）。
		if (!bRevealToVisible && TargetMesh && TargetMesh->GetName().Contains(TEXT("Swan")))
			ADysisHUD::Notify(GetWorld(), DysisCopy::SwanPuzzleSolved, 6.0f);

		// 文案表·提示：玩家下了月桥走到一层，再次与瀑布或抱月女神像交互
		// → "月光穿过水帘，从塞勒涅怀里的月亮上扫过去了。刚才在月桥上走过的，是哪一步？"
		// 条件：目标名含 Goddess/Waterfall + 关卡里的 DysisMoonPath 有 HasWalked 标记。
		if (TargetMesh && GetWorld())
		{
			const bool bIsGoddessOrWaterfall = TargetMesh->GetName().Contains(TEXT("Goddess"))
			                                  || TargetMesh->GetName().Contains(TEXT("Waterfall"));
			if (bIsGoddessOrWaterfall)
			{
				TActorIterator<class ADysisMoonPath> ItPath(GetWorld());
				if (ItPath && ItPath->HasWalked())
					ADysisHUD::Notify(GetWorld(), DysisCopy::MoonbridgeHint, 6.0f);
			}
		}
	}
}

void ADysisMechDriver::CheckTriggers()
{
	if (bActivated) return;

	// 铜镜转到了指定格（月2月石/月锁的"反射月光照到"简化：铜镜转向=光路已对准）。
	if (TriggerMirror && TriggerMirrorSlot >= 0)
		if (TriggerMirror->GetCurrentSlot() == TriggerMirrorSlot)
			Activate();

	// 入夜触发（天鹅/月桥/半桥都在入夜后激活）。
	if (bTriggerOnNight)
		if (UDysisTimeComponent* Time = ResolveTime())
			if (Time->bNight)
				Activate();

	// §5 月石采样点：在半径内取 8 个均匀点，逐点打月光遮挡 trace；
	// 被照到的比例 ≥ MoonstoneThreshold（灰盒 2/3）才触发。
	if (Motion == EDysisMechMotion::Reveal && !bRevealToVisible && TargetMesh && GetWorld())
	{
		// 取月光方向（复用 MoonSurface 的查找逻辑：找第一个 DysisSkyActor）。
		ADysisSkyActor* Sky = nullptr;
		{
			TActorIterator<ADysisSkyActor> ItSky(GetWorld());
			if (ItSky) Sky = *ItSky;
		}
		if (Sky)
		{
			const FVector MoonDir = UDysisSkyLibrary::DysisMoonDir(Sky->GetTime());
			if (MoonDir.Z > 0.0)   // 月亮在地平线上
			{
				const FVector Center = TargetMesh->GetActorLocation();
				int32 LitCount = 0;
				constexpr int32 NumSamples = 8;
				for (int32 i = 0; i < NumSamples; ++i)
				{
					const double Angle = 2.0 * UE_DOUBLE_PI * double(i) / double(NumSamples);
					const FVector SamplePoint = Center + FVector(FMath::Cos(Angle) * SampleRadiusCm, FMath::Sin(Angle) * SampleRadiusCm, 10.0);
					FHitResult Hit;
					FCollisionQueryParams MoonParams(TEXT("DysisMoonstone"), true, TargetMesh.Get());
					if (!GetWorld()->LineTraceSingleByChannel(Hit, SamplePoint, SamplePoint + MoonDir * 10000.0, ECC_Visibility, MoonParams))
						++LitCount;   // 打不到东西=月光照到这个采样点
				}
				const double Ratio = double(LitCount) / double(NumSamples);
				if (Ratio >= MoonstoneThreshold)
					Activate();
				else if (bCloseWhenMoonlightLeaves && bActivated && Alpha >= 1.0)
				{
					// §5"离开月光会合回去"：比例跌到门槛下 → 逆转动画（退回 Alpha=0）。
					bActivated = false; Alpha = 0.0; bTicked = false;
					PrimaryActorTick.TickInterval = 0.0f;
					if (TargetMesh) ApplyAlpha();
				}
			}
		}
	}
}

void ADysisMechDriver::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActivated)
	{
		CheckTriggers();
		return;
	}

	// 恒速推进到 Alpha=1，到位休眠。
	if (Alpha < 1.0)
	{
		const double Travel = (Motion == EDysisMechMotion::SlideWorld)
			? FVector::Dist(SlideFromCm, SlideToCm) / FMath::Max(Speed, 1.0)
			: FMath::Abs(RotateToDeg - RotateFromDeg) / FMath::Max(Speed, 1.0);
		Alpha = FMath::Min(Alpha + DeltaTime / FMath::Max(Travel, 0.01), 1.0);
		ApplyAlpha();
	}
	else if (!bTicked)
	{
		bTicked = true;
		PrimaryActorTick.TickInterval = 10.0f;   // 到位后降到 10s 一查（防意外重触发；§18.1 tick 纪律）
	}
}

void ADysisMechDriver::ApplyAlpha()
{
	if (!TargetMesh) return;

	switch (Motion)
	{
	case EDysisMechMotion::SlideWorld:
	{
		// 直线滑动：起点→终点线性插值（封门下沉/半桥伸出/双子推拉/外窗石块径向推）。
		TargetMesh->SetActorLocation(FMath::Lerp(SlideFromCm, SlideToCm, Alpha));
		break;
	}
	case EDysisMechMotion::RotateLocal:
	{
		// 绕本地轴旋转（天鹅 45°/格、闸轮、拉杆杆臂同款）。
		const double Angle = FMath::Lerp(RotateFromDeg, RotateToDeg, Alpha);
		TargetMesh->SetActorRotation(BaseQuat * FQuat(RotateAxis.GetSafeNormal(), FMath::DegreesToRadians(Angle)));
		break;
	}
	case EDysisMechMotion::Reveal:
	{
		// 显隐切换：月石隐去（碰撞关+隐藏）、月桥显形（碰撞开+显示）、外窗石块消失。
		// 触发以前是反过来的样子：要“显形”的（月桥……）开局看不见、踩不着、不挡光；要“隐去”的开局在场。
		// （原来开局不处理，夜里才出现的月桥白天就立在水庭上，踩上去时间会跳到夜里，还挡着日1 的光。）
		{
			const bool bShown = (Alpha >= 1.0) ? bRevealToVisible : !bRevealToVisible;
			TargetMesh->SetActorHiddenInGame(!bShown);
			TargetMesh->SetActorEnableCollision(bShown && (bRevealToVisible ? bRevealCollision : true));
		}
		break;
	}
	}
}

void ADysisMechDriver::Interact(APawn* Player, bool bFromFront)
{
	Activate();   // 手动兜口（闸轮/双子手动摸一下直接触发）
}

FText ADysisMechDriver::GetInteractPrompt() const
{
	// 文案表·交互显示：按运动类型给不同文字（双子=拉出雕像，闸轮=转动雕像，其余=操作机关）。
	switch (Motion)
	{
	case EDysisMechMotion::RotateLocal: return FText::FromString(TEXT("转动雕像"));
	case EDysisMechMotion::SlideWorld:  return FText::FromString(TEXT("拉出波吕丢刻斯雕像"));
	case EDysisMechMotion::Reveal:      return FText::FromString(TEXT("看看浮雕"));
	}
	return FText::FromString(TEXT("操作机关"));
}

UDysisTimeComponent* ADysisMechDriver::ResolveTime()
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}
