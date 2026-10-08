// 狄西斯的日落回廊 · 音效播放子系统（每个游戏世界一个，PIE 也是）
// 所有音效都从这里播：按事件名（Foot.Stone.Walk、Mech.Lever.Pull……）找到设置里的那一项，
// 随机不重复地挑一个版本，乘上“音量 × 分类音量 × 总音量”和“快慢”，2D 或放在世界里的某个位置播。
// 循环（环境、掉落中、捧着金苹果……）用 LoopId 管：开、调音量（带平滑）、低通、移动、淡出停。
// 设置页里的滑块每帧都读，所以改了立刻听得到；正在响的循环也会跟着变。
// 殿内混响：每个声音按“这一项的混响多少 × 殿内程度（总调度报）”送一路到圆殿卷积混响（Submix_TempleReverb）。
//
// 控制台（PIE 里按 ~）：
//   Dysis.Sfx.Play Mech.Lever.Pull [第几个]   在耳边试听一项
//   Dysis.Sfx.List [过滤]                     列出所有项和当前音量/快慢
//   Dysis.Sfx.Volume <事件名|Master|Footsteps|Player|Mechanism|Story|Ambience|UI> 0.8
//   Dysis.Sfx.Pitch <事件名> 1.1             改快慢
//   Dysis.Sfx.Enable <事件名> 0|1
//   Dysis.Sfx.Reverb [事件名] [0.8]          殿内混响：不带数 = 看状态；一个数 = 总量；事件名+数 = 单项
//   Dysis.Sfx.Debug [0|1]                     屏幕上显示每次播了哪一项
//   Dysis.Sfx.Check                           检查每个声音文件都导入了
//   Dysis.Sfx.Save                            把当前数值存进 Config/DefaultGame.ini
//   Dysis.Sfx.ResetAll                        恢复出厂值（要存的话再 Save）
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/StreamableManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Audio/DysisSfxSettings.h"
#include "DysisSfxSubsystem.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USceneComponent;
class USoundSubmixBase;

UCLASS()
class DYSIS_API UDysisSfxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 找到当前世界的音效子系统（编辑器非 PIE 世界里返回空）。 */
	static UDysisSfxSubsystem* Get(const UObject* WorldContextObject);

	// ───── 一次性 ─────

	/** 在世界位置播一项（2D 的项忽略位置）。Variant >= 0 = 指定第几个版本（第几级/第几格），-1 = 随机。 */
	UAudioComponent* Play(FName Key, const FVector& Location, int32 Variant = -1, float VolumeScale = 1.0f, float PitchScale = 1.0f);

	/** 跟着一个组件走（例如被推的雕像）。 */
	UAudioComponent* PlayAttached(FName Key, USceneComponent* AttachTo, int32 Variant = -1, float VolumeScale = 1.0f, float PitchScale = 1.0f);

	/** 过几秒再播（演出里的先后：先亮起、再伸出）。 */
	void PlayDelayed(float DelaySeconds, FName Key, const FVector& Location, int32 Variant = -1, float VolumeScale = 1.0f, float PitchScale = 1.0f);

	/** 在玩家耳边（2D）试听一项，不管冷却和启用开关。 */
	UAudioComponent* PlayAudition(FName Key, int32 Variant = -1);

	// ───── 循环 ─────

	/** 开一条循环（LoopId 自己起名；已经在响就只更新目标音量）。返回是否在响。 */
	bool StartLoop(FName LoopId, FName Key, const FVector& Location, float FadeInSeconds = 1.0f, float VolumeScale = 1.0f);

	/** 调循环的音量倍数（0–1+，在 BlendSeconds 秒内平滑过去）。 */
	void SetLoopScale(FName LoopId, float VolumeScale, float BlendSeconds = 0.5f);

	/** 调循环的快慢倍数（乘在设置里的快慢上）。 */
	void SetLoopPitchScale(FName LoopId, float PitchScale);

	/** 循环低通（Hz；>= 20000 = 关）。 */
	void SetLoopLowPass(FName LoopId, float CutoffHz);

	/** 移动 3D 循环的位置。 */
	void SetLoopLocation(FName LoopId, const FVector& Location);

	/** 淡出并停掉。 */
	void StopLoop(FName LoopId, float FadeOutSeconds = 1.0f);

	bool IsLoopActive(FName LoopId) const;

	// ───── 混响 ─────

	/** 殿内程度（0 = 殿外，1 = 殿内；总调度每帧报，进出殿时平滑变）。正在响的循环会跟着变。 */
	void SetReverbEnvironment(float Indoor01) { ReverbIndoor = FMath::Clamp(Indoor01, 0.0f, 1.0f); }
	float GetReverbEnvironment() const { return ReverbIndoor; }

	/** 殿内混响的 Submix（第一次用时加载并接进音频设备；没生成时返回空，声音照常播、只是不带混响）。 */
	USoundSubmixBase* GetReverbSubmix();

	/** 这一项现在该送多少混响（已乘殿内程度和远近）。Location 为空 = 不算远近。 */
	float ReverbSendFor(const FDysisSfxEvent& Event, const FVector* Location, float Indoor01) const;

	// ───── 调试 ─────

	void SetDebug(bool bOn) { bDebug = bOn; }
	bool IsDebug() const;

	/** 检查每一项的每个文件都能加载；返回缺的个数（细节写日志）。 */
	int32 CheckAllAssets(TArray<FString>* OutMissing = nullptr);

	// ───── UTickableWorldSubsystem ─────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FLoop
	{
		TWeakObjectPtr<UAudioComponent> Comp;
		FName Key;
		float Scale = 0.0f;        // 当前倍数（淡入淡出时在变）
		float Target = 1.0f;       // 目标倍数
		float Rate = 1.0f;         // 每秒变化量
		float PitchScale = 1.0f;
		bool bStopping = false;
		float LastSend = -1.0f;    // 上次送到混响的量（变了才重发）
	};

	struct FPending
	{
		double At = 0.0;
		FName Key;
		FVector Location = FVector::ZeroVector;
		int32 Variant = -1;
		float VolumeScale = 1.0f;
		float PitchScale = 1.0f;
	};

	struct FKeyState
	{
		int32 LastIndex = -1;
		double LastPlayTime = -1.0e9;
		TArray<TWeakObjectPtr<UAudioComponent>> Active;
		bool bWarnedMissing = false;
	};

	UAudioComponent* PlayInternal(FName Key, const FVector* Location, USceneComponent* AttachTo, int32 Variant,
		float VolumeScale, float PitchScale, bool bAudition);
	USoundBase* ResolveSound(const FDysisSfxEvent& Event, int32 Variant, FKeyState& State);
	USoundAttenuation* GetAttenuation(const FDysisSfxEvent& Event);
	float BaseVolume(const FDysisSfxEvent& Event) const;
	void OnSettingsEdited(FName Key);
	void DebugPrint(const FString& Line) const;
	double Now() const;
	void StartPreload();
	void ApplyReverb(UAudioComponent* Comp, const FDysisSfxEvent& Event, const FVector* Location, float Indoor01);

	/** 同步加载过的声音（防 GC）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> Loaded;

	/** 正在响的循环（强引用，防止 GC 掉）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> LoopRefs;

	/** 每一项的 3D 衰减（运行时生成，不用建资产）。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundAttenuation>> Attenuations;

	/** 殿内混响 Submix（GetReverbSubmix 第一次成功时填上）。 */
	UPROPERTY(Transient)
	TObjectPtr<USoundSubmixBase> ReverbSubmix;

	TMap<FName, FVector2f> AttenuationParams;
	TMap<FName, FKeyState> KeyStates;
	TMap<FName, FLoop> Loops;
	TArray<FPending> Pending;

	FStreamableManager Streamable;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	TSharedPtr<FStreamableHandle> ReverbLoadHandle;
	FDelegateHandle EditedHandle;
	bool bDebug = false;
	float ReverbIndoor = 1.0f;     // 没有总调度报的时候（别的测试关卡）按殿内算
	bool bReverbTried = false;
};

/** 界面音效（给 UMG 按钮、碎片栏用）。 */
UENUM(BlueprintType)
enum class EDysisUISound : uint8
{
	Hover            UMETA(DisplayName = "悬停"),
	Confirm          UMETA(DisplayName = "确认"),
	Back             UMETA(DisplayName = "返回"),
	StartGame        UMETA(DisplayName = "开始游戏"),
	ShardHoverSun    UMETA(DisplayName = "碰到日之碎片"),
	ShardHoverMoon   UMETA(DisplayName = "碰到月之碎片"),
	ShardHoverRainbow UMETA(DisplayName = "碰到虹之碎片"),
};

/** 蓝图/UMG 用：一行播音效。 */
UCLASS()
class DYSIS_API UDysisSfxLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 在某个位置播一项（2D 的项忽略位置）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sfx", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "Variant,VolumeScale,PitchScale"))
	static void PlayDysisSfx(const UObject* WorldContextObject, FName Key, FVector Location, int32 Variant = -1, float VolumeScale = 1.0f, float PitchScale = 1.0f);

	/** 界面音效（UMG 按钮的 OnHovered / OnClicked 里调）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sfx", meta = (WorldContext = "WorldContextObject"))
	static void PlayDysisUISound(const UObject* WorldContextObject, EDysisUISound Sound);

	/** 开一条循环（LoopId 自己起名）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sfx", meta = (WorldContext = "WorldContextObject"))
	static void StartDysisSfxLoop(const UObject* WorldContextObject, FName LoopId, FName Key, FVector Location, float FadeInSeconds = 1.0f);

	/** 停一条循环。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sfx", meta = (WorldContext = "WorldContextObject"))
	static void StopDysisSfxLoop(const UObject* WorldContextObject, FName LoopId, float FadeOutSeconds = 1.0f);

	/**
	 * 编辑器用（ue_import_sfx.py 会调）：读圆殿 IR 的 wav，生成 /Game/Dysis/Audio/Reverb 下的
	 * 冲激响应、卷积混响效果、Submix 三个资产（已有就更新）。音量按预览版对齐：干声 1 + 湿声 0.42。
	 * 需要打开 Synthesis 插件。返回“成功：……”或“失败：……”；资产生成后要保存（脚本会存）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sfx|Editor")
	static FString BuildTempleReverb(const FString& IrWavFile);
};
