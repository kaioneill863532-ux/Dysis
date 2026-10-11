#include "Audio/DysisUserVolume.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* const UserVolumeSection = TEXT("Dysis.Audio");
	// 2026-10-11 用户用滑条试出来的：音乐 70、音效 40 最合适——定成这两个数，设置页里的两条滑条拿掉了。
	// 以前存在设置文件里的值不再读（不然试的时候存下的数会一直盖着这两个）、也不再写。
	float GUserMusic = 0.70f, GUserSfx = 0.40f;
	bool bUserVolumeLoaded = false;

	void LoadUserVolume()
	{
		bUserVolumeLoaded = true;
	}
}

float DysisUserVolume::Music() { LoadUserVolume(); return GUserMusic; }
float DysisUserVolume::Sfx() { LoadUserVolume(); return GUserSfx; }

void DysisUserVolume::Set(float InMusic, float InSfx)
{
	LoadUserVolume();
	GUserMusic = FMath::Clamp(InMusic, 0.0f, Max);
	GUserSfx = FMath::Clamp(InSfx, 0.0f, Max);
}

void DysisUserVolume::Save()
{
	// 音量定死了，不存（见上）
}
