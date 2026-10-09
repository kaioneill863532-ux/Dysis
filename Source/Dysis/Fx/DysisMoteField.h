// 日落回廊 · 光粒：一组小片，充满“挂着的那个东西”的盒子（引擎方块 ±50 的范围，跟着它缩放），
// 每一粒的位置、飘动、明灭全在材质里算（/Game/Dysis/Fx/M_DysisMote，见 Art/Fx/ue_make_fx_materials.py），不用粒子系统。
// 光柱里的浮尘、桥上的光点、瀑布脚下的水沫都是它。
#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DysisMoteField.generated.h"

class UMaterialInstanceDynamic;

UCLASS()
class DYSIS_API UDysisMoteField : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDysisMoteField();

	/** 撒 Count 粒（重复叫就重撒）。Seed 不同，撒的位置就不同。 */
	void Build(int32 Count, int32 Seed = 1);

	/** 颜色、每一粒多大（厘米）、往哪飘多快（每秒飘过盒子的几分之几，三个方向）、晃多少、多亮。 */
	void SetLook(const FLinearColor& Color, float SizeCm, const FVector& Flow, float Wobble, float Glow);

	/** 整体显出来多少（0–1）；0 时整个不画。 */
	void SetOpacity(float Opacity);

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MoteMID;

	UMaterialInstanceDynamic* Mid();
};
