// 日落回廊 · 玩家在设置页里调的两个音量：音乐、音效（1 = 原样，0 到 1.5）。
// 音乐那一个乘在 UDysisMusicSubsystem 的音量上，音效那一个乘在 UDysisSfxSubsystem 每一项的音量上（正在响的循环也跟着变）。
// 存在玩家自己的设置文件里（GameUserSettings.ini 的 [Dysis.Audio]），下次打开还在。
#pragma once
#include "CoreMinimal.h"

namespace DysisUserVolume
{
	constexpr float Max = 1.5f;
	DYSIS_API float Music();
	DYSIS_API float Sfx();
	/** 立刻生效（不写文件）。 */
	DYSIS_API void Set(float InMusic, float InSfx);
	/** 写进设置文件（关设置页的时候叫）。 */
	DYSIS_API void Save();
}
