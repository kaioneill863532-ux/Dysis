// 日落回廊 · 天上的日月：给一个时刻 H，把太阳光、月光朝向日月，按灰盒 setTime 的规则切换主光、调亮度、摆月亮圆盘。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisSkyActor.generated.h"

class UDirectionalLightComponent;
class UStaticMeshComponent;
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

	/** 月光：Atmosphere Sun Light，Index 1。 */
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

	/** 每一盏的照度（勒克斯）。0.59 = 朝上的面得到的和灰盒一样多（灰盒 0.45 × 换算 3.0）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float NightFillLux = 0.59f;

	/** 这四盏斜着的角度（离地平线多少度）：越低墙面得到的越多、地面越少。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	float NightFillElevationDeg = 35.0f;

	/** 灰盒 nightAmb 天上那一半的颜色（#3a4c70）。 */
	UPROPERTY(EditAnywhere, Category = "Dysis|Sky")
	FLinearColor NightFillColor = FLinearColor(0.0423f, 0.0723f, 0.1620f);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	void ApplyMoonDisc();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoonDiscMID;

	float CurrentH = 0.f;
	FVector SunDir = FVector::UpVector;
	FVector MoonDir = -FVector::UpVector;
	float SunAlt = 0.f;
	float MoonAlt = 0.f;
	bool bSunMain = true;
	float GreyboxIntensity = 0.f;
	float MoonDiscOpacity = 0.f;
};