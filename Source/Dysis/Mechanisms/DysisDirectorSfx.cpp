// 机关总管 · 音效：每帧看一遍各个机关的状态，哪个变了就在它那里放对应的一次性音效。
// 以前这些音效是 ADysisSfxDirector 盯着旧的机关 Actor（拉杆、驱动器、神龛……）放的；旧 Actor 换成机关总管以后，
// 那些绑定都落空了，改在这里按机关总管自己的状态来放。循环的声音（踏步升降、推雕像的摩擦声、金苹果的嗡鸣）还没接。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "Audio/DysisSfxSubsystem.h"
#include "Sky/DysisTimeComponent.h"
#include "World/DysisWorldState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

bool ADysisDirector::SfxRose(FName Key, float Now, float Threshold)
{
	float* Was = SfxPrev.Find(Key);
	if (!Was) { SfxPrev.Add(Key, Now); return false; }   // 头一帧只记不放
	const bool bRose = *Was < Threshold && Now >= Threshold;
	*Was = Now;
	return bRose;
}

bool ADysisDirector::SfxFell(FName Key, float Now, float Threshold)
{
	float* Was = SfxPrev.Find(Key);
	if (!Was) { SfxPrev.Add(Key, Now); return false; }
	const bool bFell = *Was >= Threshold && Now < Threshold;
	*Was = Now;
	return bFell;
}

bool ADysisDirector::SfxChanged(FName Key, float Now)
{
	float* Was = SfxPrev.Find(Key);
	if (!Was) { SfxPrev.Add(Key, Now); return false; }
	const bool bChanged = *Was != Now;
	*Was = Now;
	return bChanged;
}

void ADysisDirector::Sfx(const TCHAR* Event, const FVector& At) const
{
	UDysisSfxLibrary::PlayDysisSfx(this, FName(Event), At);
}

FVector ADysisDirector::PieceLoc(FName Label, const FVector& Fallback) const
{
	const AActor* A = Piece(Label);
	return A ? A->GetActorLocation() : Fallback;
}

void ADysisDirector::UpdateSfx()
{
	const UDysisTimeComponent* Time = PlayerTime();
	const FVector Me = Time ? Time->FootCm + FVector(0.0, 0.0, 120.0) : GetActorLocation();   // 没有明确位置的就在人身边放
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	auto Stone = [this](FName Id) -> const FMoonstone* { for (const FMoonstone& Ms : Moonstones) if (Ms.Id == Id) return &Ms; return nullptr; };

	// ── 水闸、L1 的机关和两块石板 ──
	if (State && SfxRose(TEXT("sluice"), State->bSluiceOpen ? 1.0f : 0.0f, 0.5f))
	{
		const FVector At = SluiceMarker ? SluiceMarker->GetComponentLocation() : Me;
		Sfx(TEXT("Mech.Sluice.Wheel"), At);
		Sfx(TEXT("Mech.Waterfall.Start"), PieceLoc(TEXT("SM_Mech_Water_Waterfall"), At));
	}
	if (SfxChanged(TEXT("lever"), float(LeverPulls)))
	{
		Sfx(LeverPulls % 2 ? TEXT("Mech.Lever.Pull") : TEXT("Mech.Lever.Push"), PieceLoc(TEXT("SM_Mech_LeverA_Arm"), Me));
		Sfx(TEXT("Mech.Slab.Slide"), PieceLoc(TEXT("SM_Mech_Slider_Panel_b2"), Me));
		Sfx(TEXT("Mech.Slab.Slide"), PieceLoc(TEXT("SM_Mech_Slider_Panel_iris"), Me));
	}

	// ── 屋顶：桥门、接光 ──
	const FVector Roof = ArmTopCm();
	// （踏步升降、夜里的楼梯、光圈叶片的声音还是 ADysisSfxDirector 按模型的名字盯着放的，这里不重复）
	if (SfxRose(TEXT("doorOpen"), DoorOpen, 0.05f)) Sfx(TEXT("Mech.Gate.Open"), PieceLoc(TEXT("SM_Mech_RoofBridgeDoor_Leaf"), Me));
	if (SfxRose(TEXT("doorLock"), bDoorLocked ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Mech.Gate.Close"), PieceLoc(TEXT("SM_Mech_RoofBridgeDoor_Leaf"), Me));
	if (SfxRose(TEXT("caught"), bCaught ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.Apple.Take"), Roof);

	// ── 三相像、日之龛 ──
	const FVector Statue = PieceLoc(TEXT("SM_Mech_Mirror_Statue"), Me);
	if (SfxChanged(TEXT("mirrorSlot"), float(MirrorSlot))) Sfx(TEXT("Mech.Statue.Turn"), Statue);
	if (SfxChanged(TEXT("mirrorForm"), float(MirrorForm)) && MirrorForm != 0) Sfx(TEXT("Story.Mirror.Awaken"), Statue);
	const FVector SunNiche = PieceLoc(TEXT("SM_Mech_SunNiche_Box"), Me);
	if (SfxRose(TEXT("sunNiche"), bSunNicheOpen ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Mech.SunNiche.Open"), SunNiche);
	if (SfxRose(TEXT("sunShard"), SunNicheT, 2.6f)) Sfx(TEXT("Story.Shard.Sun"), SunNiche);

	// ── 虹：浮雕、虹桥、石沿和虹之龛、棱镜、塞勒涅 ──
	if (SfxRose(TEXT("relief"), bReliefDone ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.IrisRelief.Awaken"), PieceLoc(TEXT("SM_Mech_IrisRelief_Relief"), Me));
	if (SfxRose(TEXT("rainbow"), RainbowT, 0.6f)) Sfx(TEXT("Story.RainbowBridge.Appear"), Me);
	const FVector Sill = PieceLoc(TEXT("SM_Mech_Sill_Ledge"), Me);
	if (SfxRose(TEXT("sill"), SillK, 0.01f)) Sfx(TEXT("Mech.Sill.Extend"), Sill);
	if (SfxRose(TEXT("irisNiche"), bIrisNicheOpen ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Mech.Niche.DoorsOpen"), PieceLoc(TEXT("SM_Mech_Sill_Niche"), Sill));
	if (SfxRose(TEXT("rainbowShard"), IrisNicheT, 2.6f)) Sfx(TEXT("Story.Shard.Rainbow"), Sill);
	if (SfxRose(TEXT("prismRise"), PrismExt, 0.01f)) Sfx(TEXT("Mech.Prism.ColumnRise"), Sill);
	if (SfxChanged(TEXT("prismSlot"), float(PrismSlot))) Sfx(TEXT("Mech.Prism.Turn"), Sill);
	const FVector Selene = PieceLoc(TEXT("SM_Mech_Selene_Relief"), Me);
	if (SfxRose(TEXT("seleneEye"), SeleneColor == 5 ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.Selene.Eyes"), Selene);
	if (SfxRose(TEXT("selene"), bSeleneOn ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.Selene.Awaken"), Selene);

	// ── 天鹅、墙里的楼梯、月石 ──
	const FVector Swan = PieceLoc(TEXT("SM_Mech_Swan_Plinth"), Me);
	if (SfxChanged(TEXT("swanSlot"), float(SwanSlot))) Sfx(TEXT("Mech.Statue.Turn"), Swan);
	if (SfxRose(TEXT("swanForm"), SwanForm, 0.5f)) Sfx(TEXT("Story.Swan.Transform"), Swan);
	if (SfxFell(TEXT("swanBack"), SwanForm, 0.5f)) Sfx(TEXT("Story.Swan.Revert"), Swan);
	if (SfxRose(TEXT("swanSolved"), bSwanSolved ? 1.0f : 0.0f, 0.5f))
	{
		Sfx(TEXT("Mech.SwanRelief.Sink"), PieceLoc(TEXT("SM_Mech_Swan_Relief"), Swan));
		Sfx(TEXT("Mech.WallStairs.RevealTS"), PieceLoc(TEXT("SM_Mech_Swan_Door"), Swan));
	}
	for (int32 s = 0; s < 2; ++s)
		for (int32 i = 0; i < StairSets[s].Windows.Num(); ++i)
			if (SfxRose(FName(*FString::Printf(TEXT("stairWin%d_%d"), s, i)), StairSets[s].Windows[i].Open, 0.01f))
				Sfx(TEXT("Mech.WallStairs.Window"), StairSets[s].Windows[i].Base);
	if (const FMoonstone* Rel = Stone(TEXT("moonRelief")))
		if (SfxRose(TEXT("moonRelief"), Rel->bPerm ? 1.0f : 0.0f, 0.5f))
		{
			const FVector At = PieceLoc(TEXT("SM_Mech_MoonRelief_Block"), Me);
			Sfx(TEXT("Moon.Wall.Vanish"), At);
			Sfx(TEXT("Mech.WallStairs.RevealTR"), At);
		}
	if (const FMoonstone* Wall = Stone(TEXT("twinWall")))
	{
		const FVector At = PieceLoc(TEXT("SM_Mech_Twins_WallBlock"), Me);
		if (SfxRose(TEXT("twinWallOpen"), Wall->K, 0.5f)) Sfx(TEXT("Moon.Wall.Vanish"), At);
		if (SfxFell(TEXT("twinWallShut"), Wall->K, 0.5f)) Sfx(TEXT("Moon.Stone.Appear"), At);
	}
	if (const FMoonstone* Shrine = Stone(TEXT("moonShrine")))
		if (SfxRose(TEXT("moonShrine"), Shrine->bPerm ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Moon.Wall.Vanish"), PieceLoc(TEXT("SM_Mech_MoonShrine_Block"), Me));
	if (SfxRose(TEXT("moonShard"), bMoonShard ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.Shard.Moon"), PieceLoc(TEXT("SM_Mech_MoonShrine_Niche"), Me));

	// ── 双子、月桥 ──
	const FVector PolluxAt = PieceLoc(TEXT("SM_Mech_Twins_Pollux"), Me);
	if (SfxRose(TEXT("pullPollux"), TwinState >= 2 ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Mech.Statue.PullOut"), PolluxAt);
	{
		// 推着走：开始动、停下来各一声
		const float Still = SfxPrev.FindOrAdd(TEXT("polluxStill"), 1.0f);   // 已经多久没动了（秒，最多记到 1）
		const bool bMoved = SfxChanged(TEXT("polluxAz"), PolluxAz) && TwinState == 3;
		const float StillNow = bMoved ? 0.0f : FMath::Min(Still + GetWorld()->GetDeltaSeconds(), 1.0f);
		if (bMoved && Still >= 0.3f) Sfx(TEXT("Mech.Statue.PushStart"), PolluxAt);
		if (!bMoved && Still < 0.3f && StillNow >= 0.3f) Sfx(TEXT("Mech.Statue.PushStop"), PolluxAt);
		SfxPrev.FindOrAdd(TEXT("polluxStill")) = StillNow;
	}
	if (SfxRose(TEXT("twins"), bTwinsJoined ? 1.0f : 0.0f, 0.5f))
	{
		Sfx(TEXT("Story.Twins.Light"), PolluxAt);
		Sfx(TEXT("Mech.MoonBridge.Extend"), PolluxAt);
	}

	// ── 女神、半桥、影桥、放苹果 ──
	const FVector Goddess = PieceLoc(TEXT("SM_Mech_Goddess_Statue"), Me);
	if (SfxRose(TEXT("glint"), bGoddessGlint ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.Waterfall.HoleOpen"), Goddess);
	if (SfxRose(TEXT("goddess"), bGoddessLit ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Mech.HalfBridge.Extend"), PieceLoc(TEXT("SM_Mech_HalfBridge_Deck"), Goddess));
	if (SfxRose(TEXT("shadow"), ShadowOn, 0.99f)) Sfx(TEXT("Story.ShadowBridge.Join"), Me);
	if (SfxRose(TEXT("placed"), bApplePlaced ? 1.0f : 0.0f, 0.5f)) Sfx(TEXT("Story.Apple.Place"), FVector(0.0, 0.0, 180.0));
}
