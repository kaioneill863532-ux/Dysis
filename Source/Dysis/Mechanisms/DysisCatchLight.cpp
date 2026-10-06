#include "DysisCatchLight.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Audio/DysisMusicManager.h"
#include "Interaction/DysisGoldenApple.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ADysisCatchLight::ADysisCatchLight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;   // 20 Hz 判定够用（§18.1）
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ADysisSkyActor* ADysisCatchLight::ResolveSky() const
{
	ADysisSkyActor* Sky = CachedSky.Get();
	if (!Sky && GetWorld())
	{
		TActorIterator<ADysisSkyActor> It(GetWorld());
		if (It) { CachedSky = *It; Sky = *It; }
	}
	return Sky;
}

UDysisTimeComponent* ADysisCatchLight::ResolveTime() const
{
	UDysisTimeComponent* Time = CachedTime.Get();
	if (!Time && GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (APawn* Pawn = PC->GetPawn())
				if ((Time = Pawn->FindComponentByClass<UDysisTimeComponent>())) CachedTime = Time;
	return Time;
}

void ADysisCatchLight::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bCaught) { SetActorTickEnabled(false); return; }   // 接过了就休眠（单调：夜不退）

	UDysisTimeComponent* Time = ResolveTime();
	ADysisSkyActor* Sky = ResolveSky();
	APawn* Pawn = nullptr;
	if (GetWorld())
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) Pawn = PC->GetPawn();
	if (!Time || !Sky || !Pawn || Time->bNight) return;

	// ① 玩家站上接光台（XY 距离 + 高度足够高=在顶层）。
	if (FVector::DistSquared2D(Pawn->GetActorLocation(), PlatformCm) > FMath::Square(StandRadiusCm)) return;
	if (Pawn->GetActorLocation().Z < PlatformCm.Z - 200.0f) return;   // 不在顶上（掉下去了）

	// ② 太阳落到"最后一缕"以下（贴海平线；美术按岛影微调 CatchAltDeg）。
	float SunAlt = 0.0f, MoonAlt = 0.0f;
	Sky->GetAltitudes(SunAlt, MoonAlt);
	if (SunAlt > (float)CatchAltDeg) return;

	// 接住了：天空交给月亮（Sticky=H 与灰盒 catchLight 一致，由 SetNight 内部做）。
	bCaught = true;
	Time->SetNight(true);
	OnCaught.Broadcast();

	// BGM 切夜：白天淡出 → 安静 → 夜晚《月光》淡入（README 接入步骤第三条）。
	if (ADysisMusicManager* Music = ADysisMusicManager::GetDysisMusicManager(this))
	{
		Music->SwitchToNight();
	}

	// 金苹果拿起（设计 §6 日5："举起来的苹果还在光里。接住最后一缕阳光"——光从这一刻开始陪你走夜路）。
	if (Apple) Apple->PickUp();

	// 桥门关上（机关清单·屋顶桥门："关着 -141.40°（封住桥尾），开着 -43.65°；接住最后一缕光以后关上"）。
	// 开局导入的姿态是开着——接光后恒速转回关门位（设计：入夜后逆时针才是向前，桥尾封住=只能走下行梯）。
	if (GetWorld() && !BridgeDoorName.IsEmpty())
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
			if (It->GetName().Contains(BridgeDoorName))
			{
				// 文案表/机关清单：桥门关位 -141.40°（Pitch/Roll 保持门柱原始值）。
				const FRotator Original = It->GetActorRotation();
				It->SetActorRotation(FRotator(Original.Pitch, -141.40f, Original.Roll));
				break;
			}

	// Lumen 强刷（调研 §12.1：全局光照变化要数秒传播，切主光当帧刷一次）。
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		PC->ConsoleCommand(TEXT("r.LumenScene.Lighting.ForceLightingUpdate 1"));
	// TODO(M6/M7)：苹果拿起动画（视觉上从架上到手边）、回头看虹门的红虹、和声切夜组。
}
