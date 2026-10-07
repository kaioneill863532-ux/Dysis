// 狄西斯的日落回廊 · 音效默认表（101 项、265 个文件）
// 由 Art/Audio/tools/gen_sfx_table.py 从 Art/Audio/manifest.json + sfx_events.py 生成，不要手改。
// 运行时这些只是“出厂值”：Project Settings → Game → Dysis 音效 里改的音量、快慢等存在 Config/DefaultGame.ini，优先于这里。
#include "Audio/DysisSfxSettings.h"
#include "Sound/SoundBase.h"

namespace DysisSfxDefaults
{
	static FDysisSfxEvent Make(const TCHAR* Key, const TCHAR* Label, EDysisSfxCategory Category, bool b2D,
		float InnerCm, float FalloffCm, float Volume, float Pitch, float VolumeJitter, float PitchJitter,
		bool bNoRepeat, float Cooldown, int32 MaxInstances, bool bLoop, float ReverbSend, std::initializer_list<const TCHAR*> Paths)
	{
		FDysisSfxEvent E;
		E.Key = FName(Key);
		E.Label = Label;
		E.Category = Category;
		E.b2D = b2D;
		E.InnerRadiusCm = InnerCm;
		E.FalloffDistanceCm = FalloffCm;
		E.Volume = Volume;
		E.Pitch = Pitch;
		E.VolumeJitter = VolumeJitter;
		E.PitchJitter = PitchJitter;
		E.bNoRepeat = bNoRepeat;
		E.CooldownSeconds = Cooldown;
		E.MaxInstances = MaxInstances;
		E.bLoop = bLoop;
		E.ReverbSend = ReverbSend;
		for (const TCHAR* P : Paths)
		{
			E.Sounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(P)));
		}
		return E;
	}

	void Fill(TArray<FDysisSfxEvent>& Out)
	{
		Out.Reset();
		using C = EDysisSfxCategory;
		Out.Add(Make(TEXT("Foot.Stone.Walk"), TEXT("脚步·石头·走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.04f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_01.SFX_Footstep_Stone_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_02.SFX_Footstep_Stone_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_03.SFX_Footstep_Stone_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_04.SFX_Footstep_Stone_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_05.SFX_Footstep_Stone_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_06.SFX_Footstep_Stone_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_07.SFX_Footstep_Stone_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_08.SFX_Footstep_Stone_Walk_08"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_09.SFX_Footstep_Stone_Walk_09"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Walk_10.SFX_Footstep_Stone_Walk_10") }));
		Out.Add(Make(TEXT("Foot.Stone.Run"), TEXT("脚步·石头·快走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.04f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_01.SFX_Footstep_Stone_Run_01"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_02.SFX_Footstep_Stone_Run_02"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_03.SFX_Footstep_Stone_Run_03"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_04.SFX_Footstep_Stone_Run_04"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_05.SFX_Footstep_Stone_Run_05"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_06.SFX_Footstep_Stone_Run_06"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_07.SFX_Footstep_Stone_Run_07"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Run_08.SFX_Footstep_Stone_Run_08") }));
		Out.Add(Make(TEXT("Foot.Stone.Scuff"), TEXT("脚步·石头·急停蹭地"), C::Footsteps, true, 300.0f, 3000.0f, 0.8f, 1.0f, 0.0f, 0.03f, true, 0.8f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Scuff_01.SFX_Footstep_Stone_Scuff_01"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Scuff_02.SFX_Footstep_Stone_Scuff_02"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Scuff_03.SFX_Footstep_Stone_Scuff_03"), TEXT("/Game/Dysis/Audio/SFX/03_Footstep_Stone/SFX_Footstep_Stone_Scuff_04.SFX_Footstep_Stone_Scuff_04") }));
		Out.Add(Make(TEXT("Foot.Light.Walk"), TEXT("脚步·光路·走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_01.SFX_Footstep_Light_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_02.SFX_Footstep_Light_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_03.SFX_Footstep_Light_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_04.SFX_Footstep_Light_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_05.SFX_Footstep_Light_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_06.SFX_Footstep_Light_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_07.SFX_Footstep_Light_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_08.SFX_Footstep_Light_Walk_08"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_09.SFX_Footstep_Light_Walk_09"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Walk_10.SFX_Footstep_Light_Walk_10") }));
		Out.Add(Make(TEXT("Foot.Light.Run"), TEXT("脚步·光路·快走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Run_01.SFX_Footstep_Light_Run_01"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Run_02.SFX_Footstep_Light_Run_02"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Run_03.SFX_Footstep_Light_Run_03"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Run_04.SFX_Footstep_Light_Run_04"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Run_05.SFX_Footstep_Light_Run_05"), TEXT("/Game/Dysis/Audio/SFX/04_Footstep_LightPath/SFX_Footstep_Light_Run_06.SFX_Footstep_Light_Run_06") }));
		Out.Add(Make(TEXT("Foot.Moon.Walk"), TEXT("脚步·月石/月桥/夜光石·走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_01.SFX_Footstep_Moonstone_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_02.SFX_Footstep_Moonstone_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_03.SFX_Footstep_Moonstone_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_04.SFX_Footstep_Moonstone_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_05.SFX_Footstep_Moonstone_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_06.SFX_Footstep_Moonstone_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_07.SFX_Footstep_Moonstone_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Walk_08.SFX_Footstep_Moonstone_Walk_08") }));
		Out.Add(Make(TEXT("Foot.Moon.Run"), TEXT("脚步·月石/月桥/夜光石·快走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Run_01.SFX_Footstep_Moonstone_Run_01"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Run_02.SFX_Footstep_Moonstone_Run_02"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Run_03.SFX_Footstep_Moonstone_Run_03"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Run_04.SFX_Footstep_Moonstone_Run_04"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Run_05.SFX_Footstep_Moonstone_Run_05"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_Moonstone_Run_06.SFX_Footstep_Moonstone_Run_06") }));
		Out.Add(Make(TEXT("Foot.Shadow.Walk"), TEXT("脚步·影桥/月光踏片/月光大道·走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_01.SFX_Footstep_ShadowBridge_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_02.SFX_Footstep_ShadowBridge_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_03.SFX_Footstep_ShadowBridge_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_04.SFX_Footstep_ShadowBridge_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_05.SFX_Footstep_ShadowBridge_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_06.SFX_Footstep_ShadowBridge_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_07.SFX_Footstep_ShadowBridge_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Walk_08.SFX_Footstep_ShadowBridge_Walk_08") }));
		Out.Add(Make(TEXT("Foot.Shadow.Run"), TEXT("脚步·影桥/月光踏片/月光大道·快走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Run_01.SFX_Footstep_ShadowBridge_Run_01"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Run_02.SFX_Footstep_ShadowBridge_Run_02"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Run_03.SFX_Footstep_ShadowBridge_Run_03"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Run_04.SFX_Footstep_ShadowBridge_Run_04"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Run_05.SFX_Footstep_ShadowBridge_Run_05"), TEXT("/Game/Dysis/Audio/SFX/32_Footstep_Moon_Shadow/SFX_Footstep_ShadowBridge_Run_06.SFX_Footstep_ShadowBridge_Run_06") }));
		Out.Add(Make(TEXT("Foot.Bronze.Walk"), TEXT("脚步·青铜·走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.03f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_01.SFX_Footstep_Bronze_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_02.SFX_Footstep_Bronze_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_03.SFX_Footstep_Bronze_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_04.SFX_Footstep_Bronze_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_05.SFX_Footstep_Bronze_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_06.SFX_Footstep_Bronze_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_07.SFX_Footstep_Bronze_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Walk_08.SFX_Footstep_Bronze_Walk_08") }));
		Out.Add(Make(TEXT("Foot.Bronze.Run"), TEXT("脚步·青铜·快走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.03f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Run_01.SFX_Footstep_Bronze_Run_01"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Run_02.SFX_Footstep_Bronze_Run_02"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Run_03.SFX_Footstep_Bronze_Run_03"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Run_04.SFX_Footstep_Bronze_Run_04"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Run_05.SFX_Footstep_Bronze_Run_05"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Footstep_Bronze_Run_06.SFX_Footstep_Bronze_Run_06") }));
		Out.Add(Make(TEXT("Foot.Water.Walk"), TEXT("脚步·浅水·走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.04f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_01.SFX_Footstep_Water_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_02.SFX_Footstep_Water_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_03.SFX_Footstep_Water_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_04.SFX_Footstep_Water_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_05.SFX_Footstep_Water_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_06.SFX_Footstep_Water_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_07.SFX_Footstep_Water_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Walk_08.SFX_Footstep_Water_Walk_08") }));
		Out.Add(Make(TEXT("Foot.Water.Run"), TEXT("脚步·浅水·快走"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.04f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Run_01.SFX_Footstep_Water_Run_01"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Run_02.SFX_Footstep_Water_Run_02"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Run_03.SFX_Footstep_Water_Run_03"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_Water_Run_04.SFX_Footstep_Water_Run_04") }));
		Out.Add(Make(TEXT("Foot.WetStone.Walk"), TEXT("脚步·湿石（走和快走共用）"), C::Footsteps, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.05f, 0.04f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_01.SFX_Footstep_WetStone_Walk_01"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_02.SFX_Footstep_WetStone_Walk_02"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_03.SFX_Footstep_WetStone_Walk_03"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_04.SFX_Footstep_WetStone_Walk_04"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_05.SFX_Footstep_WetStone_Walk_05"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_06.SFX_Footstep_WetStone_Walk_06"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_07.SFX_Footstep_WetStone_Walk_07"), TEXT("/Game/Dysis/Audio/SFX/44_Footstep_Water_Wet/SFX_Footstep_WetStone_Walk_08.SFX_Footstep_WetStone_Walk_08") }));
		Out.Add(Make(TEXT("Player.Jump"), TEXT("起跳"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.03f, true, 0.15f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Jump_01.SFX_Jump_01"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Jump_02.SFX_Jump_02"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Jump_03.SFX_Jump_03"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Jump_04.SFX_Jump_04") }));
		Out.Add(Make(TEXT("Player.Land.Stone"), TEXT("落地·石头"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.03f, true, 0.15f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Stone_01.SFX_Land_Stone_01"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Stone_02.SFX_Land_Stone_02"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Stone_03.SFX_Land_Stone_03"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Stone_04.SFX_Land_Stone_04") }));
		Out.Add(Make(TEXT("Player.Land.Light"), TEXT("落地·光路/月石"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.15f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Light_01.SFX_Land_Light_01"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Light_02.SFX_Land_Light_02"), TEXT("/Game/Dysis/Audio/SFX/08_Jump_Land/SFX_Land_Light_03.SFX_Land_Light_03") }));
		Out.Add(Make(TEXT("Player.Land.Bronze"), TEXT("落地·青铜"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.15f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Land_Bronze_01.SFX_Land_Bronze_01"), TEXT("/Game/Dysis/Audio/SFX/28_Footstep_Bronze/SFX_Land_Bronze_02.SFX_Land_Bronze_02") }));
		Out.Add(Make(TEXT("Player.Fall.Start"), TEXT("掉落开始"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 1.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/09_Fall_Respawn/SFX_Fall_Start.SFX_Fall_Start") }));
		Out.Add(Make(TEXT("Player.Fall.Loop"), TEXT("掉落中（循环）"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.3f,
			{ TEXT("/Game/Dysis/Audio/SFX/09_Fall_Respawn/SFX_Fall_Loop.SFX_Fall_Loop") }));
		Out.Add(Make(TEXT("Player.Respawn.Day"), TEXT("回到落脚点·白天"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.5f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/09_Fall_Respawn/SFX_Respawn_Day.SFX_Respawn_Day") }));
		Out.Add(Make(TEXT("Player.Respawn.Night"), TEXT("回到落脚点·夜里"), C::Player, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.5f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/09_Fall_Respawn/SFX_Respawn_Night.SFX_Respawn_Night") }));
		Out.Add(Make(TEXT("Light.Reveal"), TEXT("光路显形"), C::Story, false, 400.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 1.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/05_LightPath_Reveal/SFX_LightPath_Reveal_01.SFX_LightPath_Reveal_01"), TEXT("/Game/Dysis/Audio/SFX/05_LightPath_Reveal/SFX_LightPath_Reveal_02.SFX_LightPath_Reveal_02"), TEXT("/Game/Dysis/Audio/SFX/05_LightPath_Reveal/SFX_LightPath_Reveal_03.SFX_LightPath_Reveal_03"), TEXT("/Game/Dysis/Audio/SFX/05_LightPath_Reveal/SFX_LightPath_Reveal_04.SFX_LightPath_Reveal_04"), TEXT("/Game/Dysis/Audio/SFX/05_LightPath_Reveal/SFX_LightPath_Reveal_05.SFX_LightPath_Reveal_05"), TEXT("/Game/Dysis/Audio/SFX/05_LightPath_Reveal/SFX_LightPath_Reveal_06.SFX_LightPath_Reveal_06") }));
		Out.Add(Make(TEXT("Light.RevealOpening"), TEXT("开场光路显形（标题音）"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/30_LightPath_Reveal_Opening/SFX_LightPath_Reveal_Opening.SFX_LightPath_Reveal_Opening") }));
		Out.Add(Make(TEXT("Moon.Stone.Appear"), TEXT("夜光石/月石亮起"), C::Story, false, 200.0f, 1800.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.12f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Appear_01.SFX_Moonstone_Appear_01"), TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Appear_02.SFX_Moonstone_Appear_02"), TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Appear_03.SFX_Moonstone_Appear_03"), TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Appear_04.SFX_Moonstone_Appear_04") }));
		Out.Add(Make(TEXT("Moon.Stone.Vanish"), TEXT("夜光石/月石暗下去"), C::Story, false, 200.0f, 1800.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.12f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Vanish_01.SFX_Moonstone_Vanish_01"), TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Vanish_02.SFX_Moonstone_Vanish_02"), TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Vanish_03.SFX_Moonstone_Vanish_03") }));
		Out.Add(Make(TEXT("Moon.Wall.Vanish"), TEXT("大块月石隐去（月亮浮雕、月之龛、双子墙）"), C::Story, false, 400.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 2, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/06_Moonstone/SFX_Moonstone_Wall_Vanish.SFX_Moonstone_Wall_Vanish") }));
		Out.Add(Make(TEXT("Amb.Waterfall.Near"), TEXT("瀑布·近（循环）"), C::Ambience, false, 600.0f, 1900.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.5f,
			{ TEXT("/Game/Dysis/Audio/SFX/10_Waterfall/SFX_Waterfall_Near_Loop.SFX_Waterfall_Near_Loop") }));
		Out.Add(Make(TEXT("Amb.Waterfall.Far"), TEXT("瀑布·远（循环）"), C::Ambience, false, 1500.0f, 4500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.5f,
			{ TEXT("/Game/Dysis/Audio/SFX/10_Waterfall/SFX_Waterfall_Far_Loop.SFX_Waterfall_Far_Loop") }));
		Out.Add(Make(TEXT("Amb.Sea.Close"), TEXT("海浪·近（循环，殿外低处）"), C::Ambience, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/11_Sea_Waves/SFX_Sea_Waves_Close_Loop.SFX_Sea_Waves_Close_Loop") }));
		Out.Add(Make(TEXT("Amb.Sea.Far"), TEXT("海浪·远（循环，一直在）"), C::Ambience, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/11_Sea_Waves/SFX_Sea_Waves_Far_Loop.SFX_Sea_Waves_Far_Loop") }));
		Out.Add(Make(TEXT("Amb.RoofWind"), TEXT("海风（循环，越高越响）"), C::Ambience, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/38_Roof_Wind/SFX_Roof_Wind_Loop.SFX_Roof_Wind_Loop") }));
		Out.Add(Make(TEXT("Amb.RoomTone"), TEXT("殿内空间底噪（循环）"), C::Ambience, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/39_Interior_RoomTone/SFX_Interior_RoomTone_Loop.SFX_Interior_RoomTone_Loop") }));
		Out.Add(Make(TEXT("Amb.Pool"), TEXT("水池水面（循环）"), C::Ambience, false, 800.0f, 1200.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.6f,
			{ TEXT("/Game/Dysis/Audio/SFX/40_Pool_Water/SFX_Pool_Water_Loop.SFX_Pool_Water_Loop") }));
		Out.Add(Make(TEXT("Amb.DayBirds"), TEXT("白天鸟鸣（循环）"), C::Ambience, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/43_Day_Birds/SFX_Day_Birds_Loop.SFX_Day_Birds_Loop") }));
		Out.Add(Make(TEXT("Amb.Mist"), TEXT("水雾（循环，开闸以后）"), C::Ambience, false, 1000.0f, 1500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 0.3f,
			{ TEXT("/Game/Dysis/Audio/SFX/55_Ambience_Layers/SFX_Mist_Loop.SFX_Mist_Loop") }));
		Out.Add(Make(TEXT("Mech.Sluice.Wheel"), TEXT("水闸轮转动"), C::Mechanism, false, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/14_Sluice_Waterfall_Start/SFX_Sluice_Wheel_Turn.SFX_Sluice_Wheel_Turn") }));
		Out.Add(Make(TEXT("Mech.Waterfall.Start"), TEXT("瀑布开始流"), C::Mechanism, false, 800.0f, 4500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/14_Sluice_Waterfall_Start/SFX_Waterfall_Start.SFX_Waterfall_Start") }));
		Out.Add(Make(TEXT("Mech.Waterfall.Stop"), TEXT("瀑布停"), C::Mechanism, false, 800.0f, 4500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/14_Sluice_Waterfall_Start/SFX_Waterfall_Stop.SFX_Waterfall_Stop") }));
		Out.Add(Make(TEXT("Mech.Steps.Rise"), TEXT("屋顶踏步升起（第 1–16 级，各一个音）"), C::Mechanism, false, 300.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, false, 0.0f, 4, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_01.SFX_Steps_Rise_01"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_02.SFX_Steps_Rise_02"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_03.SFX_Steps_Rise_03"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_04.SFX_Steps_Rise_04"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_05.SFX_Steps_Rise_05"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_06.SFX_Steps_Rise_06"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_07.SFX_Steps_Rise_07"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_08.SFX_Steps_Rise_08"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_09.SFX_Steps_Rise_09"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_10.SFX_Steps_Rise_10"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_11.SFX_Steps_Rise_11"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_12.SFX_Steps_Rise_12"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_13.SFX_Steps_Rise_13"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_14.SFX_Steps_Rise_14"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_15.SFX_Steps_Rise_15"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Rise_16.SFX_Steps_Rise_16") }));
		Out.Add(Make(TEXT("Mech.Steps.Settle"), TEXT("下行梯落平（第 1–16 级）"), C::Mechanism, false, 300.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, false, 0.0f, 4, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_01.SFX_Steps_Settle_01"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_02.SFX_Steps_Settle_02"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_03.SFX_Steps_Settle_03"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_04.SFX_Steps_Settle_04"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_05.SFX_Steps_Settle_05"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_06.SFX_Steps_Settle_06"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_07.SFX_Steps_Settle_07"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_08.SFX_Steps_Settle_08"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_09.SFX_Steps_Settle_09"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_10.SFX_Steps_Settle_10"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_11.SFX_Steps_Settle_11"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_12.SFX_Steps_Settle_12"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_13.SFX_Steps_Settle_13"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_14.SFX_Steps_Settle_14"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_15.SFX_Steps_Settle_15"), TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Settle_16.SFX_Steps_Settle_16") }));
		Out.Add(Make(TEXT("Mech.Steps.MoveLoop"), TEXT("踏步移动中（循环，跟速度走）"), C::Mechanism, false, 400.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/15_Steps_Rise_Settle/SFX_Steps_Move_Loop.SFX_Steps_Move_Loop") }));
		Out.Add(Make(TEXT("Mech.Lever.Pull"), TEXT("拉杆拉下"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/16_Lever_StoneSlab/SFX_Lever_Pull.SFX_Lever_Pull") }));
		Out.Add(Make(TEXT("Mech.Lever.Push"), TEXT("拉杆推回"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/16_Lever_StoneSlab/SFX_Lever_Push.SFX_Lever_Push") }));
		Out.Add(Make(TEXT("Mech.Slab.Slide"), TEXT("石板滑动（远处传来）"), C::Mechanism, false, 500.0f, 4500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 2, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/16_Lever_StoneSlab/SFX_Slab_Slide_01.SFX_Slab_Slide_01"), TEXT("/Game/Dysis/Audio/SFX/16_Lever_StoneSlab/SFX_Slab_Slide_02.SFX_Slab_Slide_02") }));
		Out.Add(Make(TEXT("Mech.Iris.Open"), TEXT("光圈叶片打开"), C::Mechanism, false, 600.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/17_Iris_Blades/SFX_Iris_Open.SFX_Iris_Open") }));
		Out.Add(Make(TEXT("Mech.Iris.Close"), TEXT("光圈叶片合上"), C::Mechanism, false, 600.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/17_Iris_Blades/SFX_Iris_Close.SFX_Iris_Close") }));
		Out.Add(Make(TEXT("Mech.Iris.MoveLoop"), TEXT("光圈叶片慢慢动（循环）"), C::Mechanism, false, 600.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/17_Iris_Blades/SFX_Iris_Move_Loop.SFX_Iris_Move_Loop") }));
		Out.Add(Make(TEXT("Mech.Gate.Open"), TEXT("屋顶桥门打开"), C::Mechanism, false, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/18_Bridge_Gate/SFX_Gate_Open.SFX_Gate_Open") }));
		Out.Add(Make(TEXT("Mech.Gate.Close"), TEXT("屋顶桥门关上"), C::Mechanism, false, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/18_Bridge_Gate/SFX_Gate_Close.SFX_Gate_Close") }));
		Out.Add(Make(TEXT("Mech.Statue.Turn"), TEXT("转动雕像底座（三相像、天鹅女神像）"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.03f, true, 0.2f, 2, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/19_Statue_Turn/SFX_Statue_Turn_01.SFX_Statue_Turn_01"), TEXT("/Game/Dysis/Audio/SFX/19_Statue_Turn/SFX_Statue_Turn_02.SFX_Statue_Turn_02"), TEXT("/Game/Dysis/Audio/SFX/19_Statue_Turn/SFX_Statue_Turn_03.SFX_Statue_Turn_03"), TEXT("/Game/Dysis/Audio/SFX/19_Statue_Turn/SFX_Statue_Turn_04.SFX_Statue_Turn_04") }));
		Out.Add(Make(TEXT("Mech.Statue.PullOut"), TEXT("拉出雕像"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/21_Statue_Push_Pull/SFX_Statue_PullOut.SFX_Statue_PullOut") }));
		Out.Add(Make(TEXT("Mech.Statue.PushStart"), TEXT("推雕像·起步"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/21_Statue_Push_Pull/SFX_Statue_Push_Start.SFX_Statue_Push_Start") }));
		Out.Add(Make(TEXT("Mech.Statue.PushLoop"), TEXT("推雕像·推动中（循环）"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/21_Statue_Push_Pull/SFX_Statue_Push_Loop.SFX_Statue_Push_Loop") }));
		Out.Add(Make(TEXT("Mech.Statue.PushStop"), TEXT("推雕像·停下"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/21_Statue_Push_Pull/SFX_Statue_Push_Stop.SFX_Statue_Push_Stop") }));
		Out.Add(Make(TEXT("Mech.MoonBridge.Extend"), TEXT("月桥伸出"), C::Mechanism, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/22_Twins_MoonBridge/SFX_MoonBridge_Extend.SFX_MoonBridge_Extend") }));
		Out.Add(Make(TEXT("Mech.SwanRelief.Sink"), TEXT("天鹅浮雕下沉"), C::Mechanism, false, 400.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/23_SwanRelief_Sink/SFX_SwanRelief_Sink.SFX_SwanRelief_Sink") }));
		Out.Add(Make(TEXT("Mech.Stairs.Lower"), TEXT("下行梯开始降下"), C::Mechanism, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/24_Stairs_Lower/SFX_Stairs_Lower.SFX_Stairs_Lower") }));
		Out.Add(Make(TEXT("Mech.WallStairs.RevealTS"), TEXT("墙里楼梯显现·TS（天鹅解开）"), C::Mechanism, false, 400.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/24_Stairs_Lower/SFX_WallStairs_Reveal_TS.SFX_WallStairs_Reveal_TS") }));
		Out.Add(Make(TEXT("Mech.WallStairs.RevealTR"), TEXT("墙里楼梯显现·TR（月亮浮雕隐去）"), C::Mechanism, false, 400.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/24_Stairs_Lower/SFX_WallStairs_Reveal_TR.SFX_WallStairs_Reveal_TR") }));
		Out.Add(Make(TEXT("Mech.WallStairs.Window"), TEXT("墙里楼梯的窗（单块，默认不用）"), C::Mechanism, false, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 3, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/54_WallStairs_Windows/SFX_WallStairs_Window_01.SFX_WallStairs_Window_01"), TEXT("/Game/Dysis/Audio/SFX/54_WallStairs_Windows/SFX_WallStairs_Window_02.SFX_WallStairs_Window_02"), TEXT("/Game/Dysis/Audio/SFX/54_WallStairs_Windows/SFX_WallStairs_Window_03.SFX_WallStairs_Window_03") }));
		Out.Add(Make(TEXT("Mech.HalfBridge.Extend"), TEXT("半桥伸出"), C::Mechanism, false, 400.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/25_Selene_Awaken_HalfBridge/SFX_HalfBridge_Extend.SFX_HalfBridge_Extend") }));
		Out.Add(Make(TEXT("Mech.Prism.Turn"), TEXT("棱镜转台转一格（红→紫 7 个音）"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, false, 0.0f, 2, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_1_Red.SFX_Prism_Turn_1_Red"), TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_2_Orange.SFX_Prism_Turn_2_Orange"), TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_3_Yellow.SFX_Prism_Turn_3_Yellow"), TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_4_Green.SFX_Prism_Turn_4_Green"), TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_5_Blue.SFX_Prism_Turn_5_Blue"), TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_6_Indigo.SFX_Prism_Turn_6_Indigo"), TEXT("/Game/Dysis/Audio/SFX/47_Prism_Turn/SFX_Prism_Turn_7_Violet.SFX_Prism_Turn_7_Violet") }));
		Out.Add(Make(TEXT("Mech.Sill.Extend"), TEXT("窗下石沿伸出"), C::Mechanism, false, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/49_Rainbow_Small_Mechs/SFX_Sill_Ledge_Extend.SFX_Sill_Ledge_Extend") }));
		Out.Add(Make(TEXT("Mech.Niche.DoorsOpen"), TEXT("虹之龛铜门打开"), C::Mechanism, false, 200.0f, 2000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/49_Rainbow_Small_Mechs/SFX_Niche_Doors_Open.SFX_Niche_Doors_Open") }));
		Out.Add(Make(TEXT("Mech.Prism.ColumnRise"), TEXT("棱镜铜柱升起"), C::Mechanism, false, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/49_Rainbow_Small_Mechs/SFX_Prism_Column_Rise.SFX_Prism_Column_Rise") }));
		Out.Add(Make(TEXT("Mech.SunNiche.Open"), TEXT("日之龛开盖"), C::Mechanism, false, 200.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/50_Mirror_SunNiche/SFX_SunNiche_Open.SFX_SunNiche_Open") }));
		Out.Add(Make(TEXT("Story.Selene.DroneLow"), TEXT("塞勒涅怀里的月亮·低音（循环）"), C::Story, false, 600.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/20_Selene_MoonPhase/SFX_Selene_MoonDrone_Low_Loop.SFX_Selene_MoonDrone_Low_Loop") }));
		Out.Add(Make(TEXT("Story.Selene.DroneHigh"), TEXT("塞勒涅怀里的月亮·高音（循环）"), C::Story, false, 600.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/20_Selene_MoonPhase/SFX_Selene_MoonDrone_High_Loop.SFX_Selene_MoonDrone_High_Loop") }));
		Out.Add(Make(TEXT("Story.Selene.Lock"), TEXT("月光对准月亮（Lock）"), C::Story, false, 600.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/20_Selene_MoonPhase/SFX_Selene_MoonDrone_Lock.SFX_Selene_MoonDrone_Lock") }));
		Out.Add(Make(TEXT("Story.Selene.Awaken"), TEXT("塞勒涅神像亮起"), C::Story, false, 600.0f, 4500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/25_Selene_Awaken_HalfBridge/SFX_Selene_Awaken.SFX_Selene_Awaken") }));
		Out.Add(Make(TEXT("Story.Twins.Light"), TEXT("双子亮起"), C::Story, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/22_Twins_MoonBridge/SFX_Twins_Light.SFX_Twins_Light") }));
		Out.Add(Make(TEXT("Story.ShadowBridge.Join"), TEXT("影桥接上（月光大道出现）"), C::Story, false, 600.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 5.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/26_ShadowBridge_Join/SFX_ShadowBridge_Join.SFX_ShadowBridge_Join") }));
		Out.Add(Make(TEXT("Story.Apple.Place"), TEXT("放上金苹果"), C::Story, false, 400.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/27_Apple_Place/SFX_Apple_Place.SFX_Apple_Place") }));
		Out.Add(Make(TEXT("Story.Apple.Take"), TEXT("取下金苹果（接住最后一缕光）"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/27_Apple_Place/SFX_Apple_Take.SFX_Apple_Take") }));
		Out.Add(Make(TEXT("Story.Apple.HoldLoop"), TEXT("捧着金苹果（循环）"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, true, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/31_Apple_Hold/SFX_Apple_Hold_Loop.SFX_Apple_Hold_Loop") }));
		Out.Add(Make(TEXT("Story.Shard.Sun"), TEXT("拾取日之碎片"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/33_Shard_Pickup/SFX_Shard_Pickup_Sun.SFX_Shard_Pickup_Sun") }));
		Out.Add(Make(TEXT("Story.Shard.Moon"), TEXT("拾取月之碎片"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/33_Shard_Pickup/SFX_Shard_Pickup_Moon.SFX_Shard_Pickup_Moon") }));
		Out.Add(Make(TEXT("Story.Shard.Rainbow"), TEXT("拾取虹之碎片"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/33_Shard_Pickup/SFX_Shard_Pickup_Rainbow.SFX_Shard_Pickup_Rainbow") }));
		Out.Add(Make(TEXT("Story.Swan.Transform"), TEXT("女神像变天鹅"), C::Story, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/41_Swan_Transform/SFX_Swan_Transform.SFX_Swan_Transform") }));
		Out.Add(Make(TEXT("Story.Swan.Revert"), TEXT("天鹅变回女神像"), C::Story, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/41_Swan_Transform/SFX_Swan_Revert.SFX_Swan_Revert") }));
		Out.Add(Make(TEXT("Story.Waterfall.HoleOpen"), TEXT("瀑布水帘透开（还没接到玩法）"), C::Story, false, 400.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/45_Waterfall_Hole/SFX_Waterfall_Hole_Open.SFX_Waterfall_Hole_Open") }));
		Out.Add(Make(TEXT("Story.Waterfall.HoleClose"), TEXT("瀑布水帘合上（还没接到玩法）"), C::Story, false, 400.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/45_Waterfall_Hole/SFX_Waterfall_Hole_Close.SFX_Waterfall_Hole_Close") }));
		Out.Add(Make(TEXT("Story.IrisRelief.Awaken"), TEXT("伊莉丝浮雕醒来"), C::Story, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/46_Rainbow_Bridge_IrisRelief/SFX_IrisRelief_Awaken.SFX_IrisRelief_Awaken") }));
		Out.Add(Make(TEXT("Story.RainbowBridge.Appear"), TEXT("彩虹桥出现"), C::Story, false, 600.0f, 4500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/46_Rainbow_Bridge_IrisRelief/SFX_RainbowBridge_Appear.SFX_RainbowBridge_Appear") }));
		Out.Add(Make(TEXT("Story.Selene.Eyes"), TEXT("塞勒涅眼睛点亮"), C::Story, false, 500.0f, 4000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/48_Selene_Eyes/SFX_Selene_Eyes_Light.SFX_Selene_Eyes_Light") }));
		Out.Add(Make(TEXT("Story.Mirror.Awaken"), TEXT("三相像镜子醒来（镜面被阳光照到）"), C::Story, false, 400.0f, 3500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 30.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/50_Mirror_SunNiche/SFX_Mirror_Awaken.SFX_Mirror_Awaken") }));
		Out.Add(Make(TEXT("Story.Dodecahedron"), TEXT("正十二面体与星座亮起（集齐三片）"), C::Story, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/52_Dodecahedron_Stars/SFX_Dodecahedron_Stars.SFX_Dodecahedron_Stars") }));
		Out.Add(Make(TEXT("Story.RainbowGate"), TEXT("虹门彩虹"), C::Story, false, 300.0f, 2500.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 6.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/53_RainbowGate_Rainbow/SFX_RainbowGate_Rainbow.SFX_RainbowGate_Rainbow") }));
		Out.Add(Make(TEXT("Story.Selene.SleepTalk"), TEXT("塞勒涅梦话"), C::Story, false, 300.0f, 1200.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/56_Selene_SleepTalk/SFX_Selene_SleepTalk_01.SFX_Selene_SleepTalk_01"), TEXT("/Game/Dysis/Audio/SFX/56_Selene_SleepTalk/SFX_Selene_SleepTalk_02.SFX_Selene_SleepTalk_02") }));
		Out.Add(Make(TEXT("UI.Hover"), TEXT("按钮悬停"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.04f, 2, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/29_UI/SFX_UI_Hover_01.SFX_UI_Hover_01"), TEXT("/Game/Dysis/Audio/SFX/29_UI/SFX_UI_Hover_02.SFX_UI_Hover_02") }));
		Out.Add(Make(TEXT("UI.Confirm"), TEXT("确认"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 2, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/29_UI/SFX_UI_Confirm.SFX_UI_Confirm") }));
		Out.Add(Make(TEXT("UI.Back"), TEXT("返回"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 2, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/29_UI/SFX_UI_Back.SFX_UI_Back") }));
		Out.Add(Make(TEXT("UI.StartGame"), TEXT("开始游戏"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.0f, 1, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/29_UI/SFX_UI_StartGame.SFX_UI_StartGame") }));
		Out.Add(Make(TEXT("UI.Title"), TEXT("关卡标题（序、日1–5、日落之后、月1–5）"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, false, 0.0f, 1, false, 1.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Prologue.SFX_Title_Prologue"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Day1.SFX_Title_Day1"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Day2.SFX_Title_Day2"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Day3.SFX_Title_Day3"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Day4.SFX_Title_Day4"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Day5.SFX_Title_Day5"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Dusk.SFX_Title_Dusk"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Night1.SFX_Title_Night1"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Night2.SFX_Title_Night2"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Night3.SFX_Title_Night3"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Night4.SFX_Title_Night4"), TEXT("/Game/Dysis/Audio/SFX/35_Level_Title/SFX_Title_Night5.SFX_Title_Night5") }));
		Out.Add(Make(TEXT("UI.Prompt"), TEXT("互动提示出现"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 1.0f, 1, false, 0.3f,
			{ TEXT("/Game/Dysis/Audio/SFX/36_UI_Prompt/SFX_UI_Prompt_Appear_01.SFX_UI_Prompt_Appear_01"), TEXT("/Game/Dysis/Audio/SFX/36_UI_Prompt/SFX_UI_Prompt_Appear_02.SFX_UI_Prompt_Appear_02") }));
		Out.Add(Make(TEXT("UI.Text"), TEXT("提示文字出现"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 1.0f, 1, false, 0.3f,
			{ TEXT("/Game/Dysis/Audio/SFX/36_UI_Prompt/SFX_UI_Text_Appear_01.SFX_UI_Text_Appear_01"), TEXT("/Game/Dysis/Audio/SFX/36_UI_Prompt/SFX_UI_Text_Appear_02.SFX_UI_Text_Appear_02") }));
		Out.Add(Make(TEXT("UI.DialogueNext"), TEXT("对话推进（下一句）"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.1f, 1, false, 0.3f,
			{ TEXT("/Game/Dysis/Audio/SFX/37_UI_Dialogue_Advance/SFX_UI_Dialogue_Next_01.SFX_UI_Dialogue_Next_01"), TEXT("/Game/Dysis/Audio/SFX/37_UI_Dialogue_Advance/SFX_UI_Dialogue_Next_02.SFX_UI_Dialogue_Next_02"), TEXT("/Game/Dysis/Audio/SFX/37_UI_Dialogue_Advance/SFX_UI_Dialogue_Next_03.SFX_UI_Dialogue_Next_03") }));
		Out.Add(Make(TEXT("UI.ShardHover.Sun"), TEXT("界面·碰到日之碎片"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.3f, 1, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/57_UI_Shard_Hover/SFX_UI_Shard_Hover_Sun.SFX_UI_Shard_Hover_Sun") }));
		Out.Add(Make(TEXT("UI.ShardHover.Moon"), TEXT("界面·碰到月之碎片"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.3f, 1, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/57_UI_Shard_Hover/SFX_UI_Shard_Hover_Moon.SFX_UI_Shard_Hover_Moon") }));
		Out.Add(Make(TEXT("UI.ShardHover.Rainbow"), TEXT("界面·碰到虹之碎片"), C::UI, true, 300.0f, 3000.0f, 1.0f, 1.0f, 0.0f, 0.0f, true, 0.3f, 1, false, 0.0f,
			{ TEXT("/Game/Dysis/Audio/SFX/57_UI_Shard_Hover/SFX_UI_Shard_Hover_Rainbow.SFX_UI_Shard_Hover_Rainbow") }));
	}
}
