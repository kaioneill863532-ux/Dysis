#include "DysisInteractComponent.h"
#include "Mechanisms/DysisInteractable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UDysisInteractComponent::UDysisInteractComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // 只在按键时干活（§18.1 tick 纪律）
}

bool UDysisInteractComponent::TraceFromCamera(FHitResult& OutHit) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !GetWorld()) return false;
	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return false;

	FVector ViewLoc;  FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);            // 屏幕中心 = 相机视线
	FCollisionQueryParams Params(TEXT("DysisInteract"), false, Pawn);
	return GetWorld()->LineTraceSingleByChannel(OutHit, ViewLoc, ViewLoc + ViewRot.Vector() * ReachCm, ECC_Visibility, Params);
}

bool UDysisInteractComponent::CanInteractNow(AActor*& OutTarget) const
{
	OutTarget = nullptr;
	FHitResult Hit;
	if (!TraceFromCamera(Hit)) return false;
	AActor* HitActor = Hit.GetActor();
	if (HitActor && HitActor->GetClass()->ImplementsInterface(UDysisInteractable::StaticClass()))
	{
		OutTarget = HitActor;
		return true;
	}
	return false;
}

bool UDysisInteractComponent::TryInteract()
{
	AActor* Target = nullptr;
	if (!CanInteractNow(Target)) return false;
	// C++ 实现的接口直接 Cast 调用（蓝图实现的机型以后走 Execute_Interact 宏）。
	if (IDysisInteractable* Interactable = Cast<IDysisInteractable>(Target))
	{
		Interactable->Interact(Cast<APawn>(GetOwner()));
		return true;
	}
	return false;
}
