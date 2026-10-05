#include "DysisGoldenApple.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"
#include "UI/DysisDialogueComponent.h"
#include "Save/DysisSaveSubsystem.h"
#include "Save/DysisSaveGame.h"
#include "Engine/GameInstance.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ADysisGoldenApple::ADysisGoldenApple()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;   // FInterpTo 平滑要每帧（一个 Actor，成本可忽略）

	RootComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	if (USphereComponent* Sphere = Cast<USphereComponent>(RootComponent))
	{
		Sphere->InitSphereRadius(12.0f);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 不挡人不挡光（光路从它边上过）
	}

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded()) { Mesh->SetStaticMesh(SphereMesh.Object); Mesh->SetRelativeScale3D(FVector(0.24f)); }   // 直径 ~24 cm 的苹果

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetIntensity(LightIntensity);
	Glow->SetAttenuationRadius(500.0f);   // 照亮身边一小圈（夜光石的距离判定半径 3m 内能感应到）
	Glow->SetLightColor(FLinearColor(1.0f, 0.85f, 0.5f));   // 暖金（白天落日的余温）
}

UDysisTimeComponent* ADysisGoldenApple::ResolveTime()
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}

void ADysisGoldenApple::Interact(APawn* Player, bool bFromFront)
{
	// 文案表·交互显示：架上→"取下金苹果"；被托着→不可交互（不可收起）；终点→"放入金苹果"。
	if (State == EAppleState::OnArmillary) PickUp();
	// Placed 状态不可交互（结局已触发）。
}

FText ADysisGoldenApple::GetInteractPrompt() const
{
	// 文案表·交互显示：取下前→"取下金苹果"；取下后→"放入金苹果"（在月托附近）。
	return (State == EAppleState::OnArmillary)
		? FText::FromString(DysisCopy::PromptTakeApple)
		: FText::FromString(DysisCopy::PromptPlaceApple);
}

void ADysisGoldenApple::PickUp()
{
	if (State == EAppleState::Placed) return;   // 归亭后是结局态，拿不回来（单调）
	State = EAppleState::Carried;
	SetActorTickEnabled(true);

	// 文案表·解谜后反馈：得到金苹果 → "金苹果中盛满了日落夕阳……"
	ADysisHUD::Notify(GetWorld(), DysisCopy::AppleTaken, 5.0f);

	// 文案表·游戏提示：入夜 → "塞勒涅说过，她的月光能照出事物的另一面。"
	if (UDysisTimeComponent* Time = ResolveTime())
		if (Time->bNight)
			ADysisHUD::Notify(GetWorld(), DysisCopy::NightHint, 6.0f);
}

void ADysisGoldenApple::Place(FVector TargetCm)
{
	State = EAppleState::Placed;
	SetActorLocation(TargetCm);
	SetActorTickEnabled(false);
	Glow->SetLightColor(FLinearColor(0.85f, 0.9f, 1.0f));

	// 文案表·解谜后反馈：放下金苹果 → "金苹果被放到了托盘上……"
	ADysisHUD::Notify(GetWorld(), DysisCopy::ApplePlaced, 6.0f);

	// 文案表·结局 A/B：按是否集齐三碎片播不同对话（文案表·游戏剧情·浑天仪）。
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		UDysisSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UDysisSaveSubsystem>();
		if (Save)
		{
			const bool bAllCollected = Save->GetCurrent()->AllNichesCollected();
			UDysisDialogueComponent* Ending = NewObject<UDysisDialogueComponent>(this);
			Ending->RegisterComponent();
			const TCHAR* const* Lines = bAllCollected ? DysisCopy::EndingB : DysisCopy::EndingA;
			const int32 Count = bAllCollected ? DysisCopy::EndingBCount : DysisCopy::EndingACount;
			Ending->Lines.SetNum(Count);
			for (int32 i = 0; i < Count; ++i) Ending->Lines[i] = FText::FromString(Lines[i]);
			Ending->ForcedH = -1.0f;   // 结局不钉时刻——天色交给塞勒涅
			Ending->SecondsPerLine = 5.0f;
			Ending->Play();
		}
	}
}

void ADysisGoldenApple::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State != EAppleState::Carried)
	{
		SetActorTickEnabled(false);   // 静态/归亭不 Tick
		return;
	}

	// 托着走：漂浮跟随玩家头侧（FInterpTo 平滑 + 正弦呼吸浮动——"浮在身边"）。不 Attach：
	// 光路携带/跳跃的惯性世界在动，苹果用自己的插值跟，视觉上更有"捧着"的物理感（灰盒同款）。
	if (APawn* Pawn = (GetWorld() && GetWorld()->GetFirstPlayerController()) ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr)
	{
		BobPhase += DeltaTime * (2.0f * UE_DOUBLE_PI / FMath::Max(BobPeriodSec, 0.1f));
		const FVector Target = Pawn->GetActorLocation() + Pawn->GetActorRotation().RotateVector(CarryOffsetCm)
		                     + FVector(0, 0, BobAmplitudeCm * FMath::Sin(BobPhase));
		SetActorLocation(FMath::VInterpTo(GetActorLocation(), Target, DeltaTime, FollowSpeed));
		// 夜光石的触发：苹果在被托着时夜光石侧距离判定已生效（AppleActor 指向本 Actor）。
	}
	// 夜里换月光色温（冷暖对比，设计 §2.6："它带来冷暖对比"）。
	if (UDysisTimeComponent* Time = ResolveTime())
		Glow->SetLightColor(Time->bNight ? FLinearColor(0.8f, 0.88f, 1.0f) : FLinearColor(1.0f, 0.85f, 0.5f));
}
