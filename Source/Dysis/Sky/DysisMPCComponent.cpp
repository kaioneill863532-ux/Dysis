#include "DysisMPCComponent.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisTimeComponent.h"
#include "Sky/DysisMPCFactory.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UDysisMPCComponent::UDysisMPCComponent()
{
	PrimaryComponentTick.bCanEverTick = false;   // 事件驱动：只在 OnTimeChanged 时写（§18.1 tick 纪律）
}

void UDysisMPCComponent::BeginPlay()
{
	Super::BeginPlay();

	// Collection 没指定（美术还没建 MPC_Dysis 资产）→ 运行时自动创建（零编辑器依赖）。
	// 材质图用 Collection Parameter 节点按参数名引用——参数名见 DysisMPCFactory。
	if (!Collection)
	{
		Collection = UDysisMPCFactory::CreateDysisMPC(this);
		if (Collection) bWarnedNoCollection = true;   // 标记已用运行时创建，不再 Warning
	}

	if (UDysisTimeComponent* Time = ResolveTime())
		Time->OnTimeChanged.AddDynamic(this, &UDysisMPCComponent::HandleTimeChanged);
	WriteAll(ResolveTime() ? ResolveTime()->H : 0.0f);   // 开局先写一遍（首个变化前的初值）
}

UDysisTimeComponent* UDysisMPCComponent::ResolveTime()
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}

ADysisSkyActor* UDysisMPCComponent::ResolveSky()
{
	ADysisSkyActor* Sky = CachedSky.Get();
	if (!Sky && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) { CachedSky = *It; Sky = *It; }
	}
	return Sky;
}

void UDysisMPCComponent::HandleTimeChanged(float H)
{
	WriteAll(H);
}

void UDysisMPCComponent::SetMist(float InMist)
{
	Mist = FMath::Clamp(InMist, 0.0f, 1.0f);
	if (Collection && GetWorld())
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Collection, MistParam, Mist);
}

void UDysisMPCComponent::WriteAll(float H)
{
	if (!GetWorld()) return;
	if (!Collection)
	{
		if (!bWarnedNoCollection)
		{
			UE_LOG(LogTemp, Warning, TEXT("DysisMPCComponent：没指 Collection（美术建 MPC_Dysis 后指上来），参数不写、玩法不受影响"));
			bWarnedNoCollection = true;
		}
		return;
	}

	UDysisTimeComponent* Time = ResolveTime();
	const ADysisSkyActor* Sky = ResolveSky();

	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Collection, HParam, H);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Collection, NightParam, (Time && Time->bNight) ? 1.0f : 0.0f);
	UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Collection, MistParam, Mist);

	if (Sky)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(GetWorld(), Collection, MainLightParam, Sky->GetGreyboxIntensity());
		UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), Collection, SunDirParam, FLinearColor(Sky->GetSunDir()));
		UKismetMaterialLibrary::SetVectorParameterValue(GetWorld(), Collection, MoonDirParam, FLinearColor(Sky->GetMoonDir()));
	}
}
