#include "DysisGameMode.h"
#include "DysisCharacter.h"
#include "Beams/DysisBeamActor.h"
#include "DysisGreybox.h"
#include "Mechanisms/DysisDirector.h"
#include "UI/DysisHUD.h"
#include "World/DysisInvisibleWall.h"
#include "World/DysisWorldState.h"
#include "HAL/IConsoleManager.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADysisGameMode::ADysisGameMode()
{
	DefaultPawnClass = ADysisCharacter::StaticClass();
	HUDClass = ADysisHUD::StaticClass();
}

namespace
{
	// Dysis.StartPad 0|1：开局落脚的那一块放回灰盒里小岛的位置（0，测试里和灰盒比光用）/ 海面上（1，游戏里的样子）
	FAutoConsoleCommandWithWorldAndArgs GDysisStartPad(TEXT("Dysis.StartPad"), TEXT("开局落脚的那一块：Dysis.StartPad 0 = 放回灰盒里小岛的位置（比对用），1 = 海面上"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADysisGameMode* GM = World ? Cast<ADysisGameMode>(World->GetAuthGameMode()) : nullptr) GM->DebugSetStartOnSea(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}));
}

void ADysisGameMode::DebugSetStartOnSea(bool bSea)
{
	if (bSea == bStartOnSea) return;
	bStartOnSea = bSea;
	for (const TWeakObjectPtr<AActor>& A : StartPadActors)
		if (A.IsValid()) A->AddActorWorldOffset(bSea ? DysisGB::IsleMoveCm : -DysisGB::IsleMoveCm);
	if (UDysisWorldState* State = UDysisWorldState::Get(this))
	{
		State->StartPadTopZ = bSea ? DysisGB::ISLAND_TOP_Z : DysisGB::ISLAND_TOP_Z - float(DysisGB::IsleMoveCm.Z);
		State->BumpLight();
	}
}

void ADysisGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	MoveStartOntoSea();
}

void ADysisGameMode::MoveStartOntoSea()
{
	UWorld* World = GetWorld();
	if (!World) return;
	auto Move = [](AActor* A)
	{
		TInlineComponentArray<USceneComponent*> Comps(A);
		for (USceneComponent* C : Comps) C->SetMobility(EComponentMobility::Movable);
		A->AddActorWorldOffset(DysisGB::IsleMoveCm);
	};
	// 岛认名字；护栏和出生点认位置（都在岛顶面那一圈以内，别处没有）
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		const FVector L = A->GetActorLocation();
		const bool bNearIsle = FVector2D::Distance(FVector2D(L), DysisGB::IsleWasCenterCm) < 800.0 && L.Z < -900.0;
		if (A->ActorHasTag(TEXT("SM_Island"))) { Move(A); A->SetActorHiddenInGame(true); StartPadActors.Add(A); }
		else if (bNearIsle && A->IsA<ADysisInvisibleWall>()) { Move(A); StartPadActors.Add(A); }
		else if (bNearIsle && A->IsA<APlayerStart>()) Move(A);
	}
	bStartOnSea = true;
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
	static const TCHAR* Ids[] = { TEXT("isle"), TEXT("b1"), TEXT("b2"), TEXT("iris"), TEXT("c"), TEXT("h1"), TEXT("h2"), TEXT("h3"), TEXT("oculus"), TEXT("mirror"), TEXT("mmoon") };
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
