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

protected:
	virtual void BeginPlay() override;

private:
	/** 灰盒有 8 束窗光（isle、b1、b2、iris、c、h1、h2、h3）；关卡里缺哪束就生成哪束。 */
	void EnsureWindowBeams();
};
