// 日落回廊 · 天上的日月：给一个时刻 H，把太阳光、月光朝向日月，按灰盒 setTime 的规则切换主光、调亮度、摆月亮圆盘。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisSkyActor.generated.h"

class UDirectionalLightComponent;
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS(Blueprintable)
class DYSIS_API ADysisSkyActor : public AActor
{
	GENERATED_BODY()

public:
	ADysisSkyActor();

	/** 把天摆到时刻 H（时角，度；钟点 = 12:00 + H/15）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sky")
	void SetTime(float H);

	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	float GetTime() const { return CurrentH; }

	/** 当前主光是不是太阳（太阳高度 > −0.8° 时是太阳，否则是月亮）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	bool IsSunMain() const { return bSunMain; }

	/** 当前主光的朝向（DysisLightRotation(主光天体方向)）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	FRotator GetMainLightRotation() const;

	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	FVector GetSunDir() const { return SunDir; }

	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	FVector GetMoonDir() const { return MoonDir; }

	/** 太阳 / 月亮高度角（度）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	void GetAltitudes(float& SunAltDeg, float& MoonAltDeg) const { SunAltDeg = SunAlt; MoonAltDeg = MoonAlt; }

	/** 灰盒里的主光亮度（three.js 单位，没乘换算系数），给调试和组员参考。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	float GetGreyboxIntensity() const { return GreyboxIntensity; }

	/** 月亮圆盘的不透明度（0–1）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	float GetMoonDiscOpacity() const { return MoonDiscOpacity; }

	/** 太阳光：照物体的那一盏（方向、谁是主光、亮度都按灰盒）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TObjectPtr<UDirectionalLightComponent> SunLight;

	/** 太阳照亮天空的那一盏（Atmosphere Sun Light，Index 0）：不照任何物体、不投影，只给大气用。
	 *  白天和太阳光一样亮；太阳落到地平线下以后它还慢慢暗下去一阵（暮光），天不会一下子全黑——
	 *  日落后、月亮升高之前这一小段，场景靠天光还看得见（灰盒里这一段是用环境光顶着的）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TObjectPtr<UDirectionalLightComponent> SunSkyGlow;

	/** 太阳落到地平线下多少度时暮光完全消失。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float TwilightEndAltDeg = -14.0f;

	/** 月光：只照物体，不照亮大气（照亮大气的话整片夜空是亮蓝色的，比灰盒亮得多）；夜空的颜色由下面的 NightDome 画。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	/** 月亮圆盘：放在月亮方向很远的地方的发光球（从哪边看都是圆的）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TObjectPtr<UStaticMeshComponent> MoonDisc;

	/** 月亮圆盘的材质，要有标量参数 Opacity（设置脚本会建 /Game/Dysis/Sky/M_DysisMoonDisc）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	TObjectPtr<UMaterialInterface> MoonDiscMaterial;

	/** 编辑器里和开始时摆的时刻（默认 H_I，岛上 13:29）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float PreviewH;

	/** 灰盒亮度 → UE 勒克斯的换算（颜色、强度可以按 UE 曝光重调；切换时机和方向不能变）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float SunLuxPerGreyboxUnit = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float MoonLuxPerGreyboxUnit = 3.0f;

	/** 月亮圆盘离天空中心多远（厘米）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float MoonDiscDistanceCm = 400000.f;

	/** 月亮圆盘的角直径（度）。灰盒是 8000 远处半径 95 的圆，约 1.36°。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float MoonDiscAngularDiameterDeg = 1.36f;

	/** 灰盒的颜色：SUN_LO → SUN_HI 按 smoothstep(2, 25, 太阳高度) 过渡；月光 MOON_COL。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	FLinearColor SunColorLow = FLinearColor(1.0f, 0.52f, 0.26f);

	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	FLinearColor SunColorHigh = FLinearColor(1.0f, 0.94f, 0.84f);

	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	FLinearColor MoonColor = FLinearColor(0.62f, 0.72f, 0.95f);

	/** 夜里的环境光（灰盒 nightAmb：一盏不被遮挡的半球光，天上来的是深蓝）。UE 里用四盏不投影子的平行光从四面斜上方照下来顶着：
	 *  月光照不到的回廊深处也看得清。强度跟着“入夜多少”走（太阳在地平线上 1.5° 到地平线下 9° 之间慢慢亮起来）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TArray<TObjectPtr<UDirectionalLightComponent>> NightFill;

	/** 每一盏的照度（勒克斯）。按灰盒的数算是 0.59（朝上的面得到 0.45 × 换算 3.0）；这里的引擎自己还会算一点反光，
	 *  拿画面对下来 0.45 时月光照不到的墙和灰盒一样暗。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky")
	float NightFillLux = 0.45f;

	/** 这四盏斜着的角度（离地平线多少度）：越低墙面得到的越多、地面越少。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float NightFillElevationDeg = 35.0f;

	/** 落日以后、天还没黑透的那一阵（灰盒 dayAmb × (1 − dusk)）：白天这一份环境光是引擎的天光出的，可太阳一落到地平线下
	 *  引擎的天光很快就全黑了，灰盒里却是跟着 dusk 慢慢暗下去的——所以落日以后的这一份也让上面那四盏灯顶着。
	 *  每一盏的照度（勒克斯；0.33 是拿灰盒同样时刻的画面对出来的）和颜色（灰盒 dayAmb 天上那一半 #b8c4d0）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky")
	float DayFillLux = 0.33f;

	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	FLinearColor DayFillColor = FLinearColor(0.479f, 0.552f, 0.631f);

	/** 落日以后那一阵天的颜色（灰蒙蒙的暮色；灰盒里是它的天空着色器在太阳落到地平线下时画出来的样子），随着天黑慢慢换成夜空。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky")
	FLinearColor TwilightSkyColor = FLinearColor(0.055f, 0.049f, 0.043f);

	// ───── 黄昏的调子：太阳快落山时画面压暗、偏橙红，天边的红更浓（2026-10-08 用户：走到殿顶时不像黄昏、太亮、没有橙红色的感觉） ─────

	/** 太阳落到这个高度（度）开始有黄昏的调子，落到 DuskFullAltDeg 时最浓；落日以后随着天黑慢慢退掉。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	float DuskStartAltDeg = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	float DuskFullAltDeg = 3.0f;

	/** 最浓时曝光加多少（负数 = 压暗）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	float DuskExposureBias = -1.2f;

	/** 最浓时整个画面乘上的颜色（偏橙红）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	FLinearColor DuskTint = FLinearColor(1.06f, 0.87f, 0.72f);

	/** 最浓时颜色的浓度（1 = 不变）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	float DuskSaturation = 1.08f;

	/** 最浓时大气把蓝光散掉多少倍（越大落日越红、天边的橙红越浓）、空气里的尘雾多几倍（越大太阳周围那一片亮晕越大）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	float DuskRayleighScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky|黄昏")
	float DuskMieScale = 3.0f;

	/** 现在黄昏的调子有多浓（0–1）。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky|黄昏")
	float GetDuskLook() const;

	/** 把黄昏的颜色写进一台镜头的后期设置；返回曝光该加多少（由镜头自己加到它的曝光上）。 */
	float ApplyDuskLook(struct FPostProcessSettings& PP) const;

	/** 游戏里接住最后一缕光的那一刻叫一下（时间组件会叫）：从这一刻起天由夜空的球来画、暮色的环境光亮起来。
	 *  没叫过的时候（比如主界面的延时摄影）按太阳的高度自己判断：落到地平线下 1.2°–2° 之间换过去。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sky")
	void SetAfterSunset(bool bAfter);

	/** 灰盒 nightAmb 天上那一半的颜色（#3a4c70）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	FLinearColor NightFillColor = FLinearColor(0.0423f, 0.0723f, 0.1620f);

	/** 夜空（灰盒 nightDome）：罩在外面的一个大球，朝里的一面画夜空——地平线附近深蓝、头顶近乎黑，月亮周围一圈淡淡的光。
	 *  太阳落到地平线下 2° 开始显出来，到 10° 完全盖住白天的天。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TObjectPtr<UStaticMeshComponent> NightDome;

	/** 夜空的材质，参数 Opacity、Glow、Halo、MoonDir（设置脚本会建 /Game/Dysis/Sky/M_DysisNightDome）。 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> NightDomeMaterial;

	/** 夜空的球有多大（厘米；要比月亮圆盘远）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float NightDomeRadiusCm = 460000.f;

	/** 夜空亮度的换算（灰盒的颜色 × 这个数）。3.6 是拿灰盒同样位置的画面对出来的。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky")
	float NightSkyGain = 3.6f;

	/** 星星（灰盒 stars）：1800 颗，位置、亮度、颜色和灰盒同一套随机数；绕着北天极跟时间一起转。
	 *  太阳落到地平线下 4° 开始出现，到 13° 全亮。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis|Sky")
	TObjectPtr<UInstancedStaticMeshComponent> Stars;

	/** 星星的材质，参数 Opacity、Glow（设置脚本会建 /Game/Dysis/Sky/M_DysisStar）。 */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> StarMaterial;

	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	int32 StarCount = 1800;

	/** 星星离天空中心多远（厘米；在月亮圆盘和夜空的球之间）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float StarDistanceCm = 430000.f;

	/** 一颗星看上去多大（度）。灰盒是 1.6 个像素的点；这里是一个中间亮、边上淡的小圆片，稍大一点免得闪。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float StarAngularSizeDeg = 0.16f;

	/** 星星亮度的换算（灰盒的亮度 × 这个数）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis|Sky")
	float StarGain = 6.0f;

	/** 星星现在显出来多少（0–1），给结局的星座用。 */
	UFUNCTION(BlueprintPure, Category = "Dysis|Sky")
	float GetStarOpacity() const { return StarOpacity; }

	/** 把夜里的灯和夜空按现在的参数重摆一遍（调过上面的数以后叫一下）。 */
	UFUNCTION(BlueprintCallable, Category = "Dysis|Sky")
	void RefreshSky();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ApplyMoonDisc();
	void BuildStars();
	void ApplyNightSky();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonDiscMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NightDomeMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StarMID;

	int32 StarsBuilt = 0;
	float StarOpacity = 0.f;
	bool bTimeSet = false;
	bool bAfterSunset = false;
	float AfterSunsetK() const;
	void ApplyDuskAtmosphere();
	TWeakObjectPtr<class USkyAtmosphereComponent> Atmosphere;
	float BaseRayleigh = -1.f, BaseMie = -1.f, AppliedDusk = -1.f;

	float CurrentH = 0.f;
	FVector SunDir = FVector::UpVector;
	FVector MoonDir = -FVector::UpVector;
	float SunAlt = 0.f;
	float MoonAlt = 0.f;
	bool bSunMain = true;
	float GreyboxIntensity = 0.f;
	float MoonDiscOpacity = 0.f;
};