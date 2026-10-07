// 狄西斯的日落回廊 · 音效总调度（关卡里放一个，导入脚本 ue_import_sfx.py 会自动放好并绑定）
// 不改玩法代码：开局找到关卡里的光柱、拉杆、机关驱动、三相像、棱镜、时辰龛、接光、金苹果、夜光石、水面、屋顶踏步……
// 每秒 30 次看它们的公开状态（拉杆拉下了没有、机关激活了没有、光路能不能走、第几格……），
// 状态一变就在对应的位置播对应的音效；接光、拿碎片、靛色入眼、虹醒这几个直接订阅它们的事件。
// 环境声（海浪、海风、殿内底噪、鸟、水池、瀑布、水雾）按玩家在殿内外、高度、昼夜、水闸开没开混合。
// 关卡标题按灰盒的“区域 → 第几关”表，第一次走进一关时播那一关的短乐句。
// 玩家脚步由它挂到玩家身上的 UDysisFootstepComponent 负责。
//
// 绑定（Anchors / MechBindings / 屋顶踏步列表）在编辑器里按大纲名字自动填好（Details 面板“自动绑定”按钮，
// 或导入脚本），存进关卡——打包后大纲名字就没了，所以要先存引用。每一条都可以手改：换个位置、换个音、加延迟。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Save/DysisSaveGame.h"
#include "DysisSfxDirector.generated.h"

class ADysisBeamActor;
class ADysisLeverActor;
class ADysisMechDriver;
class ADysisMirrorSource;
class ADysisPrismActor;
class ADysisNicheActor;
class ADysisCatchLight;
class ADysisRainbowAlign;
class ADysisGoldenApple;
class ADysisLuminousStone;
class ADysisMoonSurface;
class UDysisSfxSubsystem;
class UDysisFootstepComponent;
class UDysisDialogueComponent;

/** 机关的哪一刻出声。 */
UENUM(BlueprintType)
enum class EDysisMechSfxWhen : uint8
{
	Activate UMETA(DisplayName = "激活 / 开始动"),
	Moving   UMETA(DisplayName = "动的时候（循环）"),
	Arrive   UMETA(DisplayName = "到位 / 停下"),
};

/** 一条机关音效绑定：哪个机关、什么时候、播哪一项、放在哪、延迟多久。 */
USTRUCT(BlueprintType)
struct DYSIS_API FDysisMechSfxBinding
{
	GENERATED_BODY()

	/** 机关（DysisMechDriver / DysisSliderActor / 任何会动的 Actor）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "机关"))
	TObjectPtr<AActor> Mech;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "什么时候"))
	EDysisMechSfxWhen When = EDysisMechSfxWhen::Activate;

	/** 播哪一项（Project Settings → Dysis 音效 里的事件名）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "事件名"))
	FName Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "延迟（秒）", ClampMin = "0.0"))
	float Delay = 0.0f;

	/** 声音放在哪（空 = 机关驱动的那块网格）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "声音放在"))
	TObjectPtr<AActor> SoundAt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "音量倍数", ClampMin = "0.0", ClampMax = "4.0"))
	float VolumeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx", meta = (DisplayName = "快慢倍数", ClampMin = "0.25", ClampMax = "4.0"))
	float PitchScale = 1.0f;
};

UCLASS(ClassGroup = (Dysis), meta = (DisplayName = "Dysis Sfx Director"))
class DYSIS_API ADysisSfxDirector : public AActor
{
	GENERATED_BODY()

public:
	ADysisSfxDirector();

	// ───── 绑定（自动绑定会填；都能手改）─────

	/** 位置锚点：Waterfall 瀑布、Pool 水池、BridgeDoor 屋顶桥门、SeleneRelief 塞勒涅浮雕、Goddess 抱月女神像、
	 *  SwanGoddess 天鹅女神像、IrisRelief 伊莉丝浮雕、RainbowBridge 虹桥、RainbowArch 虹门、Castor 卡斯托耳、Mirror 三相像…… */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|绑定", meta = (DisplayName = "位置锚点"))
	TMap<FName, TObjectPtr<AActor>> Anchors;

	/** 机关音效：哪个机关激活/动/到位时播哪一项。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|绑定", meta = (DisplayName = "机关音效", TitleProperty = "Key"))
	TArray<FDysisMechSfxBinding> MechBindings;

	/** 屋顶上行 16 级踏面（按 01–16 排好）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|绑定", meta = (DisplayName = "屋顶上行踏步"))
	TArray<TObjectPtr<AActor>> RoofUpSteps;

	/** 下行 16 级（入夜降下来的那段，按 01–16 排好）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|绑定", meta = (DisplayName = "下行踏步"))
	TArray<TObjectPtr<AActor>> RoofDnSteps;

	/** 圆眼光圈叶片（动起来时播叶片移动的循环；现在还没有代码驱动它们，接上以后自动有声）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|绑定", meta = (DisplayName = "光圈叶片"))
	TArray<TObjectPtr<AActor>> IrisBlades;

	// ───── 开关 ─────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|开关", meta = (DisplayName = "环境声"))
	bool bAmbience = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|开关", meta = (DisplayName = "机关与剧情"))
	bool bMechanisms = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|开关", meta = (DisplayName = "玩家脚步（自动挂组件）"))
	bool bFootsteps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|开关", meta = (DisplayName = "关卡标题"))
	bool bTitles = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sfx|开关", meta = (DisplayName = "界面（互动提示/提示文字/对话翻页）"))
	bool bUIHooks = true;

	// ───── 接口 ─────

	/** 按大纲名字把锚点、机关绑定、屋顶踏步、光圈叶片全部重新填一遍（Details 面板上的按钮；导入脚本也调它）。 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dysis|Sfx", meta = (DisplayName = "自动绑定"))
	void AutoBind();

	/** 绑定情况（几条、缺哪些），导入脚本打印它。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sfx")
	FString DescribeBindings() const;

	/** 关卡里的音效总调度（没有返回空）。 */
	static ADysisSfxDirector* Get(const UObject* WorldContextObject);

	bool IsWaterfallFlowing() const { return bWaterfallOn; }

	/** 灰盒的“区域 → 第几关”（0=序，1–5=日1–日5，6–10=月1–月5，-1=不算）。关卡标题用。 */
	static int32 LevelOf(const FString& Zone, bool bNight);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	// 订阅的事件（动态委托要 UFUNCTION）。
	UFUNCTION()
	void HandleLightCaught();

	UFUNCTION()
	void HandleNicheCollected(EDysisNiche Niche);

	UFUNCTION()
	void HandleIndigo();

	UFUNCTION()
	void HandleRainbowAligned();

	void HandleNotification(const FText& Text);

private:
	// ── 运行时状态 ──
	struct FMechWatch
	{
		TWeakObjectPtr<AActor> Mech;
		TWeakObjectPtr<AActor> Mesh;      // 动的是哪块网格（驱动器/石板的 TargetMesh，否则就是它自己）
		bool bWasActivated = false;
		bool bMoving = false;
		bool bEverMoved = false;
		bool bWasHidden = false;
		float StillSeconds = 0.0f;
		FTransform LastXf;
		FName LoopId;
	};
	struct FLeverWatch { TWeakObjectPtr<ADysisLeverActor> Lever; bool bPulled = false; };
	struct FBeamWatch { TWeakObjectPtr<ADysisBeamActor> Beam; bool bWalkable = false; double ChangedAt = 0.0; bool bPendingReveal = false; };
	struct FStoneWatch { TWeakObjectPtr<ADysisLuminousStone> Stone; bool bLit = false; };
	struct FStepWatch { TWeakObjectPtr<AActor> Step; float LastZ = 0.0f; bool bMoving = false; float StillSeconds = 0.0f; bool bEverMoved = false; };

	void GatherWorld();
	void SyncAll();
	void EnsureFootsteps();
	APawn* GetPlayerPawn() const;
	FVector AnchorLocation(FName Anchor, const FVector& Fallback) const;
	AActor* FindAnchor(FName Anchor) const;
	FVector PlayerFoot() const;

	void UpdateMechs(float Dt);
	void UpdateLevers();
	void UpdateBeams();
	void UpdateStatues();
	void UpdateApple();
	void UpdateStones();
	void UpdateWater();
	void UpdateRoofSteps(float Dt);
	void UpdateIris(float Dt);
	void UpdateGate();
	void UpdateAmbience(float Dt);
	void UpdateTitles();
	void UpdateUI();
	void UpdateProximity(float Dt);

	void FireBinding(const FDysisMechSfxBinding& B, const FMechWatch& W);
	void SetWaterfall(bool bOn, bool bFromSluice);
	void EnsureLoop(FName LoopId, FName Key, const FVector& Location, float Scale, float BlendSeconds = 0.5f, float FadeInSeconds = 1.0f);

	TWeakObjectPtr<UDysisSfxSubsystem> Sfx;
	TArray<FMechWatch> MechWatches;
	TArray<FLeverWatch> LeverWatches;
	TArray<FBeamWatch> BeamWatches;
	TArray<FStoneWatch> StoneWatches;
	TArray<FStepWatch> UpWatches;
	TArray<FStepWatch> DnWatches;
	TArray<FStepWatch> IrisWatches;

	TWeakObjectPtr<ADysisMirrorSource> Mirror;
	TWeakObjectPtr<ADysisPrismActor> Prism;
	TWeakObjectPtr<ADysisCatchLight> CatchLight;
	TWeakObjectPtr<ADysisRainbowAlign> RainbowAlign;
	TWeakObjectPtr<ADysisGoldenApple> Apple;
	TWeakObjectPtr<ADysisMoonSurface> Water;
	TArray<TWeakObjectPtr<ADysisNicheActor>> Niches;
	TWeakObjectPtr<UDysisFootstepComponent> Footsteps;
	TWeakObjectPtr<UDysisDialogueComponent> LastDialogue;

	double StartTime = 0.0;
	int32 MirrorSlot = -1;
	bool bMirrorLit = false;
	double MirrorDarkSince = 0.0;
	int32 PrismSlot = -1;
	uint8 AppleState = 0;
	uint8 WaterState = 0;
	bool bWaterKnown = false;
	bool bWaterfallOn = false;
	double WaterfallLoopsAt = -1.0;
	float MistAmount = 0.0f;
	bool bCaught = false;
	bool bPrologueRevealPlayed = false;
	bool bIndigoFired = false;
	bool bSeleneLocked = false;
	FRotator DoorRot = FRotator::ZeroRotator;
	bool bDoorKnown = false;

	float Indoor = 0.0f;
	float DayAmount = 1.0f;
	int32 SeenLevel = -1;
	bool bDuskTitlePlayed = false;

	bool bPromptShown = false;
	TWeakObjectPtr<AActor> PromptTarget;
	float UIPollTimer = 0.0f;
	int32 LastDialogueLine = -1;

	TArray<bool> RiseFired;
	double NextRiseAt = 0.0;
	TArray<int32> RiseQueue;
	bool bStairsLowerPlayed = false;
	float StepsLoopIdle = 0.0f;

	bool bInRainbowGate = false;
	float SleepTalkTimer = 12.0f;

	FDelegateHandle NotifyHandle;
};
