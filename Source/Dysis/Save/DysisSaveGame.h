// 日落回廊 · 存档数据（M7 先行，调研 §18.4 存档最佳实践 + 开放问题 O-1 的最小答案）：
// 三龛收集 + 水闸状态（按名字）+ 昤夜（bNight/H）。第一字段是版本号——以后改结构就链式迁移（v1→v2→…），
// 永不 N 跳到最新。写入走子系统的"临时槽 → 原子轮换"，损坏时从 .bak 回滚。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DysisSaveGame.generated.h"

/** 三个时辰龛（game-design §8：日/虹/月）。 */
UENUM()
enum class EDysisNiche : uint8
{
	Sun        UMETA(DisplayName = "日之龛"),
	Rainbow    UMETA(DisplayName = "虹之龛"),
	Moon       UMETA(DisplayName = "月之龛"),
};

UCLASS()
class DYSIS_API UDysisSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 存档结构版本。改字段 = 版本 +1 并在子系统的迁移链里加一步。 */
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(VisibleAnywhere, Category = "Dysis|Save")
	int32 SaveVersion = CurrentVersion;

	// ───── 世界状态 ─────
	/** 昼夜与时刻（接光后夜里读档从这里恢复；H 是"上一刻"，进关后由站位重算）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	bool bNight = false;
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	float H = 22.369624f;   // 默认 H_I（岛上 13:29）

	// ───── 收集（三龛；集齐解锁黎明结局，O-5）─────
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	bool bNicheSun = false;
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	bool bNicheRainbow = false;
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	bool bNicheMoon = false;

	/** 打开过的水闸/进水口（机关只进不退——§14.2 单调原则，存档同规则）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Save")
	TArray<FName> OpenSluices;

	// ───── 小工具 ─────
	bool HasNiche(EDysisNiche Niche) const;
	void SetNiche(EDysisNiche Niche, bool bCollected = true);
	bool AllNichesCollected() const;   // 黎明结局判定（O-5）
	bool IsSluiceOpen(FName SluiceName) const;
};
