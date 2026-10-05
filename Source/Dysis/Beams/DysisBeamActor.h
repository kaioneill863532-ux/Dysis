// 日落回廊 · 一束可以踩的光：以窗为铰链的刚性直杆，每帧按当前 H 摆（灰盒：光路 pivot=window，站到哪里就是那一刻，无平滑）。调研 §15.1。
// 可见光与碰撞同 Transform：可见网格不加宽，碰撞盒两侧各宽 WalkExtraCm（灰盒：可踩面比可见光两侧各宽 0.3 m）。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisBeamActor.generated.h"

class UStaticMeshComponent;
class ADysisSkyActor;
class ADysisMirrorSource;

/** 人"钉"在光上的参数坐标（灰盒：站在光上时记录沿光位置，时间一变按它反算新世界位——"光带着人走"）。单位厘米。 */
USTRUCT(BlueprintType)
struct FDysisBeamParam
{
	GENERATED_BODY()

	double S = 0;   // 沿光距离（本地 X，从窗心起算）
	double E = 0;   // 侧向偏移（本地 Y，光宽方向）
	double D = 0;   // 厚度方向偏移（本地 Z）
};

UCLASS()
class DYSIS_API ADysisBeamActor : public AActor
{
	GENERATED_BODY()

public:
	ADysisBeamActor();

	// ───── 配置（关卡里每束光一个实例；窗位用 Optics::AzRyToCm 从施工图换算，勿手填错数）─────

	/** 窗心，UE 厘米。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	FVector WindowCm = FVector::ZeroVector;

	/** 区域名，如 beam:b1。BeginPlay 自动打 Tag "DysisZone=beam:b1"——时间系统踩上即认，无需 SetZoneOverride。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	FName BeamZone;

	/** 铜镜反射段：光前进方向 = 太阳直射方向关于镜面法线反射（日3 托镜石像）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	bool bFromMirror = false;

	/** 镜面法线（世界系，归一；随三相像 Yaw/Pitch 更新，由机关侧写入）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	FVector MirrorNormal = FVector(0, 0, 1);

	/** 铜镜反射源（三相像）：设了它，每帧的镜面法线从这取（覆盖上面的手填值）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	TObjectPtr<ADysisMirrorSource> MirrorSource;

	/** 光方向角速度上限（度/秒，P1-1 护栏）：只防 H 跳变把光甩飞，正常步行的太阳变化远低于它。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double MaxDirDegPerSec = 240.0;

	/** 坡度门：光与水平面夹角超过它，光不成路（灰盒 35°）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double SlopeMaxDeg = 35.0;

	/** 已站上时的放宽坡度（灰盒：站上去后 38° 都站得住）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double SlopeMaxStandingDeg = 38.0;

	/** 碰撞面比可见光两侧各宽多少（灰盒 0.3 m = 30 cm）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double WalkExtraCm = 30.0;

	/** 求交上限（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double MaxLengthCm = 20000.0;

	/** 打不到东西时落到这个高度的面（米，施工图 y）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double FallbackFloorY = 0.0;

	/** 可见光截面半尺寸（厘米）：窄缝=光索、宽窗=光廊（窗决定截面，灰盒 2.2）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	FVector BoxExtentCm = FVector(50, 120, 120);

	/** 雾浓度 0–1（水闸开着才有雾，光才显形；美术/MCP 接入后由外部写入，先默认 1 调试）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	float MistLevel = 1.0f;

	/** §4② 光路最短长度（厘米）：太短走不上去。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double MinLengthCm = 500.0;

	/** §4④ 雾覆盖门槛（光路上至少 25% 在雾里才能走；站上放宽到 15%）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double MistThreshold = 0.25;
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double MistThresholdStanding = 0.15;

	/** §4① 窗照亮比例门槛（灰盒 30%）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	double WindowIlluminationThreshold = 0.30;

	/** §4 圆眼光柱（true=穹顶圆眼落下的光，走上表面）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	bool bOculusBeam = false;

	/** §6 序·登殿开场引导光。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	bool bPrologueBeam = false;

	/** §4 镜光两端封闭（规格书原话"三相像的镜光除了两头都拦着"）：true = 镜头端和落点端各加一个封闭碰撞盒。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	bool bMirrorBeam = false;

	/** 有人离开后，光塌缩/碰撞再保留的宽限（灰盒：刚踏空 0.14 s 里还站得住）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Beam")
	float StandingGraceSeconds = 0.14f;

	// ───── 运行时 ─────

	/** 按时刻 H 摆光（方向、长度、可见性、碰撞开关）。Tick 自动调（H 取关卡里第一个 DysisSky），也可外部显式调。 */
	void UpdateFor(double H);

	/** 世界坐标 → 光上参数坐标（携带算法的一半）。 */
	FDysisBeamParam WorldToParam(const FVector& PointCm) const;

	/** 光上参数坐标 → 世界坐标（携带算法的另一半，移动组件每帧用它把人放回光上）。 */
	FVector ParamToWorld(const FDysisBeamParam& P) const;

	/** 这一帧坡度够不够成路。 */
	bool IsWalkableNow() const { return bWalkable; }

	/** 当前光前进方向（世界系；Dysis.MirrorInfo 调试用）。 */
	FVector GetTravelDir() const { return TravelDir; }

	/** 光的落点（= 窗心 + 方向 × 当前长度；镜光落点验证用，L3 东南点位对照施工图）。 */
	FVector GetTipCm() const { return WindowCm + TravelDir * float(FMath::Max(LenCm, 0.0)); }

	/** 移动组件通知"有人站着我/没人了"——站上时坡度放宽到 38°、光不塌缩。 */
	void NotifyStanding(bool bStanding);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 可见光：先用引擎方块占位，美术后换 Unlit+Additive 长条（勿开 Nanite——半透明+Nanite 会被默认材质替换，P1-10）。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Beam")
	TObjectPtr<UStaticMeshComponent> Visual;

	/** 碰撞盒：与可见光同向，截面加宽 WalkExtraCm。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Beam")
	TObjectPtr<UStaticMeshComponent> Collision;

	/** §4 镜光两端封闭碰撞盒（bMirrorBeam=true 时激活）：窗端和落点端各一个。 */
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Beam")
	TObjectPtr<UStaticMeshComponent> EndCapStart;
	UPROPERTY(VisibleAnywhere, Category = "Dysis|Beam")
	TObjectPtr<UStaticMeshComponent> EndCapEnd;

private:
	/** 沿 InDir 求光长：先 line trace（机关面/地形），打不到退到 FallbackFloorY 平面。 */
	bool SolveLength(const FVector& InDir, double& OutLenCm) const;

	/** §4① 窗口照亮比例：在窗截面（BoxExtent 的 YZ 平面）取 3×3 网格共 9 个采样点，
	 *  每个点沿 -SunDir 打 trace（朝太阳方向），打不到东西=这点被照亮。返回照亮比例 0–1。 */
	double SampleWindowIllumination(const FVector& SunDir) const;

	/** 把玩家时间组件挂成本 Actor 的 Tick 前置（先算 H 再摆光）；Pawn 晚生成时在 Tick 里重试。 */
	void TryAddTimePrerequisite();

	bool bWalkable = false;
	bool bSomeoneStanding = false;
	bool bPrereqSet = false;
	double LenCm = 0.0;
	double WindowIllumination = 1.0;   // §4① 本帧窗口照亮比例（0–1，多点采样算出）
	FVector TravelDir = FVector::ForwardVector;   // 本帧光前进方向（世界系）
	FVector PrevDir = FVector::ForwardVector;     // 上一帧方向（角速度护栏用）
	bool bDirInit = false;
	FTimerHandle GraceTimer;                      // 踏空宽限倒计时（P1-4 后半）
	TWeakObjectPtr<ADysisSkyActor> CachedSky;
};
