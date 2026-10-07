// 狄西斯的日落回廊 · 音效设置
#include "Audio/DysisSfxSettings.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"   // FPropertyChangedChainEvent
#endif

FDysisSfxEdited UDysisSfxSettings::OnEventEdited;

UDysisSfxSettings::UDysisSfxSettings()
{
	// 出厂表（ini 里有整张表时会被 ini 覆盖；缺的项由 EnsureDefaults 补）。
	DysisSfxDefaults::Fill(Events);

	// 区域名 → 材质（区域名和灰盒一致：beam:b1、moonbr、gbridge、shadowbr……）。
	auto AddZoneRule = [this](const TCHAR* Prefix, EDysisFootSurface S)
	{
		FDysisZoneSurfaceRule R;
		R.ZonePrefix = Prefix;
		R.Surface = S;
		ZoneSurfaceRules.Add(R);
	};
	AddZoneRule(TEXT("beam:"), EDysisFootSurface::Light);       // 光柱（站上去时时间系统报 beam:xx）
	AddZoneRule(TEXT("rainbow"), EDysisFootSurface::Light);     // 虹桥
	AddZoneRule(TEXT("moonbr"), EDysisFootSurface::Moon);       // 月桥
	AddZoneRule(TEXT("shadowbr"), EDysisFootSurface::Shadow);   // 影桥
	AddZoneRule(TEXT("gbridge"), EDysisFootSurface::Shadow);    // 月光大道（DysisMoonPath 报 gbridge）

	// 名字 → 材质（地面 Actor 名字 / 大纲标签 / Tag / 网格资产名；不分大小写）。
	auto AddNameRule = [this](const TCHAR* Contains, EDysisFootSurface S)
	{
		FDysisSurfaceRule R;
		R.NameContains = Contains;
		R.Surface = S;
		NameSurfaceRules.Add(R);
	};
	AddNameRule(TEXT("MoonBridge"), EDysisFootSurface::Moon);
	AddNameRule(TEXT("MoonShrine"), EDysisFootSurface::Moon);
	AddNameRule(TEXT("MoonRelief"), EDysisFootSurface::Moon);
	AddNameRule(TEXT("Moonstone"), EDysisFootSurface::Moon);
	AddNameRule(TEXT("RainbowBridge"), EDysisFootSurface::Light);
	AddNameRule(TEXT("Bronze"), EDysisFootSurface::Bronze);
	AddNameRule(TEXT("Copper"), EDysisFootSurface::Bronze);
	AddNameRule(TEXT("Brass"), EDysisFootSurface::Bronze);
	AddNameRule(TEXT("_Curb"), EDysisFootSurface::Bronze);       // 屋顶踏步的铜沿
	AddNameRule(TEXT("TopBar"), EDysisFootSurface::Bronze);
	AddNameRule(TEXT("Armillary"), EDysisFootSurface::Bronze);   // 浑天仪
	AddNameRule(TEXT("Shallow"), EDysisFootSurface::Water);
	AddNameRule(TEXT("Tide"), EDysisFootSurface::Water);
	AddNameRule(TEXT("Wet"), EDysisFootSurface::WetStone);

	// 殿外（灰盒区域：out=岛上，beam:isle=开场光路，crown=屋顶，rbridge=屋顶细桥）。
	OutdoorZones = { TEXT("out"), TEXT("beam:isle"), TEXT("crown"), TEXT("rbridge") };
}

const UDysisSfxSettings* UDysisSfxSettings::Get()
{
	return GetMutable();
}

UDysisSfxSettings* UDysisSfxSettings::GetMutable()
{
	UDysisSfxSettings* S = GetMutableDefault<UDysisSfxSettings>();
	if (S && !S->bDefaultsEnsured)
	{
		S->bDefaultsEnsured = true;
		const int32 Added = S->EnsureDefaults();
		if (Added > 0)
		{
			UE_LOG(LogTemp, Display, TEXT("DysisSfx: ini 里缺 %d 项，已按出厂值补上（Dysis.Sfx.Save 可以存回 ini）"), Added);
		}
	}
	return S;
}

int32 UDysisSfxSettings::EnsureDefaults()
{
	TArray<FDysisSfxEvent> Factory;
	DysisSfxDefaults::Fill(Factory);

	// 去重（ini 被手改出重复时保留第一项）。
	TSet<FName> Seen;
	for (int32 i = Events.Num() - 1; i >= 0; --i)
	{
		bool bAlready = false;
		for (int32 j = 0; j < i; ++j)
		{
			if (Events[j].Key == Events[i].Key) { bAlready = true; break; }
		}
		if (bAlready) Events.RemoveAt(i);
	}
	for (const FDysisSfxEvent& E : Events) Seen.Add(E.Key);

	int32 Added = 0;
	for (const FDysisSfxEvent& F : Factory)
	{
		if (!Seen.Contains(F.Key))
		{
			Events.Add(F);
			++Added;
		}
	}
	IndexedNum = -1;
	return Added;
}

void UDysisSfxSettings::ResetToFactory()
{
	DysisSfxDefaults::Fill(Events);
	IndexedNum = -1;
}

bool UDysisSfxSettings::SaveToDefaultIni()
{
	return TryUpdateDefaultConfigFile();
}

void UDysisSfxSettings::RebuildIndex() const
{
	Index.Reset();
	for (int32 i = 0; i < Events.Num(); ++i)
	{
		if (!Index.Contains(Events[i].Key)) Index.Add(Events[i].Key, i);
	}
	IndexedNum = Events.Num();
}

const FDysisSfxEvent* UDysisSfxSettings::FindEvent(FName Key) const
{
	if (IndexedNum != Events.Num()) RebuildIndex();
	const int32* Found = Index.Find(Key);
	if (!Found || !Events.IsValidIndex(*Found) || Events[*Found].Key != Key)
	{
		// 列表被改过顺序（设置页里拖动/删除）：重建一次再找。
		RebuildIndex();
		Found = Index.Find(Key);
	}
	return (Found && Events.IsValidIndex(*Found)) ? &Events[*Found] : nullptr;
}

float UDysisSfxSettings::GetCategoryVolume(EDysisSfxCategory Category) const
{
	float V = 1.0f;
	switch (Category)
	{
	case EDysisSfxCategory::Footsteps: V = FootstepsVolume; break;
	case EDysisSfxCategory::Player:    V = PlayerVolume; break;
	case EDysisSfxCategory::Mechanism: V = MechanismVolume; break;
	case EDysisSfxCategory::Story:     V = StoryVolume; break;
	case EDysisSfxCategory::Ambience:  V = AmbienceVolume; break;
	case EDysisSfxCategory::UI:        V = UIVolume; break;
	}
	return V * MasterVolume;
}

FName UDysisSfxSettings::GetCategoryName() const
{
	return FName(TEXT("Game"));
}

#if WITH_EDITOR
FText UDysisSfxSettings::GetSectionText() const
{
	return FText::FromString(TEXT("Dysis 音效"));
}

FText UDysisSfxSettings::GetSectionDescription() const
{
	return FText::FromString(TEXT("每一项音效的音量、快慢（音高）、随机幅度、衰减距离；PIE 里拖滑块会立刻生效并试听。改动自动存进 Config/DefaultGame.ini。"));
}

void UDysisSfxSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	IndexedNum = -1;

	// 改的是“音效列表”里的某一行 → 通知 PIE（试听 + 刷新正在响的循环）。
	const int32 Row = PropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UDysisSfxSettings, Events));
	if (Events.IsValidIndex(Row))
	{
		OnEventEdited.Broadcast(Events[Row].Key);
	}
	else
	{
		OnEventEdited.Broadcast(NAME_None);   // 总音量等：只刷新循环
	}
}
#endif
