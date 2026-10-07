// 狄西斯的日落回廊 · 音效自动化测试（Session Frontend → Automation → Dysis.Sfx，或命令行
//   UnrealEditor-Cmd Dysis.uproject -ExecCmds="Automation RunTests Dysis.Sfx; Quit" -unattended -nullrhi）
//   Table  ：事件表本身（不需要资产）——名字不重复、每项都有文件、代码里用到的名字都在表里。
//   Assets ：每个声音文件都导入了，循环项的 SoundWave 是 Looping（要先跑 Art/Audio/ue_import_sfx.py）。
//   Levels ：区域 → 第几关（关卡标题）和灰盒一致。
#include "Audio/DysisSfxDirector.h"
#include "Audio/DysisSfxSettings.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"

namespace DysisSfxTestKeys
{
	// 代码里直接用到的事件名（Director / 脚步组件 / 蓝图库）。改了代码里的名字记得这里也改。
	static const TCHAR* const Used[] = {
		TEXT("Foot.Stone.Walk"), TEXT("Foot.Stone.Run"), TEXT("Foot.Stone.Scuff"), TEXT("Foot.Light.Walk"), TEXT("Foot.Light.Run"),
		TEXT("Foot.Moon.Walk"), TEXT("Foot.Moon.Run"), TEXT("Foot.Shadow.Walk"), TEXT("Foot.Shadow.Run"), TEXT("Foot.Bronze.Walk"),
		TEXT("Foot.Bronze.Run"), TEXT("Foot.Water.Walk"), TEXT("Foot.Water.Run"), TEXT("Foot.WetStone.Walk"),
		TEXT("Player.Jump"), TEXT("Player.Land.Stone"), TEXT("Player.Land.Light"), TEXT("Player.Land.Bronze"), TEXT("Player.Fall.Start"),
		TEXT("Player.Fall.Loop"), TEXT("Player.Respawn.Day"), TEXT("Player.Respawn.Night"),
		TEXT("Light.Reveal"), TEXT("Light.RevealOpening"), TEXT("Moon.Stone.Appear"), TEXT("Moon.Stone.Vanish"), TEXT("Moon.Wall.Vanish"),
		TEXT("Amb.Waterfall.Near"), TEXT("Amb.Waterfall.Far"), TEXT("Amb.Sea.Close"), TEXT("Amb.Sea.Far"), TEXT("Amb.RoofWind"),
		TEXT("Amb.RoomTone"), TEXT("Amb.Pool"), TEXT("Amb.DayBirds"), TEXT("Amb.Mist"),
		TEXT("Mech.Sluice.Wheel"), TEXT("Mech.Waterfall.Start"), TEXT("Mech.Waterfall.Stop"), TEXT("Mech.Steps.Rise"), TEXT("Mech.Steps.Settle"),
		TEXT("Mech.Steps.MoveLoop"), TEXT("Mech.Lever.Pull"), TEXT("Mech.Lever.Push"), TEXT("Mech.Slab.Slide"), TEXT("Mech.Iris.MoveLoop"),
		TEXT("Mech.Gate.Open"), TEXT("Mech.Gate.Close"), TEXT("Mech.Statue.Turn"), TEXT("Mech.Statue.PushStart"), TEXT("Mech.Statue.PushLoop"),
		TEXT("Mech.Statue.PushStop"), TEXT("Mech.MoonBridge.Extend"), TEXT("Mech.SwanRelief.Sink"), TEXT("Mech.Stairs.Lower"),
		TEXT("Mech.WallStairs.RevealTS"), TEXT("Mech.WallStairs.RevealTR"), TEXT("Mech.HalfBridge.Extend"), TEXT("Mech.Prism.Turn"),
		TEXT("Mech.Sill.Extend"), TEXT("Mech.Niche.DoorsOpen"), TEXT("Mech.Prism.ColumnRise"), TEXT("Mech.SunNiche.Open"),
		TEXT("Story.Selene.DroneLow"), TEXT("Story.Selene.DroneHigh"), TEXT("Story.Selene.Lock"), TEXT("Story.Selene.Awaken"),
		TEXT("Story.Twins.Light"), TEXT("Story.ShadowBridge.Join"), TEXT("Story.Apple.Place"), TEXT("Story.Apple.Take"),
		TEXT("Story.Apple.HoldLoop"), TEXT("Story.Shard.Sun"), TEXT("Story.Shard.Moon"), TEXT("Story.Shard.Rainbow"),
		TEXT("Story.Swan.Transform"), TEXT("Story.IrisRelief.Awaken"), TEXT("Story.RainbowBridge.Appear"), TEXT("Story.Selene.Eyes"),
		TEXT("Story.Mirror.Awaken"), TEXT("Story.Dodecahedron"), TEXT("Story.RainbowGate"), TEXT("Story.Selene.SleepTalk"),
		TEXT("UI.Hover"), TEXT("UI.Confirm"), TEXT("UI.Back"), TEXT("UI.StartGame"), TEXT("UI.Title"), TEXT("UI.Prompt"), TEXT("UI.Text"),
		TEXT("UI.DialogueNext"), TEXT("UI.ShardHover.Sun"), TEXT("UI.ShardHover.Moon"), TEXT("UI.ShardHover.Rainbow"),
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisSfxTableTest, "Dysis.Sfx.Table",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisSfxTableTest::RunTest(const FString&)
{
	TArray<FDysisSfxEvent> Factory;
	DysisSfxDefaults::Fill(Factory);
	TestTrue(TEXT("出厂表不空"), Factory.Num() > 90);

	TSet<FName> Keys;
	int32 Files = 0;
	for (const FDysisSfxEvent& E : Factory)
	{
		TestFalse(FString::Printf(TEXT("%s 名字重复"), *E.Key.ToString()), Keys.Contains(E.Key));
		Keys.Add(E.Key);
		TestTrue(FString::Printf(TEXT("%s 至少一个文件"), *E.Key.ToString()), E.Sounds.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s 音量在 0–4"), *E.Key.ToString()), E.Volume >= 0.0f && E.Volume <= 4.0f);
		TestTrue(FString::Printf(TEXT("%s 快慢在 0.25–4"), *E.Key.ToString()), E.Pitch >= 0.25f && E.Pitch <= 4.0f);
		Files += E.Sounds.Num();
	}
	TestEqual(TEXT("文件总数（和 Art/Audio/manifest.json 一致）"), Files, 265);

	// 代码里用到的名字都在表里，设置里也找得到（ini 是旧的也会被补上）。
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	for (const TCHAR* K : DysisSfxTestKeys::Used)
	{
		TestTrue(FString::Printf(TEXT("出厂表里有 %s"), K), Keys.Contains(FName(K)));
		TestNotNull(FString::Printf(TEXT("设置里有 %s"), K), S->FindEvent(FName(K)));
	}

	// 有编号的几项，个数要对（第 1–16 级、红到紫 7 格、12 个标题）。
	auto Count = [&Factory](const TCHAR* K)
	{
		for (const FDysisSfxEvent& E : Factory) if (E.Key == FName(K)) return E.Sounds.Num();
		return 0;
	};
	TestEqual(TEXT("踏步升起 16 个"), Count(TEXT("Mech.Steps.Rise")), 16);
	TestEqual(TEXT("下行梯落平 16 个"), Count(TEXT("Mech.Steps.Settle")), 16);
	TestEqual(TEXT("棱镜 7 个音"), Count(TEXT("Mech.Prism.Turn")), 7);
	TestEqual(TEXT("关卡标题 12 个"), Count(TEXT("UI.Title")), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisSfxAssetsTest, "Dysis.Sfx.Assets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisSfxAssetsTest::RunTest(const FString&)
{
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	int32 Missing = 0;
	for (const FDysisSfxEvent& E : S->Events)
	{
		for (const TSoftObjectPtr<USoundBase>& Snd : E.Sounds)
		{
			USoundBase* Sound = Snd.LoadSynchronous();
			if (!Sound)
			{
				++Missing;
				AddError(FString::Printf(TEXT("%s：没导入 %s（跑 Art/Audio/ue_import_sfx.py）"), *E.Key.ToString(), *Snd.ToString()));
				continue;
			}
			if (const USoundWave* Wave = Cast<USoundWave>(Sound))
			{
				TestEqual(FString::Printf(TEXT("%s 的 Looping"), *Wave->GetName()), static_cast<bool>(Wave->bLooping), E.bLoop);
			}
		}
	}
	TestEqual(TEXT("缺的声音文件"), Missing, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisSfxLevelsTest, "Dysis.Sfx.Levels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisSfxLevelsTest::RunTest(const FString&)
{
	// 灰盒 levelOf 的对照（prototype/temple/index.html）。
	TestEqual(TEXT("岛上=序"), ADysisSfxDirector::LevelOf(TEXT("out"), false), 0);
	TestEqual(TEXT("L0 白天=日1"), ADysisSfxDirector::LevelOf(TEXT("L0"), false), 1);
	TestEqual(TEXT("虹=日2"), ADysisSfxDirector::LevelOf(TEXT("rainbow"), false), 2);
	TestEqual(TEXT("光阶 h2=日3"), ADysisSfxDirector::LevelOf(TEXT("beam:h2"), false), 3);
	TestEqual(TEXT("L3 白天=日4"), ADysisSfxDirector::LevelOf(TEXT("L3"), false), 4);
	TestEqual(TEXT("屋顶白天=日5"), ADysisSfxDirector::LevelOf(TEXT("crown"), false), 5);
	TestEqual(TEXT("屋顶夜里=月1"), ADysisSfxDirector::LevelOf(TEXT("crown"), true), 6);
	TestEqual(TEXT("TS 楼梯=月2"), ADysisSfxDirector::LevelOf(TEXT("tun:TS"), true), 7);
	TestEqual(TEXT("TR 楼梯=月3"), ADysisSfxDirector::LevelOf(TEXT("tun:TR"), true), 8);
	TestEqual(TEXT("月桥=月4"), ADysisSfxDirector::LevelOf(TEXT("moonbr"), true), 9);
	TestEqual(TEXT("水亭=月5"), ADysisSfxDirector::LevelOf(TEXT("pav"), true), 10);
	TestEqual(TEXT("不认识的区域"), ADysisSfxDirector::LevelOf(TEXT("nowhere"), true), -1);
	return true;
}
