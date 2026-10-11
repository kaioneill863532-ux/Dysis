#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DysisGameMode.generated.h"

/** 日落回廊的 GameMode：玩家用 ADysisCharacter、界面用 ADysisHUD；开局把关卡里没摆的窗光补齐。 */
UCLASS()
class DYSIS_API ADysisGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADysisGameMode();

	/** 测试用：开局落脚的那一块放到海面上（true，游戏里的样子）/ 放回灰盒里小岛的位置（false；和灰盒逐点比开场那束光的时候用）。 */
	void DebugSetStartOnSea(bool bSea);

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;

private:
	/** 开局站的那座小岛不要了，人出生在海面上（见 DysisGB::ISLAND_TOP_Z）：岛（不画，只留挡人、接光的那一层）、岛边的护栏、出生点
	 *  顺着开场那束光挪到海面那么高。关卡里摆的还是灰盒的位置，这里在人生成以前挪。 */
	void MoveStartOntoSea();
	bool bStartOnSea = false;
	TArray<TWeakObjectPtr<AActor>> StartPadActors;      // 岛、岛边的护栏

	/** 灰盒有 8 束窗光（isle、b1、b2、iris、c、h1、h2、h3）；关卡里缺哪束就生成哪束。 */
	void EnsureWindowBeams();
};
