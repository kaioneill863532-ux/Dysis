// 日落回廊 · 全殿共用的状态（灰盒里的 MIST、FLOW、ISLE_GROW 这几个全局量）：
//   水闸开没开、中庭的雾升到哪了、开场那束光伸到哪了。光路能不能踩要问这里“这一点有没有雾”。
// 一个世界一份（世界子系统），每帧自己更新。数值照灰盒 v0.12（updateNymph、updateIsleGrow、mistAt、shadowY）。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DysisWorldState.generated.h"

/** 三相像手里那面铜镜现在的样子（机关总管每帧写，镜光和月光反射读）。厘米、单位向量。 */
struct FDysisMirrorState
{
	bool bValid = false;
	FVector Center = FVector::ZeroVector;      // 镜面中心
	FVector U = FVector::RightVector;          // 镜面的横向
	FVector V = FVector::UpVector;             // 镜面的竖向（朝上的那一边）
	FVector N = FVector::ForwardVector;        // 镜面的法线
	float WidthCm = 220.0f, HeightCm = 140.0f;
	int32 Form = 0;                            // 0 暗相（垂镜）、1 日相（举镜）、2 月相（递镜）
	float W3[3] = { 1.0f, 0.0f, 0.0f };        // 三相各占多少（暗、日、月），慢慢过渡
	bool bHasLitBox = false;                   // 镜面上被照亮的那一块（0–1）：U0、U1、V0、V1
	float LitBox[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	int32 LitBoxForm = 0;                      // 这一块是哪一相的光照出来的（1 日、2 月）
	FVector ReflectDir = FVector::ForwardVector;   // 反射出去的方向（有光时）
	TArray<TWeakObjectPtr<AActor>> Self;       // 雕像、镜子自己：算它被没被照到时不算挡光
};

UCLASS()
class DYSIS_API UDysisWorldState : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDysisWorldState* Get(const UObject* WorldContext);

	// ───── 水闸、雾 ─────

	/** 水闸开着：瀑布流、雾往上升。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	bool bSluiceOpen = false;

	/** 雾的浓度 0–1（开闸后每秒 +0.1，关闸后每秒 −0.2）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	float MistAmt = 0.0f;

	/** 雾升到的高度（厘米；开闸后从水面起每秒升 3 m）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	float MistFrontCm = -100.0f;

	/** 瀑布的水量 0–1。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	float FlowK = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Dysis|World")
	void SetSluiceOpen(bool bOpen);

	/** 这一点的雾有多浓（≥ 0.85 算“在雾里”）：中庭里的雾，加上海峡上一直有的水沫。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|World")
	float MistAt(FVector PointCm) const;

	// ───── 开场的光 ─────

	/** 人在岛上往神殿迈出一步（0.8 m）以后，光开始从殿里伸出来。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	bool bIsleGrowStarted = false;

	/** 伸出来的进度 0–1（3 秒伸到岛上，到 1 才能踩）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	float IsleGrowK = 0.0f;

	// ───── 屋顶的光圈 ─────

	/** 光圈现在张到多大（半径，厘米；合拢 40，全开 1100）。机关总管每帧写，圆眼光柱读。 */
	UPROPERTY(BlueprintReadOnly, Category = "Dysis|World")
	float IrisACm = 40.0f;

	/** 测试用：把光圈直接摆成这么大（之后机关总管不再改它，直到 DebugRelease）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|World")
	void DebugSetIris(float ACm);
	bool IsIrisDebug() const { return bDebugIris; }

	// ───── 三相像的镜子 ─────
	FDysisMirrorState Mirror;

	// ───── 别的 ─────

	/** 挡光的东西挪了（推拉石板、雕像……）就加一：各束光看见它变了就重算。 */
	int32 LightSerial = 0;
	void BumpLight() { ++LightSerial; }

	/** 入夜了没有（接住最后一缕光以后）。 */
	bool IsNight() const;

	/** 远处岩岛的影子爬到多高（厘米）：太阳沉到它的山脊后面时，这个高度以下照不到太阳。 */
	static double ShadowZ(const FVector& SunDir);

	/** 这一点朝太阳看过去，是不是被远处岩岛挡着（灰盒 placeOccluder 的那块挡板）。
	 *  岛影的边界不是水平的：ShadowZ 是它在殿中心的高度，往太阳那一侧每走 1 m 高出 tan(太阳高度) m。
	 *  只在日落前后有区别（太阳高的时候影子边界远在地面以下）。 */
	static bool InIsleShadow(const FVector& PointCm, const FVector& SunDir);

	// ───── 测试用：把状态直接摆成某个样子（之后不再自动更新，直到 DebugRelease） ─────
	UFUNCTION(BlueprintCallable, Category = "Dysis|World")
	void DebugSetMist(float Amt, float FrontCm);

	UFUNCTION(BlueprintCallable, Category = "Dysis|World")
	void DebugSetIsleGrow(bool bStarted, float K);

	UFUNCTION(BlueprintCallable, Category = "Dysis|World")
	void DebugRelease();

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool bDebugMist = false;
	bool bDebugGrow = false;
	bool bDebugIris = false;
};
