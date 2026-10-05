#include "DysisSaveSubsystem.h"
#include "Sky/DysisTimeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/ConsoleManager.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"

namespace
{
	/** SaveGameToSlot 写的是 Saved/SaveGames/<槽>.sav（含平台差异，走 SaveMessageHandler 的惯例路径）。 */
	FString ResolveSlotPath(const FString& Slot)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Slot + TEXT(".sav"));
	}
}

UDysisSaveGame* UDysisSaveSubsystem::GetCurrent()
{
	if (!Current)
	{
		Current = NewObject<UDysisSaveGame>(this);
		Current->SaveVersion = UDysisSaveGame::CurrentVersion;
	}
	return Current;
}

bool UDysisSaveSubsystem::LoadOrCreate()
{
	const FString Slot = ManualSlot;
	const FString Bak = Slot + TEXT(".bak");
	UDysisSaveGame* Loaded = nullptr;
	bool bFromBak = false;

	Loaded = Cast<UDysisSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Loaded)
	{
		Loaded = Cast<UDysisSaveGame>(UGameplayStatics::LoadGameFromSlot(Bak, 0));   // 目标坏 → .bak 回滚
		bFromBak = Loaded != nullptr;
	}
	if (!Loaded)
	{
		GetCurrent();   // 新档
		return false;
	}

	MigrateIfNeeded(Loaded);
	Current = Loaded;
	if (bFromBak)
		SaveNow(false);   // 从备份救回的档立刻回写到主槽（修复损坏的目标文件）
	return true;
}

bool UDysisSaveSubsystem::SaveNow(bool bAutosave)
{
	UDysisSaveGame* Save = GetCurrent();
	if (!Save) return false;
	Save->SaveVersion = UDysisSaveGame::CurrentVersion;

	// 原子写：先落临时槽，成功后再文件级轮换——临时槽写失败（断电/磁盘满）时目标档原封不动。
	const FString Tmp = TEXT("DysisTmp");
	if (!UGameplayStatics::SaveGameToSlot(Save, Tmp, 0))
		return false;
	RotateFiles(bAutosave ? AutosaveSlot : ManualSlot);
	return true;
}

void UDysisSaveSubsystem::RotateFiles(const FString& Slot)
{
	// 三个路径都走 ResolveSlotPath，保证和 SaveGameToSlot/LoadGameFromSlot 的"<槽>.sav"规则严格一致
	// （备份槽 = "<槽>.bak" → 文件 "<槽>.bak.sav"，读档侧 LoadGameFromSlot(Slot + ".bak") 正好对上）。
	IFileManager& FM = IFileManager::Get();
	const FString TmpPath = ResolveSlotPath(TEXT("DysisTmp"));
	const FString TargetPath = ResolveSlotPath(Slot);
	const FString BakPath = ResolveSlotPath(Slot + TEXT(".bak"));
	if (FM.FileExists(*TargetPath))
		FM.Move(*BakPath, *TargetPath, /*bReplace=*/true);   // 旧档 → 备份（覆盖更旧的）
	if (FM.FileExists(*TmpPath))
		FM.Move(*TargetPath, *TmpPath, /*bReplace=*/true);   // 临时 → 正档
}

void UDysisSaveSubsystem::MigrateIfNeeded(UDysisSaveGame* Save)
{
	// 链式迁移：以后 v2 出来时在这里 if (Save->SaveVersion < 2) { MigrateV1ToV2(Save); Save->SaveVersion = 2; }
	// 每步只升一版，逐级 fallthrough；永不 N 跳（调研 §18.4）。
	if (Save->SaveVersion < 1) Save->SaveVersion = 1;   // v0/野档：当 v1 处理（v1 是首发结构）
}

void UDysisSaveSubsystem::MarkNiche(EDysisNiche Niche)
{
	GetCurrent()->SetNiche(Niche, true);
}

void UDysisSaveSubsystem::SetSluiceOpen(FName SluiceName)
{
	UDysisSaveGame* Save = GetCurrent();
	if (!Save->OpenSluices.Contains(SluiceName))
		Save->OpenSluices.Add(SluiceName);   // 只进不退（§14.2 单调原则）
}

void UDysisSaveSubsystem::ApplyNightTo(UDysisTimeComponent* Time)
{
	if (!Time) return;
	const UDysisSaveGame* Save = GetCurrent();
	Time->bNight = Save->bNight;
	Time->Sticky = Save->H;   // 夜里读档：从存档时刻继续（白天 H 由站位重算，不覆盖）
}

// ── 控制台：Dysis.Save / Dysis.Load（实测回路的存档链验证）──────────────────────
//   Dysis.Save    手动档立即存（打印槽路径与 .bak 是否存在）
//   Dysis.Load    读档（目标→.bak 回滚）+ ApplyNightTo + 打印档内容摘要
namespace
{
	UDysisSaveSubsystem* GetSave(UWorld* World)
	{
		return World && World->GetGameInstance()
		     ? World->GetGameInstance()->GetSubsystem<UDysisSaveSubsystem>()
		     : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GDysisSave(
		TEXT("Dysis.Save"),
		TEXT("手动档立即存（原子+备份），打印结果"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (!GetSave(World)) { UE_LOG(LogTemp, Warning, TEXT("Dysis.Save：没有 GameInstance")); return; }
			const bool bOk = GetSave(World)->SaveNow(false);
			UE_LOG(LogTemp, Display, TEXT("Dysis.Save：%s（%s / bak=%s）"), bOk ? TEXT("已存") : TEXT("失败"),
				*UDysisSaveSubsystem::SlotPath(GetSave(World)->ManualSlot),
				IFileManager::Get().FileExists(*UDysisSaveSubsystem::SlotPath(GetSave(World)->ManualSlot + TEXT(".bak"))) ? TEXT("有") : TEXT("无"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GDysisLoad(
		TEXT("Dysis.Load"),
		TEXT("读档（.bak 回滚）+ 恢复昼夜 + 打印摘要"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			UDysisSaveSubsystem* Save = GetSave(World);
			if (!Save) { UE_LOG(LogTemp, Warning, TEXT("Dysis.Load：没有 GameInstance")); return; }
			const bool bHad = Save->LoadOrCreate();
			const UDysisSaveGame* D = Save->GetCurrent();
			// 存档四口④：把昼夜恢复给玩家时间组件（白天 H 由站位重算，不动）。
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
				if (APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
					if (UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>())
						Save->ApplyNightTo(Time);
			UE_LOG(LogTemp, Display, TEXT("Dysis.Load：%s v%d night=%d H=%.2f 龛=%d%d%d 水闸=%d 个"),
				bHad ? TEXT("读到旧档") : TEXT("开了新档"), D->SaveVersion, D->bNight ? 1 : 0, D->H,
				D->bNicheSun ? 1 : 0, D->bNicheRainbow ? 1 : 0, D->bNicheMoon ? 1 : 0, D->OpenSluices.Num());
		}));
}

FString UDysisSaveSubsystem::SlotPath(const FString& Slot)
{
	return ResolveSlotPath(Slot);
}
