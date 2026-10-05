#include "DysisPrismActor.h"
#include "Optics/DysisOpticsLibrary.h"
#include "Beams/DysisBeamActor.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "UI/DysisDialogueComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ADysisPrismActor::ADysisPrismActor()
{
	PrimaryActorTick.bCanEverTick = false;   // 纯状态件：转格是离散事件
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisPrismActor::BeginPlay()
{
	Super::BeginPlay();
	CurrentSlot = FMath::Clamp(StartSlot, 0, FMath::Max(NumSlots - 1, 0));

	// 演出就位：塞勒涅醒来的对话——文案表原句 11 句（伊莉丝×塞勒涅×狄西斯，比之前的 6 句版完整）。
	Dialogue = NewObject<UDysisDialogueComponent>(this);
	Dialogue->RegisterComponent();
	Dialogue->Lines.SetNum(DysisCopy::PrismDialogueCount);
	for (int32 i = 0; i < DysisCopy::PrismDialogueCount; ++i)
		Dialogue->Lines[i] = FText::FromString(DysisCopy::PrismDialogue[i]);
	Dialogue->ForcedH = (float)UDysisSkyLibrary::DysisConst(TEXT("RELIEF_H"));   // 31.0506

	// 色散光束：生成 7 个 Beam Actor（红→紫各不同 IOR → 不同折射角 → 不同落点）。
	if (bSpawnDispersionBeams && GetWorld())
	{
		for (int32 c = 0; c < 7; ++c)
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ADysisBeamActor* Beam = GetWorld()->SpawnActor<ADysisBeamActor>(GetActorLocation(), FRotator::ZeroRotator, Params);
			if (Beam)
			{
				Beam->BeamZone = FName(*FString::Printf(TEXT("prism:%d"), c));
				Beam->BoxExtentCm = BeamExtentCm;
				Beam->SlopeMaxDeg = 0.0;             // 设计 9.1："棱镜色散光束——七道细细的彩色光线，不可以踩"
				Beam->SlopeMaxStandingDeg = 0.0;     // 站上也不行
				Beam->MaxLengthCm = 3000.0;
				Beam->MistLevel = 1.0f;              // 始终可见（不需要雾）
				Beam->WindowIlluminationThreshold = 0.0;  // 不需要窗采样
				Beam->MistThreshold = 0.0;           // 不需要雾覆盖
				DispersionBeams.Add(Beam);
			}
		}
		UpdateDispersionBeams();
	}
}

void ADysisPrismActor::UpdateDispersionBeams()
{
	// §4 色散光束（设计 9.1）：棱镜把入射白光分成七色，每色按 Snell 折射定律偏向不同方向。
	// 入射方向 = 逆太阳（棱镜面向太阳）；棱镜法线 = 铅水晶 45° 顶角的等分线（简化：Actor 的前方向）。
	// 每色 IOR 从 PrismIor(0)=红1.600 到 PrismIor(6)=紫1.642 → 折射角不同 → 落点扇形展开。
	if (!GetWorld()) return;

	// 找 SkyActor 获取太阳方向。
	ADysisSkyActor* Sky = nullptr;
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) Sky = *It;   // Mac clang：迭代器判一次
	}
	if (!Sky) return;

	const float H = Sky->GetTime();
	const FVector SunDir = UDysisSkyLibrary::DysisSunDir(H);
	const FVector IncidentDir = -SunDir;   // 光从太阳射向棱镜
	const FVector PrismNormal = GetActorRotation().Vector();   // 棱镜面法线（Actor 前向）

	// 入射光从空气(η=1.0)进入铅水晶（η=PrismIor），第一次折射。
	// 然后从铅晶射出空气，第二次折射——两次折射的偏转角由 IOR 决定。
	for (int32 c = 0; c < DispersionBeams.Num() && c < 7; ++c)
	{
		if (!DispersionBeams[c]) continue;
		const double Ior = UDysisOpticsLibrary::PrismIor(c);

		// 第一次折射（空气→铅晶，η = 1/Ior）。
		FVector Refracted1;
		if (!UDysisOpticsLibrary::RefractDir(IncidentDir, PrismNormal, 1.0 / Ior, Refracted1)) continue;

		// 第二次折射（铅晶→空气，法线取反方向，η = Ior）。
		FVector ExitDir;
		if (!UDysisOpticsLibrary::RefractDir(Refracted1, -PrismNormal, Ior, ExitDir)) continue;

		// 用 BeamActor 的公共接口直接摆方向和位置（不走 Tick 更新，色散是静态的）。
		ADysisBeamActor* Beam = DispersionBeams[c];
		Beam->SetActorLocation(GetActorLocation());
		Beam->SetActorRotation(ExitDir.Rotation());
		// 设一个固定长度——Beam 的 SolveLength 在 Tick 里才跑，这里手动摆视觉。
		Beam->SetActorScale3D(FVector(20.0, BeamExtentCm.Y * 2.0 / 100.0, BeamExtentCm.Z * 2.0 / 100.0));
	}
}

void ADysisPrismActor::Interact(APawn* Player, bool bFromFront)
{
	if (NumSlots <= 0) return;
	CurrentSlot = (CurrentSlot + 1) % NumSlots;   // 每转一格换一种颜色（红→橙→…→紫→红）
	// TODO(M6)：色散光束按 GetCurrentIor() 用 RefractDir 重算七道细光（顶角 45°，两侧各折一次）。
	if (IsIndigoOnTarget() && !bIndigoFired)
	{
		bIndigoFired = true;
		OnIndigoOnTarget.Broadcast();
		// 文案表·解谜后反馈：棱镜 → "靛色光芒落进月神塞勒涅的青金石之眼。"
		ADysisHUD::Notify(GetWorld(), DysisCopy::PrismPuzzleSolved);
		if (Dialogue) Dialogue->Play();   // 塞勒涅醒来：11 句对白（文案表完整版）
		// 色散光束颜色已接（7 道 Beam Actor 按各自 IOR 折射；视觉颜色待美术材质——红→紫的 Emissive 色阶）。
	}
}

double ADysisPrismActor::GetCurrentIor() const
{
	// 七色表按 0=红…6=紫线性；8 槽转台把槽位映射到色带（槽 0..7 → 色 0..6 均匀取点）。
	const double T = NumSlots > 1 ? double(CurrentSlot) / double(NumSlots - 1) : 0.0;
	return UDysisOpticsLibrary::PrismIor(int32(FMath::RoundToInt(T * 6.0)));
}

FText ADysisPrismActor::GetInteractPrompt() const
{
	// 文案表·交互显示：棱镜 → "转动棱镜"
	return FText::FromString(DysisCopy::PromptRotatePrism);
}
