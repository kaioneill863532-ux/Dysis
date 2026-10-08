#include "DysisInteractComponent.h"
#include "Mechanisms/DysisInteractable.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

UDysisInteractComponent::UDysisInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDysisInteractComponent::RefreshCandidates() const
{
	Candidates.Reset();
	const UWorld* World = GetWorld();
	if (!World) return;
	for (TActorIterator<AActor> It(World); It; ++It)
		if (It->GetClass()->ImplementsInterface(UDysisInteractable::StaticClass())) Candidates.Add(*It);
}

bool UDysisInteractComponent::CanInteractNow(AActor*& OutTarget) const
{
	OutTarget = nullptr;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UWorld* World = GetWorld();
	if (!Character || !World) return false;

	// 候选名单一秒刷新一次（机关不会凭空多出来，不用每帧扫全关卡）
	if (World->GetTimeSeconds() >= NextRefreshTime)
	{
		RefreshCandidates();
		NextRefreshTime = World->GetTimeSeconds() + 1.0;
	}

	const FVector Foot = Character->GetActorLocation() - FVector(0.0, 0.0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	float Best = ReachCm;
	for (const TWeakObjectPtr<AActor>& Weak : Candidates)
	{
		AActor* Actor = Weak.Get();
		if (!Actor) continue;
		const FVector P = Actor->GetActorLocation();
		if (Foot.Z < P.Z - BelowCm || Foot.Z > P.Z + AboveCm) continue;
		const float D = FVector::Dist2D(Foot, P);
		if (D < Best) { Best = D; OutTarget = Actor; }
	}
	return OutTarget != nullptr;
}

bool UDysisInteractComponent::TryInteract()
{
	AActor* Target = nullptr;
	if (!CanInteractNow(Target)) return false;
	if (IDysisInteractable* Interactable = Cast<IDysisInteractable>(Target))
	{
		Interactable->Interact(Cast<APawn>(GetOwner()));
		return true;
	}
	return false;
}
