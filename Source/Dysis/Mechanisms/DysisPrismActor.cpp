#include "DysisPrismActor.h"
#include "Optics/DysisOpticsLibrary.h"
#include "Sky/DysisSkyLibrary.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "UI/DysisDialogueComponent.h"

ADysisPrismActor::ADysisPrismActor()
{
	PrimaryActorTick.bCanEverTick = false;   // 纯状态件：转格是离散事件，无 Tick
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
