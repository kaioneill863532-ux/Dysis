#include "Audio/DysisUserVolume.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* const UserVolumeSection = TEXT("Dysis.Audio");
	float GUserMusic = 1.0f, GUserSfx = 1.0f;
	bool bUserVolumeLoaded = false;

	void LoadUserVolume()
	{
		if (bUserVolumeLoaded) return;
		bUserVolumeLoaded = true;
		if (!GConfig) return;
		GConfig->GetFloat(UserVolumeSection, TEXT("MusicVolume"), GUserMusic, GGameUserSettingsIni);
		GConfig->GetFloat(UserVolumeSection, TEXT("SfxVolume"), GUserSfx, GGameUserSettingsIni);
		GUserMusic = FMath::Clamp(GUserMusic, 0.0f, DysisUserVolume::Max);
		GUserSfx = FMath::Clamp(GUserSfx, 0.0f, DysisUserVolume::Max);
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
	LoadUserVolume();
	if (!GConfig) return;
	GConfig->SetFloat(UserVolumeSection, TEXT("MusicVolume"), GUserMusic, GGameUserSettingsIni);
	GConfig->SetFloat(UserVolumeSection, TEXT("SfxVolume"), GUserSfx, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
