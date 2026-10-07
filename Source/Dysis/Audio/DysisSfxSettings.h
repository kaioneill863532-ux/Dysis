// 狄西斯的日落回廊 · 音效设置（Project Settings → Game → Dysis 音效）
// 每一项音效一行：音量、快慢（音高）、随机幅度、2D/3D、衰减距离、冷却……都是滑块，改了立刻生效（PIE 里也是），
// 并自动存进 Config/DefaultGame.ini（提交它就等于把调好的数值交给大家）。
// 出厂值来自 DysisSfxDefaults.cpp（由 Art/Audio/tools/gen_sfx_table.py 生成）；ini 里缺的项会自动补上。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DysisSfxSettings.generated.h"

class USoundBase;

/** 分类：每类有一个总音量滑块。 */
UENUM(BlueprintType)
enum class EDysisSfxCategory : uint8
{
	Footsteps UMETA(DisplayName = "脚步"),
	Player    UMETA(DisplayName = "玩家动作"),
	Mechanism UMETA(DisplayName = "机关"),
	Story     UMETA(DisplayName = "剧情与奇观"),
	Ambience  UMETA(DisplayName = "环境"),
	UI        UMETA(DisplayName = "界面"),
};

/** 脚下的材质（决定换哪一组脚步）。 */
UENUM(BlueprintType)
enum class EDysisFootSurface : uint8
{
	Stone    UMETA(DisplayName = "石头/大理石"),
	Light    UMETA(DisplayName = "光路/虹桥"),
	Moon     UMETA(DisplayName = "月石/月桥/夜光石"),
	Shadow   UMETA(DisplayName = "影桥/月光踏片/月光大道"),
	Bronze   UMETA(DisplayName = "青铜"),
	Water    UMETA(DisplayName = "浅水"),
	WetStone UMETA(DisplayName = "湿石"),
};

/** 一项音效（一个游戏事件，可以有多个随机版本）。 */
USTRUCT(BlueprintType)
struct DYSIS_API FDysisSfxEvent
{
	GENERATED_BODY()

	/** 事件名（代码里用它播，例如 Foot.Stone.Walk）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "事件名"))
	FName Key;

	/** 中文说明。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "说明"))
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "分类"))
	EDysisSfxCategory Category = EDysisSfxCategory::Mechanism;

	/** 关掉 = 这一项不响。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "启用"))
	bool bEnabled = true;

	/** 音量（1 = 交付时对好的响度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "音量", ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "2.0"))
	float Volume = 1.0f;

	/** 快慢：UE 里速度和音高一起变（1.2 = 快 20%、高约 3 个半音；0.8 = 慢、低）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "快慢（音高）", ClampMin = "0.25", ClampMax = "4.0", UIMin = "0.5", UIMax = "2.0"))
	float Pitch = 1.0f;

	/** 每次随机的音量幅度（±，0.1 = 每次在 0.9–1.1 倍之间）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "音量随机 ±", ClampMin = "0.0", ClampMax = "0.9", UIMin = "0.0", UIMax = "0.5"))
	float VolumeJitter = 0.0f;

	/** 每次随机的快慢幅度（±，0.04 = 0.96–1.04 倍）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "快慢随机 ±", ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.2"))
	float PitchJitter = 0.0f;

	/** 2D = 不分方位、不随距离衰减（玩家自己的声音、界面、标题）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "2D（不分方位）"))
	bool b2D = false;

	/** 3D：这个半径以内都是满音量（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "满音量半径（厘米）", EditCondition = "!b2D", ClampMin = "0.0", UIMax = "5000.0"))
	float InnerRadiusCm = 300.0f;

	/** 3D：从满音量半径往外，再过这么远衰减到无声（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "衰减距离（厘米）", EditCondition = "!b2D", ClampMin = "1.0", UIMax = "10000.0"))
	float FalloffDistanceCm = 3000.0f;

	/** 有多个版本时，随机但不和上一次重复。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "随机不重复"))
	bool bNoRepeat = true;

	/** 同一项两次之间最短间隔（秒）；间隔内再触发就跳过。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "最短间隔（秒）", ClampMin = "0.0", UIMax = "10.0"))
	float CooldownSeconds = 0.0f;

	/** 同一项同时最多几个在响（多了就停掉最早的）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "同时最多几个", ClampMin = "1", UIMax = "8"))
	int32 MaxInstances = 4;

	/** 循环素材（只读说明：循环由代码开关，导入脚本已把这些 SoundWave 设成 Looping）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "循环"))
	bool bLoop = false;

	/** 声音文件（多个 = 随机版本；“第几级/第几格”这种按顺序取）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "声音文件"))
	TArray<TSoftObjectPtr<USoundBase>> Sounds;
};

/** 脚下材质的判定规则：地面 Actor 名字 / 大纲标签 / Tag / 网格资产名 里包含这段文字，就算这种材质。 */
USTRUCT(BlueprintType)
struct DYSIS_API FDysisSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "名字里包含"))
	FString NameContains;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "材质"))
	EDysisFootSurface Surface = EDysisFootSurface::Stone;
};

/** 区域名（时间系统的 Zone）→ 脚下材质。前缀匹配，例如 beam: 匹配所有光柱。 */
USTRUCT(BlueprintType)
struct DYSIS_API FDysisZoneSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "区域名开头是"))
	FString ZonePrefix;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dysis|Sfx", meta = (DisplayName = "材质"))
	EDysisFootSurface Surface = EDysisFootSurface::Stone;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FDysisSfxEdited, FName /*Key*/);

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Dysis 音效"))
class DYSIS_API UDysisSfxSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDysisSfxSettings();

	/** 取设置（第一次取时把 ini 里缺的出厂项补上）。 */
	static const UDysisSfxSettings* Get();
	static UDysisSfxSettings* GetMutable();

	// ───────── 总音量 ─────────

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "音效总音量", ClampMin = "0.0", ClampMax = "2.0"))
	float MasterVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "脚步", ClampMin = "0.0", ClampMax = "2.0"))
	float FootstepsVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "玩家动作", ClampMin = "0.0", ClampMax = "2.0"))
	float PlayerVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "机关", ClampMin = "0.0", ClampMax = "2.0"))
	float MechanismVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "剧情与奇观", ClampMin = "0.0", ClampMax = "2.0"))
	float StoryVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "环境", ClampMin = "0.0", ClampMax = "2.0"))
	float AmbienceVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "界面", ClampMin = "0.0", ClampMax = "2.0"))
	float UIVolume = 1.0f;

	/** PIE 运行时拖某一项的滑块，就在玩家耳边播一下这一项（方便边听边调）。 */
	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "拖滑块时自动试听（PIE 里）"))
	bool bAuditionOnEdit = true;

	/** 屏幕左上角显示每次播了哪一项（也可以用控制台 Dysis.Sfx.Debug 开关）。 */
	UPROPERTY(config, EditAnywhere, Category = "总音量", meta = (DisplayName = "屏幕上显示播了什么"))
	bool bShowDebug = false;

	// ───────── 每一项 ─────────

	/** 每一项音效。展开一行就能调音量、快慢、随机幅度、衰减距离。 */
	UPROPERTY(config, EditAnywhere, Category = "每一项", meta = (DisplayName = "音效列表", TitleProperty = "Label"))
	TArray<FDysisSfxEvent> Events;

	// ───────── 脚步 ─────────

	/** 走路时一步的距离（厘米）：WalkSpeed 450 cm/s → 每 0.5 秒一步。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "走一步的距离（厘米）", ClampMin = "50.0", UIMax = "400.0"))
	float WalkStepCm = 225.0f;

	/** 快走（Shift）时一步的距离（厘米）：RunSpeed 900 cm/s → 每 0.33 秒一步。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "快走一步的距离（厘米）", ClampMin = "50.0", UIMax = "500.0"))
	float RunStepCm = 300.0f;

	/** 水平速度超过它就算快走（厘米/秒）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "算作快走的速度（厘米/秒）", ClampMin = "0.0"))
	float RunSpeedThreshold = 650.0f;

	/** 水平速度低于它不出脚步声（厘米/秒）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "出脚步声的最低速度（厘米/秒）", ClampMin = "0.0"))
	float MinStepSpeed = 60.0f;

	/** 在空中超过这么久、下落速度超过下面的值，才播“掉落开始”（秒）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "掉落声：最短空中时间（秒）", ClampMin = "0.0"))
	float FallSoundMinAirSeconds = 0.45f;

	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "掉落声：最低下落速度（厘米/秒）", ClampMin = "0.0"))
	float FallSoundMinSpeed = 700.0f;

	/** 空中超过这么久再落地，落地声大一点。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "重落地：空中时间（秒）", ClampMin = "0.0"))
	float HeavyLandingAirSeconds = 0.6f;

	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "重落地：音量倍数", ClampMin = "1.0", ClampMax = "2.0"))
	float HeavyLandingVolume = 1.3f;

	/** 区域名 → 材质（先看这里；区域名见 HUD 调试面板 F3 / Dysis.Where）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "按区域名判定材质", TitleProperty = "ZonePrefix"))
	TArray<FDysisZoneSurfaceRule> ZoneSurfaceRules;

	/** 名字 → 材质（再看这里：地面 Actor 名字、大纲标签、Tag、网格资产名）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "按名字判定材质", TitleProperty = "NameContains"))
	TArray<FDysisSurfaceRule> NameSurfaceRules;

	/** 脚底低于这个高度（厘米）、又站在实地上，就算踩在浅水里（水庭水面是 -45）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "浅水：脚底低于（厘米）"))
	float ShallowWaterTopCm = -25.0f;

	/** 瀑布流着的时候，离瀑布多近的石面算湿石（厘米）。 */
	UPROPERTY(config, EditAnywhere, Category = "脚步", meta = (DisplayName = "湿石：离瀑布多近（厘米）", ClampMin = "0.0"))
	float WetStoneRadiusCm = 700.0f;

	// ───────── 环境 ─────────

	/** 这些区域算殿外（其余都算殿内）：海浪、海风、鸟鸣在殿外响，进殿压低、低通。 */
	UPROPERTY(config, EditAnywhere, Category = "环境", meta = (DisplayName = "殿外的区域名"))
	TArray<FString> OutdoorZones;

	/** 脚底高于它（厘米）也算殿外（屋顶）。 */
	UPROPERTY(config, EditAnywhere, Category = "环境", meta = (DisplayName = "屋顶高度（厘米）"))
	float RoofHeightCm = 2950.0f;

	/** 殿内外切换的平滑时间（秒）。 */
	UPROPERTY(config, EditAnywhere, Category = "环境", meta = (DisplayName = "进出殿的过渡（秒）", ClampMin = "0.05"))
	float IndoorBlendSeconds = 1.5f;

	/** 入夜以后鸟鸣淡出的秒数。 */
	UPROPERTY(config, EditAnywhere, Category = "环境", meta = (DisplayName = "入夜鸟鸣淡出（秒）", ClampMin = "0.1"))
	float BirdsNightFadeSeconds = 8.0f;

	/** 海风：从这个高度开始出现、到下一个高度最响（厘米）。 */
	UPROPERTY(config, EditAnywhere, Category = "环境", meta = (DisplayName = "海风：开始高度（厘米）"))
	float WindLowCm = 1500.0f;

	UPROPERTY(config, EditAnywhere, Category = "环境", meta = (DisplayName = "海风：最响高度（厘米）"))
	float WindHighCm = 3300.0f;

	// ───────── 查询 ─────────

	/** 按事件名找一项（找不到返回空）。 */
	const FDysisSfxEvent* FindEvent(FName Key) const;

	/** 这一类的总音量 × 音效总音量。 */
	float GetCategoryVolume(EDysisSfxCategory Category) const;

	/** 把出厂表里有、当前列表里没有的项补上（ini 是旧版本时）；返回补了几项。 */
	int32 EnsureDefaults();

	/** 把 ini 里的整张表恢复成出厂值（控制台 Dysis.Sfx.ResetAll）。 */
	void ResetToFactory();

	/** 存进 Config/DefaultGame.ini（控制台 Dysis.Sfx.Save；编辑器里改设置页会自动存）。 */
	bool SaveToDefaultIni();

	/** 某一项在设置页被改了（PIE 里的音效子系统用它试听、刷新正在响的循环）。 */
	static FDysisSfxEdited OnEventEdited;

	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

private:
	void RebuildIndex() const;

	mutable TMap<FName, int32> Index;
	mutable int32 IndexedNum = -1;
	bool bDefaultsEnsured = false;
};

namespace DysisSfxDefaults
{
	/** 出厂表（DysisSfxDefaults.cpp，生成的）。 */
	void Fill(TArray<FDysisSfxEvent>& Out);
}
