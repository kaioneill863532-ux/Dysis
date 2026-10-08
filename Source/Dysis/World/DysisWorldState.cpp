#include "DysisWorldState.h"
#include "DysisGreybox.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisHUD.h"
#include "UI/DysisDialogueComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// 灰盒常数（厘米）
	constexpr float WS_R_A = 1219.0f;            // 回廊内沿（中庭半径）
	constexpr float WS_CEIL_Z = 2950.0f;         // 天花板
	constexpr float WS_F3_Z = 2300.0f;           // 四层地面
	constexpr float WS_HARP_GAP0 = 156.06774f, WS_HARP_GAP1 = 195.86795f;   // 光阶那一段回廊的缺口（度）
	// 海峡上的水沫（开场就有）：从台地崖边到小岛，一条 9 m 宽的带子
	constexpr float WS_STRAIT_AZ = 23.73427f, WS_STRAIT_R0 = 3055.828f, WS_STRAIT_R1 = 5358.794f, WS_STRAIT_HALF_W = 450.0f, WS_STRAIT_Z0 = -1600.0f, WS_STRAIT_Z1 = -90.0f;
	// 远处的岩岛
	constexpr double WS_FAR_ISLE_AZ = 240.0, WS_FAR_ISLE_DIST = 46300.0, WS_FAR_ISLE_RIDGE = 5690.0;

	const ACharacter* PlayerCharacter(const UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	}

	// 控制台（测试用）：
	//   Dysis.Sluice 0|1            开 / 关水闸
	//   Dysis.Mist <浓度> <高度cm>   把雾直接摆成这样（之后不再自动变）
	//   Dysis.IsleGrow <0|1> <进度>  把开场的光摆到伸出一半之类
	//   Dysis.WorldRelease          放开上面两条，恢复自动
	FAutoConsoleCommandWithWorldAndArgs GDysisSluice(TEXT("Dysis.Sluice"), TEXT("开 / 关水闸：Dysis.Sluice 0|1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDysisWorldState* S = UDysisWorldState::Get(World)) S->SetSluiceOpen(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}));
	FAutoConsoleCommandWithWorldAndArgs GDysisMist(TEXT("Dysis.Mist"), TEXT("把雾摆成：Dysis.Mist <浓度 0–1> <升到的高度 cm>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDysisWorldState* S = UDysisWorldState::Get(World))
				S->DebugSetMist(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.0f, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 3150.0f);
		}));
	FAutoConsoleCommandWithWorldAndArgs GDysisIsleGrow(TEXT("Dysis.IsleGrow"), TEXT("把开场的光摆成：Dysis.IsleGrow <开始了没 0|1> <进度 0–1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDysisWorldState* S = UDysisWorldState::Get(World))
				S->DebugSetIsleGrow(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.0f);
		}));
	FAutoConsoleCommandWithWorld GDysisWorldRelease(TEXT("Dysis.WorldRelease"), TEXT("雾和开场的光恢复自动"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UDysisWorldState* S = UDysisWorldState::Get(World)) S->DebugRelease();
		}));
}

UDysisWorldState* UDysisWorldState::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDysisWorldState>() : nullptr;
}

bool UDysisWorldState::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UDysisWorldState::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDysisWorldState, STATGROUP_Tickables);
}

void UDysisWorldState::SetSluiceOpen(bool bOpen)
{
	bSluiceOpen = bOpen;
}

bool UDysisWorldState::IsNight() const
{
	if (const ACharacter* C = PlayerCharacter(GetWorld()))
		if (const UDysisTimeComponent* Time = C->FindComponentByClass<UDysisTimeComponent>())
			return Time->bNight;
	return false;
}

double UDysisWorldState::ShadowZ(const FVector& SunDir)
{
	const double Alt = FMath::Asin(FMath::Clamp(double(SunDir.Z), -1.0, 1.0));
	if (Alt <= 0.0) return 1.0e6;
	const double Diff = FMath::Fmod(FMath::Fmod(double(DysisGB::AzOf(SunDir)) - WS_FAR_ISLE_AZ, 360.0) + 540.0, 360.0) - 180.0;
	const double D = WS_FAR_ISLE_DIST / FMath::Max(0.2, FMath::Cos(FMath::DegreesToRadians(Diff)));
	return WS_FAR_ISLE_RIDGE - D * FMath::Tan(Alt);
}

float UDysisWorldState::MistAt(FVector P) const
{
	const float R = DysisGB::ROf(P);
	float M = 0.0f;
	// 中庭的雾：只在雾已经升到的高度以下；回廊里淡得多
	if (P.Z > -60.0 && P.Z < FMath::Min(WS_CEIL_Z, MistFrontCm))
	{
		const bool bHarpGap = P.Z > WS_F3_Z - 60.0 && P.Z < WS_CEIL_Z && R > WS_R_A - 30.0f && R < DysisGB::R_IN && DysisGB::InArc(DysisGB::AzOf(P), WS_HARP_GAP0, WS_HARP_GAP1);
		M += MistAmt * ((R < WS_R_A || bHarpGap) ? 1.0f : (R < DysisGB::R_IN ? 0.1f : 0.0f));
	}
	// 海峡上的水沫
	const float Cos = FMath::Cos(FMath::DegreesToRadians(WS_STRAIT_AZ)), Sin = FMath::Sin(FMath::DegreesToRadians(WS_STRAIT_AZ));
	const float Along = float(P.X) * Cos + float(P.Y) * Sin;
	const float Lat = FMath::Abs(float(P.X) * Sin - float(P.Y) * Cos);
	if (Along > WS_STRAIT_R0 && Along < WS_STRAIT_R1 && Lat < WS_STRAIT_HALF_W && P.Z > WS_STRAIT_Z0 && P.Z < WS_STRAIT_Z1) M += 1.0f;
	return M;
}

void UDysisWorldState::DebugSetMist(float Amt, float FrontCm)
{
	bDebugMist = true;
	MistAmt = Amt;
	MistFrontCm = FrontCm;
	bSluiceOpen = Amt > 0.0f;
}

void UDysisWorldState::DebugSetIsleGrow(bool bStarted, float K)
{
	bDebugGrow = true;
	bIsleGrowStarted = bStarted;
	IsleGrowK = K;
}

void UDysisWorldState::DebugRelease()
{
	bDebugMist = bDebugGrow = false;
}

void UDysisWorldState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!World || World->IsPaused()) return;

	// 水闸、雾（灰盒 updateNymph 开头三行）
	if (!bDebugMist)
	{
		FlowK = DysisGB::Toward(FlowK, bSluiceOpen ? 1.0f : 0.0f, bSluiceOpen ? 0.9f : 0.45f, DeltaTime);
		MistAmt = DysisGB::Toward(MistAmt, bSluiceOpen ? 1.0f : 0.0f, bSluiceOpen ? 0.1f : 0.2f, DeltaTime);
		if (bSluiceOpen) MistFrontCm = FMath::Min(WS_CEIL_Z + 200.0f, FMath::Max(MistFrontCm, DysisGB::WATER_Z) + DeltaTime * 300.0f);
		else if (MistAmt < 0.01f) MistFrontCm = -100.0f;
	}

	// 开场的光（灰盒 updateIsleGrow）：游戏开始了、开场对话说完了、人在岛上往神殿走了 0.8 m → 开始伸，3 秒伸到岛上
	if (!bDebugGrow)
	{
		if (!bIsleGrowStarted)
		{
			const APlayerController* PC = World->GetFirstPlayerController();
			const ADysisHUD* Hud = PC ? Cast<ADysisHUD>(PC->GetHUD()) : nullptr;
			const bool bStarted = !Hud || !Hud->IsMenuOpen();
			const bool bTalking = Hud && Hud->ActiveDialogue && Hud->ActiveDialogue->IsPlaying();
			const ACharacter* C = PlayerCharacter(World);
			const UDysisTimeComponent* Time = C ? C->FindComponentByClass<UDysisTimeComponent>() : nullptr;
			if (bStarted && !bTalking && C && Time && !Time->bNight && Time->Zone == TEXT("out"))
			{
				const FVector Foot = C->GetActorLocation() - FVector(0.0, 0.0, C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				if (DysisGB::ROf(Foot) < DysisGB::ROf(DysisGB::SpawnFootCm) - 80.0f) bIsleGrowStarted = true;
			}
		}
		if (bIsleGrowStarted && IsleGrowK < 1.0f) IsleGrowK = FMath::Min(1.0f, IsleGrowK + DeltaTime / 3.0f);
	}
}
