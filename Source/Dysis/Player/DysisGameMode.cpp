#include "DysisGameMode.h"
#include "DysisCharacter.h"
#include "Beams/DysisBeamActor.h"
#include "Mechanisms/DysisDirector.h"
#include "UI/DysisHUD.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADysisGameMode::ADysisGameMode()
{
	DefaultPawnClass = ADysisCharacter::StaticClass();
	HUDClass = ADysisHUD::StaticClass();
}

void ADysisGameMode::BeginPlay()
{
	Super::BeginPlay();
	// 机关总管：关卡里没摆就生成一个
	if (UWorld* World = GetWorld())
		if (!ADysisDirector::Get(World)) World->SpawnActor<ADysisDirector>();
	// 等关卡里摆好的光先认完自己的名字（它们的 BeginPlay），下一帧再补缺的
	GetWorldTimerManager().SetTimerForNextTick(this, &ADysisGameMode::EnsureWindowBeams);
}

void ADysisGameMode::EnsureWindowBeams()
{
	UWorld* World = GetWorld();
	if (!World) return;
	static const TCHAR* Ids[] = { TEXT("isle"), TEXT("b1"), TEXT("b2"), TEXT("iris"), TEXT("c"), TEXT("h1"), TEXT("h2"), TEXT("h3") };
	TSet<FName> Have;
	for (TActorIterator<ADysisBeamActor> It(World); It; ++It)
		if (It->IsGreybox()) Have.Add(It->GreyboxId);
	for (const TCHAR* Id : Ids)
	{
		if (Have.Contains(FName(Id))) continue;
		ADysisBeamActor* Beam = World->SpawnActorDeferred<ADysisBeamActor>(ADysisBeamActor::StaticClass(), FTransform::Identity);
		if (!Beam) continue;
		Beam->GreyboxId = FName(Id);
		Beam->FinishSpawning(FTransform::Identity);
		UE_LOG(LogTemp, Display, TEXT("Dysis：关卡里没有窗光 %s，已生成"), Id);
	}
}
