// 日落回廊 · 存档子系统（M7 先行）：GameInstance 子系统（任何 GameInstance 都自动挂，工程不用配类）。
// 写入纪律（调研 §18.4 / Meta 最佳实践）：
//   ① 原子：先写临时槽 DysisTmp.sav，成功后文件级轮换（目标→.bak，临时→目标）——中途断电不会留半档；
//   ② 回滚：目标损坏时从 .bak 读；.bak 也没有才开新档；
//   ③ 迁移：SaveVersion 落后就链式迁移（v1→v2→…，永不 N 跳），迁移后回写；
//   ④ 双槽：手动槽 DysisSave 与自动槽 DysisAutosave 分开轮换，互不覆盖。
// 用法：玩法侧改 GetCurrent() 后调 SaveNow(false=手动/true=自动)；MarkNiche/SetSluiceOpen 是便捷口。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DysisSaveGame.h"
#include "DysisSaveSubsystem.generated.h"

UCLASS()
class DYSIS_API UDysisSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 手动槽 / 自动槽（自动不覆盖手动，坏了各自从 .bak 回滚）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	FString ManualSlot = TEXT("DysisSave");
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	FString AutosaveSlot = TEXT("DysisAutosave");

	/** 当前内存档（玩法侧读改它，然后 SaveNow）。没有就 LoadOrCreate 出来。 */
	UDysisSaveGame* GetCurrent();

	/** 读档：目标 → .bak → 开新档；顺带做版本迁移。返回是否读到了旧档（false=新档）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Save")
	bool LoadOrCreate();

	/** 存档：临时槽写入成功后原子轮换。bAutosave = 写自动槽。返回是否成功。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Save")
	bool SaveNow(bool bAutosave);

	// ───── 玩法便捷口（改完不会自动存——由调用方决定 SaveNow 时机）─────
	UFUNCTION(BlueprintCallable, Category = "Dysis|Save")
	void MarkNiche(EDysisNiche Niche);

	UFUNCTION(BlueprintCallable, Category = "Dysis|Save")
	void SetSluiceOpen(FName SluiceName);

	/** 把档里的昼/夜恢复给玩家时间组件（读档后、进关稳定站位时调；H 只在夜里强制，白天由站位重算）。 */
	void ApplyNightTo(class UDysisTimeComponent* Time);

	/** 槽 → 文件：SaveGameToSlot 写 Saved/SaveGames/<槽>.sav（引擎自动补 .sav，备份槽同理）。 */
	static FString SlotPath(const FString& Slot);

	/** 目标→<槽>.bak.sav、临时→目标 的文件级原子轮换（IFileManager::Move 带覆盖；测试也直用它）。 */
	static void RotateFiles(const FString& Slot);

private:
	/** 版本迁移链：v1 是当前；以后每个 +1 在这里加一步 MigrateV1ToV2(Save) 后 fallthrough。 */
	static void MigrateIfNeeded(UDysisSaveGame* Save);

	UPROPERTY(Transient)
	TObjectPtr<UDysisSaveGame> Current;
};
