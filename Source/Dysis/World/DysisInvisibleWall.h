// 日落回廊 · 看不见的墙：只挡人，不挡光、不挡镜头，游戏里看不见（编辑器里是一个线框盒子）。
// 灰盒里有很多这样的墙没有模型：每层走到瀑布边就到头的挡墙、栏杆上方的高护栏（跳不过去）、台地和小岛边缘的护栏、
// 窗洞里的挡板……关卡里的这一批由 Art/Models/temple-v0.12/ue_place_invisible_walls.py 按灰盒导出的数据摆出来，
// 放在大纲的 Dysis_InvisibleWalls 文件夹里。要调某一堵，改它的 ExtentCm 或直接挪。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DysisInvisibleWall.generated.h"

class UBoxComponent;

UCLASS()
class DYSIS_API ADysisInvisibleWall : public AActor
{
	GENERATED_BODY()

public:
	ADysisInvisibleWall();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dysis")
	TObjectPtr<UBoxComponent> Box;

	/** 盒子的半尺寸（厘米）：X 沿墙的厚度方向或长度方向都行，跟着 Actor 的朝向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis")
	FVector ExtentCm = FVector(50.0, 50.0, 100.0);

	/** 这堵墙是灰盒里的哪一类（SeamEnd、ParapetGuard、TerraceEdge……），只是给人看的。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dysis")
	FName Group;

	virtual void OnConstruction(const FTransform& Transform) override;
};
