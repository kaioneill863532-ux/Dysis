// 日落回廊 · 虚拟地面接口：水面/夜光石这类"亮了才踩得住"的面（调研 §15.2/§16.2）。
// 这些面物理上 NoCollision，改由自定义移动组件在 Tick 末尾来问一句 IsWalkableAt——不亮就没有地。
// 第二批扩展（M5）：回写路径——移动组件站稳时 NoteWalkableAt（踏片记"脚下那片保留到离开"，灰盒 2.3）、
// 离开时 NoteDeparted；踩在虚拟面上时可用 GetZoneName 让时间系统改判区域（月光大道走 gbridge/moonbr）。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DysisVirtualSurface.generated.h"

UINTERFACE(MinimalAPI)
class UDysisVirtualSurface : public UInterface
{
	GENERATED_BODY()
};

class IDysisVirtualSurface
{
	GENERATED_BODY()

public:
	/** 脚下这一点（UE 厘米）现在踩不踩得住。水面=亮格判定，夜光石=磷光未衰减完。 */
	virtual bool IsWalkableAt(const FVector& FootCm) const = 0;

	/** 移动组件站稳在本面上时每帧回写脚位（踏片记 KeepTile；灰盒：脚下那片一直保留到离开）。 */
	virtual void NoteWalkableAt(const FVector& FootCm) {}

	/** 移动组件离开本面（换面/起跳/落水）时通知——清 KeepTile 等过期状态。 */
	virtual void NoteDeparted() {}

	/** 踩在本面上时时间系统应改判的区域名；返回 false = 不改判（用脚下的物理 Tag 判）。 */
	virtual bool GetZoneName(FName& OutZone) const { return false; }
};
