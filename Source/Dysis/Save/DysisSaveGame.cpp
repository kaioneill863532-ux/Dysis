#include "DysisSaveGame.h"

bool UDysisSaveGame::HasNiche(EDysisNiche Niche) const
{
	switch (Niche)
	{
	case EDysisNiche::Sun:     return bNicheSun;
	case EDysisNiche::Rainbow: return bNicheRainbow;
	case EDysisNiche::Moon:    return bNicheMoon;
	}
	return false;
}

void UDysisSaveGame::SetNiche(EDysisNiche Niche, bool bCollected)
{
	switch (Niche)
	{
	case EDysisNiche::Sun:     bNicheSun = bCollected; break;
	case EDysisNiche::Rainbow: bNicheRainbow = bCollected; break;
	case EDysisNiche::Moon:    bNicheMoon = bCollected; break;
	}
}

bool UDysisSaveGame::AllNichesCollected() const
{
	return bNicheSun && bNicheRainbow && bNicheMoon;
}

bool UDysisSaveGame::IsSluiceOpen(FName SluiceName) const
{
	return OpenSluices.Contains(SluiceName);
}
