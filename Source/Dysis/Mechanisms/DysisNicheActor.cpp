#include "DysisNicheActor.h"
#include "Save/DysisSaveSubsystem.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

ADysisNicheActor::ADysisNicheActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ADysisNicheActor::BeginPlay()
{
	Super::BeginPlay();
	// 读档后存档为真 → 本局直接视为已拿（重开不重给）。
	if (UDysisSaveSubsystem* Save = GetGameInstanceSave())
		bCollectedThisSession = Save->GetCurrent()->HasNiche(Niche);
}

bool ADysisNicheActor::IsCollected() const
{
	if (bCollectedThisSession) return true;
	UDysisSaveSubsystem* Save = GetGameInstanceSave();
	return Save && Save->GetCurrent()->HasNiche(Niche);
}

UDysisSaveSubsystem* ADysisNicheActor::GetGameInstanceSave() const
{
	return GetWorld() && GetWorld()->GetGameInstance()
	     ? GetWorld()->GetGameInstance()->GetSubsystem<UDysisSaveSubsystem>()
	     : nullptr;
}

void ADysisNicheActor::Interact(APawn* Player, bool bFromFront)
{
	if (IsCollected())
	{
		// 已拿的龛：提示"空了"（HUD 显示 GetInteractPrompt 的返回值，v1 生效）
		return;
	}
	if (UDysisSaveSubsystem* Save = GetGameInstanceSave())
	{
		Save->MarkNiche(Niche);                       // 存档四口①：收集
		if (bSaveManualOnCollect) Save->SaveNow(false);   // ②：重要节点写手动档（原子+备份）
	}
	bCollectedThisSession = true;
	OnCollected.Broadcast(Niche);

	// 文案表·机关反馈：三个龛各自的碎片获得文字。
	switch (Niche)
	{
	case EDysisNiche::Rainbow: ADysisHUD::Notify(GetWorld(), DysisCopy::IrisNicheOpened); break;
	case EDysisNiche::Sun:     ADysisHUD::Notify(GetWorld(), DysisCopy::SunNicheOpened); break;
	case EDysisNiche::Moon:    ADysisHUD::Notify(GetWorld(), DysisCopy::MoonNicheOpened); break;
	}
	// TODO(M6/M7)：开盖动画（Sequencer）、碎片光效、集齐三龛的黎明结局触发（AllNichesCollected）。
}

FText ADysisNicheActor::GetInteractPrompt() const
{
	if (IsCollected()) return FText::FromString(TEXT("（已空）"));
	// 文案表·交互显示：日之龛→"打开日之龛"；虹之龛→"打开虹之龛"；月之龛→"打开月之龛"
	switch (Niche)
	{
	case EDysisNiche::Sun:     return FText::FromString(DysisCopy::PromptOpenSunNiche);
	case EDysisNiche::Rainbow: return FText::FromString(DysisCopy::PromptOpenIrisNiche);
	case EDysisNiche::Moon:    return FText::FromString(DysisCopy::PromptOpenMoonNiche);
	}
	return FText::FromString(TEXT("打开"));
}
