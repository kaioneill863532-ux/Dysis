// 日落回廊 · 可交互物接口："摸一下"（game-design 2.6 基础行为：移动、跳、观察、交互）。调研 §2.6。
// 拉杆/绞盘/转台/滑板都归约到 Interact；由 Interaction/DysisInteractComponent 的屏幕中心射线触发。
#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "UObject/Interface.h"
#include "DysisInteractable.generated.h"

UINTERFACE(MinimalAPI)
class UDysisInteractable : public UInterface
{
	GENERATED_BODY()
};

class IDysisInteractable
{
	GENERATED_BODY()

public:
	/** 玩家摸了一下。bFromFront 预留给"必须站在正面拉"的机关（v1 未用）。 */
	virtual void Interact(APawn* Player, bool bFromFront = true) = 0;

	/** 交互提示文本（空 = 不提示；UI 层 v1 未接，先给调试用）。 */
	virtual FText GetInteractPrompt() const { return FText::GetEmpty(); }
};
