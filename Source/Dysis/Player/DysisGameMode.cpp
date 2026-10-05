#include "DysisGameMode.h"
#include "DysisCharacter.h"
#include "UI/DysisHUD.h"

ADysisGameMode::ADysisGameMode()
{
	DefaultPawnClass = ADysisCharacter::StaticClass();
	HUDClass = ADysisHUD::StaticClass();   // 零资产交互提示 + 调试面板（PIE 里改 bShowDebug）
}