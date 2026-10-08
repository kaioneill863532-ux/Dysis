// 狄西斯的日落回廊 · 音效总调度
#include "Audio/DysisSfxDirector.h"

#include "Audio/DysisFootstepComponent.h"
#include "Audio/DysisSfxSubsystem.h"
#include "Beams/DysisBeamActor.h"
#include "Interaction/DysisGoldenApple.h"
#include "Interaction/DysisInteractComponent.h"
#include "Mechanisms/DysisCatchLight.h"
#include "Mechanisms/DysisInteractable.h"
#include "Mechanisms/DysisLeverActor.h"
#include "Mechanisms/DysisMechDriver.h"
#include "Mechanisms/DysisMirrorSource.h"
#include "Mechanisms/DysisNicheActor.h"
#include "Mechanisms/DysisPrismActor.h"
#include "Mechanisms/DysisRainbowAlign.h"
#include "Mechanisms/DysisSliderActor.h"
#include "Save/DysisSaveSubsystem.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisTimeComponent.h"
#include "Surfaces/DysisLuminousStone.h"
#include "Surfaces/DysisMoonSurface.h"
#include "UI/DysisDialogueComponent.h"
#include "UI/DysisHUD.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

namespace DysisSfxDirectorPrivate
{
	// 位置锚点：名字 → 大纲名里包含的文字（机关清单里的网格名）。
	struct FAnchorDef { const TCHAR* Name; const TCHAR* Pattern; };
	static const FAnchorDef AnchorDefs[] = {
		{ TEXT("Waterfall"),     TEXT("SM_Mech_Water_Waterfall") },
		{ TEXT("Pool"),          TEXT("SM_Mech_Water_Pool") },
		{ TEXT("Sea"),           TEXT("SM_Mech_Water_Sea") },
		{ TEXT("BridgeDoor"),    TEXT("RoofBridgeDoor_Leaf") },
		{ TEXT("SeleneRelief"),  TEXT("SM_Mech_Selene_Relief") },
		{ TEXT("Goddess"),       TEXT("SM_Mech_Goddess_Statue") },
		{ TEXT("SwanGoddess"),   TEXT("SM_Mech_Swan_Goddess") },
		{ TEXT("IrisRelief"),    TEXT("SM_Mech_IrisRelief_Relief") },
		{ TEXT("RainbowBridge"), TEXT("SM_Mech_RainbowBridge_Light") },
		{ TEXT("RainbowArch"),   TEXT("SM_Mech_RainbowArch_Arch") },
		{ TEXT("Castor"),        TEXT("SM_Mech_Twins_Castor") },
		{ TEXT("Mirror"),        TEXT("SM_Mech_Mirror_Statue") },
		{ TEXT("Prism"),         TEXT("SM_Mech_Prism_Glass") },
		{ TEXT("ArmillaryTop"),  TEXT("SM_Mech_Armillary_Top") },
		{ TEXT("Pavilion"),      TEXT("SM_Mech_Armillary_Pavilion") },
	};

	// 机关音效：驱动的网格名里包含 Pattern → 这几条绑定。SoundAt 为锚点名（空 = 机关网格自己）。
	struct FMechDef { const TCHAR* Pattern; EDysisMechSfxWhen When; const TCHAR* Key; float Delay; const TCHAR* SoundAt; float Volume; float Pitch; };
	static const FMechDef MechDefs[] = {
		// 月2 天鹅：转动女神像（快一点）→ 变天鹅、浮雕下沉、石门沉、墙里楼梯 TS 显现
		{ TEXT("Swan_Goddess"),     EDysisMechSfxWhen::Activate, TEXT("Mech.Statue.Turn"),         0.0f, nullptr,              1.0f, 1.08f },
		{ TEXT("Swan_Relief"),      EDysisMechSfxWhen::Activate, TEXT("Story.Swan.Transform"),     0.0f, TEXT("SwanGoddess"),  1.0f, 1.0f },
		{ TEXT("Swan_Relief"),      EDysisMechSfxWhen::Activate, TEXT("Mech.SwanRelief.Sink"),     1.2f, nullptr,              1.0f, 1.0f },
		{ TEXT("Swan_Door"),        EDysisMechSfxWhen::Activate, TEXT("Mech.Slab.Slide"),          0.4f, nullptr,              1.0f, 0.9f },
		{ TEXT("StairSeal_TS"),     EDysisMechSfxWhen::Activate, TEXT("Mech.WallStairs.RevealTS"), 0.8f, nullptr,              1.0f, 1.0f },
		// 月3 月亮浮雕隐去 → TR 楼梯显现；月之龛月石透开；月4 双子墙块
		{ TEXT("StairSeal_TR"),     EDysisMechSfxWhen::Activate, TEXT("Mech.WallStairs.RevealTR"), 0.3f, nullptr,              1.0f, 1.0f },
		{ TEXT("MoonRelief_Block"), EDysisMechSfxWhen::Activate, TEXT("Moon.Wall.Vanish"),         0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("MoonShrine_Block"), EDysisMechSfxWhen::Activate, TEXT("Moon.Wall.Vanish"),         0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("Twins_WallBlock"),  EDysisMechSfxWhen::Activate, TEXT("Moon.Wall.Vanish"),         0.0f, nullptr,              1.0f, 1.0f },
		// 月4 双子：推波吕丢刻斯回卡斯托耳身边（起步/推动中/停下）→ 双子亮起 → 月桥伸出
		{ TEXT("Twins_Pollux"),     EDysisMechSfxWhen::Activate, TEXT("Mech.Statue.PushStart"),    0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("Twins_Pollux"),     EDysisMechSfxWhen::Moving,   TEXT("Mech.Statue.PushLoop"),     0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("Twins_Pollux"),     EDysisMechSfxWhen::Arrive,   TEXT("Mech.Statue.PushStop"),     0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("Twins_Chain"),      EDysisMechSfxWhen::Activate, TEXT("Story.Twins.Light"),        0.0f, TEXT("Castor"),       1.0f, 1.0f },
		{ TEXT("MoonBridge_Deck"),  EDysisMechSfxWhen::Activate, TEXT("Mech.MoonBridge.Extend"),   1.4f, nullptr,              1.0f, 1.0f },
		// 月5 半桥：月光对准（Lock）→ 塞勒涅亮起 → 半桥伸出
		{ TEXT("HalfBridge_Deck"),  EDysisMechSfxWhen::Activate, TEXT("Story.Selene.Lock"),        0.0f, TEXT("Goddess"),      1.0f, 1.0f },
		{ TEXT("HalfBridge_Deck"),  EDysisMechSfxWhen::Activate, TEXT("Story.Selene.Awaken"),      1.0f, TEXT("Goddess"),      1.0f, 1.0f },
		{ TEXT("HalfBridge_Deck"),  EDysisMechSfxWhen::Activate, TEXT("Mech.HalfBridge.Extend"),   2.4f, nullptr,              1.0f, 1.0f },
		// 日3 日之龛开盖；虹线：窗下石沿、棱镜铜柱
		{ TEXT("SunNiche_Lid"),     EDysisMechSfxWhen::Activate, TEXT("Mech.SunNiche.Open"),       0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("Sill_Ledge"),       EDysisMechSfxWhen::Activate, TEXT("Mech.Sill.Extend"),         0.0f, nullptr,              1.0f, 1.0f },
		{ TEXT("Prism_Column"),     EDysisMechSfxWhen::Activate, TEXT("Mech.Prism.ColumnRise"),    0.0f, nullptr,              1.0f, 1.0f },
		// 日2 推拉石板（拉杆一拉，两块石板开始滑）
		{ TEXT("Slider_Panel"),     EDysisMechSfxWhen::Activate, TEXT("Mech.Slab.Slide"),          0.0f, nullptr,              1.0f, 1.0f },
	};

	static FString ActorNames(const AActor* A)
	{
		if (!A) return FString();
		FString N = A->GetName();
#if WITH_EDITOR
		N += TEXT("|") + A->GetActorLabel();
#endif
		for (const FName& Tag : A->Tags) N += TEXT("|") + Tag.ToString();
		return N;
	}

	static bool NameHas(const AActor* A, const TCHAR* Pattern)
	{
		return A && ActorNames(A).Contains(Pattern, ESearchCase::IgnoreCase);
	}

	static AActor* MeshOf(AActor* Mech)
	{
		if (const ADysisMechDriver* D = Cast<ADysisMechDriver>(Mech)) return D->TargetMesh ? D->TargetMesh.Get() : Mech;
		if (const ADysisSliderActor* S = Cast<ADysisSliderActor>(Mech)) return S->TargetMesh ? S->TargetMesh.Get() : Mech;
		return Mech;
	}

	static float SmoothStep01(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / FMath::Max(B - A, 1.0f), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** 低通：0 → 不滤（20 kHz），1 → Hz（按对数插）。 */
	static float LowPassFor(float Amount, float Hz)
	{
		return 20000.0f * FMath::Pow(Hz / 20000.0f, FMath::Clamp(Amount, 0.0f, 1.0f));
	}

	static const FName LoopSeaFar(TEXT("Amb.SeaFar"));
	static const FName LoopSeaClose(TEXT("Amb.SeaClose"));
	static const FName LoopWind(TEXT("Amb.Wind"));
	static const FName LoopRoom(TEXT("Amb.Room"));
	static const FName LoopBirds(TEXT("Amb.Birds"));
	static const FName LoopPool(TEXT("Amb.Pool"));
	static const FName LoopMist(TEXT("Amb.Mist"));
	static const FName LoopFallNear(TEXT("Amb.WaterfallNear"));
	static const FName LoopFallFar(TEXT("Amb.WaterfallFar"));
	static const FName LoopDroneLow(TEXT("Story.DroneLow"));
	static const FName LoopDroneHigh(TEXT("Story.DroneHigh"));
	static const FName LoopApple(TEXT("Story.AppleHold"));
	static const FName LoopSteps(TEXT("Mech.StepsMove"));
	static const FName LoopIris(TEXT("Mech.IrisMove"));
}

namespace DSD = DysisSfxDirectorPrivate;

ADysisSfxDirector::ADysisSfxDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.0f / 30.0f;   // 30 Hz 看状态足够（脚步在玩家身上的组件里，每帧）
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ADysisSfxDirector* ADysisSfxDirector::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World) return nullptr;
	TActorIterator<ADysisSfxDirector> It(World);   // Mac clang：迭代器判一次
	return It ? *It : nullptr;
}

// ───────────────────────── 自动绑定 ─────────────────────────

void ADysisSfxDirector::AutoBind()
{
	UWorld* World = GetWorld();
	if (!World) return;
#if WITH_EDITOR
	Modify();
#endif

	// 锚点（只认网格 Actor）。
	Anchors.Reset();
	for (const DSD::FAnchorDef& Def : DSD::AnchorDefs)
	{
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (DSD::NameHas(*It, Def.Pattern))
			{
				Anchors.Add(FName(Def.Name), *It);
				break;
			}
		}
	}

	// 机关：每个驱动器/石板看它驱动的网格叫什么。
	MechBindings.Reset();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Mech = *It;
		if (!Cast<ADysisMechDriver>(Mech) && !Cast<ADysisSliderActor>(Mech)) continue;
		const AActor* Mesh = DSD::MeshOf(Mech);
		for (const DSD::FMechDef& Def : DSD::MechDefs)
		{
			if (!DSD::NameHas(Mesh, Def.Pattern) && !DSD::NameHas(Mech, Def.Pattern)) continue;
			FDysisMechSfxBinding B;
			B.Mech = Mech;
			B.When = Def.When;
			B.Key = FName(Def.Key);
			B.Delay = Def.Delay;
			B.VolumeScale = Def.Volume;
			B.PitchScale = Def.Pitch;
			if (Def.SoundAt)
			{
				const TObjectPtr<AActor>* At = Anchors.Find(FName(Def.SoundAt));
				B.SoundAt = At ? At->Get() : nullptr;
			}
			MechBindings.Add(B);
		}
	}

	// 屋顶踏步（踏面，不要柱身和铜沿），按名字里的 01–16 排好。
	auto CollectSteps = [World](const TCHAR* Family, TArray<TObjectPtr<AActor>>& Out)
	{
		TArray<AActor*> Found;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (DSD::NameHas(*It, Family) && !DSD::NameHas(*It, TEXT("_Shaft")) && !DSD::NameHas(*It, TEXT("_Curb"))) Found.Add(*It);
		}
		Found.Sort([](const AActor& A, const AActor& B) { return DSD::ActorNames(&A) < DSD::ActorNames(&B); });
		Out.Reset();
		for (AActor* A : Found) Out.Add(A);
	};
	CollectSteps(TEXT("RoofSteps_Up"), RoofUpSteps);
	CollectSteps(TEXT("RoofSteps_Dn"), RoofDnSteps);

	IrisBlades.Reset();
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (DSD::NameHas(*It, TEXT("IrisBlades_"))) IrisBlades.Add(*It);
	}

	UE_LOG(LogTemp, Display, TEXT("%s"), *DescribeBindings());
}

FString ADysisSfxDirector::DescribeBindings() const
{
	FString Out = FString::Printf(TEXT("DysisSfxDirector 绑定：锚点 %d/%d，机关音效 %d 条，屋顶上行 %d 级、下行 %d 级，光圈叶片 %d 片"),
		Anchors.Num(), static_cast<int32>(UE_ARRAY_COUNT(DSD::AnchorDefs)), MechBindings.Num(), RoofUpSteps.Num(), RoofDnSteps.Num(), IrisBlades.Num());
	for (const DSD::FAnchorDef& Def : DSD::AnchorDefs)
	{
		const TObjectPtr<AActor>* A = Anchors.Find(FName(Def.Name));
		if (!A || !*A) Out += FString::Printf(TEXT("\n  缺锚点 %s（大纲里找不到含 %s 的网格；这一处的声音会放在别处）"), Def.Name, Def.Pattern);
	}
	TSet<FString> Bound;
	for (const FDysisMechSfxBinding& B : MechBindings)
	{
		Bound.Add(B.Key.ToString());
	}
	for (const DSD::FMechDef& Def : DSD::MechDefs)
	{
		if (!Bound.Contains(Def.Key)) Out += FString::Printf(TEXT("\n  没绑上 %s（找不到驱动 %s 的机关）"), Def.Key, Def.Pattern);
	}
	return Out;
}

// ───────────────────────── 开局 ─────────────────────────

void ADysisSfxDirector::BeginPlay()
{
	Super::BeginPlay();
	Sfx = UDysisSfxSubsystem::Get(this);
	StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// 关卡里没存绑定（没跑导入脚本），或者绑定指向的机关/网格已经不在了（重跑过 ue_import_gameplay.py /
	// ue_import_temple.py，它们会删掉重摆）：现场按名字重新找一遍。
	bool bStale = MechBindings.Num() == 0 && Anchors.Num() == 0;
	for (const FDysisMechSfxBinding& B : MechBindings) bStale |= (B.Mech == nullptr);
	for (const TPair<FName, TObjectPtr<AActor>>& A : Anchors) bStale |= (A.Value == nullptr);
	if (bStale)
	{
		UE_LOG(LogTemp, Display, TEXT("DysisSfxDirector: 绑定是空的或过期了，按名字重新绑定（建议重跑 Art/Audio/ue_import_sfx.py 存进关卡）"));
		AutoBind();
	}
	GatherWorld();
	EnsureFootsteps();
	NotifyHandle = ADysisHUD::OnNotificationShown.AddUObject(this, &ADysisSfxDirector::HandleNotification);
}

void ADysisSfxDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ADysisHUD::OnNotificationShown.Remove(NotifyHandle);
	if (ADysisCatchLight* C = CatchLight.Get()) C->OnCaught.RemoveDynamic(this, &ADysisSfxDirector::HandleLightCaught);
	if (ADysisPrismActor* P = Prism.Get()) P->OnIndigoOnTarget.RemoveDynamic(this, &ADysisSfxDirector::HandleIndigo);
	if (ADysisRainbowAlign* R = RainbowAlign.Get()) R->OnAligned.RemoveDynamic(this, &ADysisSfxDirector::HandleRainbowAligned);
	for (const TWeakObjectPtr<ADysisNicheActor>& N : Niches)
	{
		if (ADysisNicheActor* Niche = N.Get()) Niche->OnCollected.RemoveDynamic(this, &ADysisSfxDirector::HandleNicheCollected);
	}
	Super::EndPlay(EndPlayReason);
}

void ADysisSfxDirector::GatherWorld()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 机关：每个不同的机关一条观察记录。
	MechWatches.Reset();
	for (const FDysisMechSfxBinding& B : MechBindings)
	{
		if (!B.Mech) continue;
		const bool bHave = MechWatches.ContainsByPredicate([&B](const FMechWatch& W) { return W.Mech.Get() == B.Mech; });
		if (bHave) continue;
		FMechWatch W;
		W.Mech = B.Mech.Get();
		W.Mesh = DSD::MeshOf(B.Mech.Get());
		W.LoopId = FName(*FString::Printf(TEXT("Mech.%s"), *B.Mech->GetName()));
		if (const ADysisMechDriver* D = Cast<ADysisMechDriver>(B.Mech.Get())) W.bWasActivated = D->bActivated;
		if (AActor* Mesh = W.Mesh.Get()) W.LastXf = Mesh->GetActorTransform();
		MechWatches.Add(W);
	}

	LeverWatches.Reset();
	BeamWatches.Reset();
	StoneWatches.Reset();
	Niches.Reset();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (ADysisLeverActor* L = Cast<ADysisLeverActor>(A))
		{
			FLeverWatch W; W.Lever = L; W.bPulled = L->IsPulled(); LeverWatches.Add(W);
		}
		else if (ADysisBeamActor* B = Cast<ADysisBeamActor>(A))
		{
			if (B->BeamZone.ToString().StartsWith(TEXT("prism:"))) continue;   // 棱镜的七道彩光不能踩，不出声
			FBeamWatch W; W.Beam = B; W.bWalkable = B->IsWalkableNow(); W.ChangedAt = StartTime; BeamWatches.Add(W);
		}
		else if (ADysisLuminousStone* S = Cast<ADysisLuminousStone>(A))
		{
			FStoneWatch W; W.Stone = S; W.bLit = S->GetGlow() >= 0.95f; StoneWatches.Add(W);
		}
		else if (ADysisNicheActor* N = Cast<ADysisNicheActor>(A))
		{
			Niches.Add(N);
			N->OnCollected.AddUniqueDynamic(this, &ADysisSfxDirector::HandleNicheCollected);
		}
		else if (ADysisMirrorSource* M = Cast<ADysisMirrorSource>(A))
		{
			Mirror = M; MirrorSlot = M->GetCurrentSlot(); bMirrorLit = M->CanMirrorBeamWalk();
		}
		else if (ADysisPrismActor* P = Cast<ADysisPrismActor>(A))
		{
			Prism = P; PrismSlot = P->GetCurrentSlot();
			P->OnIndigoOnTarget.AddUniqueDynamic(this, &ADysisSfxDirector::HandleIndigo);
		}
		else if (ADysisCatchLight* C = Cast<ADysisCatchLight>(A))
		{
			CatchLight = C;
			C->OnCaught.AddUniqueDynamic(this, &ADysisSfxDirector::HandleLightCaught);
		}
		else if (ADysisRainbowAlign* R = Cast<ADysisRainbowAlign>(A))
		{
			RainbowAlign = R;
			R->OnAligned.AddUniqueDynamic(this, &ADysisSfxDirector::HandleRainbowAligned);
		}
		else if (ADysisGoldenApple* G = Cast<ADysisGoldenApple>(A))
		{
			Apple = G; AppleState = static_cast<uint8>(G->State);
		}
		else if (ADysisMoonSurface* W = Cast<ADysisMoonSurface>(A))
		{
			Water = W;
		}
	}

	// 屋顶踏步。
	auto Watch = [](const TArray<TObjectPtr<AActor>>& In, TArray<FStepWatch>& Out)
	{
		Out.Reset();
		for (const TObjectPtr<AActor>& A : In)
		{
			FStepWatch W;
			W.Step = A.Get();
			W.LastZ = A ? static_cast<float>(A->GetActorLocation().Z) : 0.0f;
			Out.Add(W);
		}
	};
	Watch(RoofUpSteps, UpWatches);
	Watch(RoofDnSteps, DnWatches);
	Watch(IrisBlades, IrisWatches);
	RiseFired.Init(false, UpWatches.Num());

	if (const AActor* Door = FindAnchor(TEXT("BridgeDoor")))
	{
		DoorRot = Door->GetActorRotation();
		bDoorKnown = true;
	}

	UE_LOG(LogTemp, Display, TEXT("DysisSfxDirector: 机关 %d、拉杆 %d、光柱 %d、夜光石 %d、龛 %d、屋顶踏步 %d+%d"),
		MechWatches.Num(), LeverWatches.Num(), BeamWatches.Num(), StoneWatches.Num(), Niches.Num(), UpWatches.Num(), DnWatches.Num());
}

APawn* ADysisSfxDirector::GetPlayerPawn() const
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

void ADysisSfxDirector::EnsureFootsteps()
{
	if (!bFootsteps || Footsteps.IsValid()) return;
	ACharacter* Char = Cast<ACharacter>(GetPlayerPawn());
	if (!Char) return;
	UDysisFootstepComponent* Comp = Char->FindComponentByClass<UDysisFootstepComponent>();
	if (!Comp)
	{
		Comp = NewObject<UDysisFootstepComponent>(Char, TEXT("DysisFootsteps"));
		Comp->RegisterComponent();
		Char->AddInstanceComponent(Comp);
	}
	Footsteps = Comp;
}

AActor* ADysisSfxDirector::FindAnchor(FName Anchor) const
{
	const TObjectPtr<AActor>* A = Anchors.Find(Anchor);
	return A ? A->Get() : nullptr;
}

FVector ADysisSfxDirector::AnchorLocation(FName Anchor, const FVector& Fallback) const
{
	const AActor* A = FindAnchor(Anchor);
	return A ? A->GetActorLocation() : Fallback;
}

FVector ADysisSfxDirector::PlayerFoot() const
{
	const APawn* Pawn = GetPlayerPawn();
	if (!Pawn) return FVector::ZeroVector;
	if (const UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>())
		if (!Time->FootCm.IsNearlyZero()) return Time->FootCm;
	const ACharacter* Char = Cast<ACharacter>(Pawn);
	const float Half = (Char && Char->GetCapsuleComponent()) ? Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	return Pawn->GetActorLocation() - FVector(0, 0, Half);
}

void ADysisSfxDirector::EnsureLoop(FName LoopId, FName Key, const FVector& Location, float Scale, float BlendSeconds, float FadeInSeconds)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	if (!S) return;
	if (Scale > 0.001f)
	{
		if (!S->IsLoopActive(LoopId)) S->StartLoop(LoopId, Key, Location, FadeInSeconds, Scale);
		else S->SetLoopScale(LoopId, Scale, BlendSeconds);
	}
	else if (S->IsLoopActive(LoopId))
	{
		S->SetLoopScale(LoopId, 0.0f, BlendSeconds);
	}
}

// ───────────────────────── 每帧（30 Hz）─────────────────────────

void ADysisSfxDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Sfx.IsValid()) Sfx = UDysisSfxSubsystem::Get(this);
	if (!Sfx.IsValid()) return;
	EnsureFootsteps();

	// 开局 1 秒：各机关在自己的 BeginPlay 里摆初始状态（三相像转到开局格、踏步归零……），只记下来，不出声。
	if (GetWorld()->GetTimeSeconds() - StartTime < 1.0)
	{
		SyncAll();
		UpdateIndoor(DeltaTime, true);
		if (bAmbience) UpdateAmbience(DeltaTime);
		return;
	}
	UpdateIndoor(DeltaTime, false);

	if (bMechanisms)
	{
		UpdateMechs(DeltaTime);
		UpdateLevers();
		UpdateBeams();
		UpdateStatues();
		UpdateApple();
		UpdateStones();
		UpdateRoofSteps(DeltaTime);
		UpdateIris(DeltaTime);
		UpdateGate();
		UpdateProximity(DeltaTime);
	}
	UpdateWater();
	if (bAmbience) UpdateAmbience(DeltaTime);
	if (bTitles) UpdateTitles();
	if (bUIHooks) UpdateUI();
}

void ADysisSfxDirector::SyncAll()
{
	for (FMechWatch& W : MechWatches)
	{
		AActor* Mech = W.Mech.Get();
		if (!Mech) continue;
		if (const ADysisMechDriver* D = Cast<ADysisMechDriver>(Mech)) W.bWasActivated = D->bActivated;
		if (const AActor* Mesh = W.Mesh.IsValid() ? W.Mesh.Get() : Mech) W.LastXf = Mesh->GetActorTransform();
		W.bMoving = false;
	}
	for (FLeverWatch& W : LeverWatches)
	{
		if (W.Lever.IsValid()) W.bPulled = W.Lever->IsPulled();
	}
	for (FBeamWatch& W : BeamWatches)
	{
		if (W.Beam.IsValid()) { W.bWalkable = W.Beam->IsWalkableNow(); W.bPendingReveal = false; }
	}
	for (FStoneWatch& W : StoneWatches)
	{
		if (W.Stone.IsValid()) W.bLit = W.Stone->GetGlow() >= 0.95f;
	}
	if (Mirror.IsValid()) { MirrorSlot = Mirror->GetCurrentSlot(); bMirrorLit = Mirror->CanMirrorBeamWalk(); }
	if (Prism.IsValid()) PrismSlot = Prism->GetCurrentSlot();
	if (Apple.IsValid()) AppleState = static_cast<uint8>(Apple->State);
	bWaterKnown = false;   // 预热结束后第一次 UpdateWater 按当时的水态初始化（开着闸就直接接瀑布循环）
	auto SyncSteps = [](TArray<FStepWatch>& Steps)
	{
		for (FStepWatch& W : Steps)
		{
			if (W.Step.IsValid()) { W.LastZ = static_cast<float>(W.Step->GetActorLocation().Z); W.bMoving = false; W.StillSeconds = 0.0f; }
		}
	};
	SyncSteps(UpWatches);
	SyncSteps(DnWatches);
	SyncSteps(IrisWatches);
	if (const AActor* Door = FindAnchor(TEXT("BridgeDoor"))) { DoorRot = Door->GetActorRotation(); bDoorKnown = true; }
}

void ADysisSfxDirector::FireBinding(const FDysisMechSfxBinding& B, const FMechWatch& W)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	if (!S || B.Key.IsNone()) return;
	const AActor* At = B.SoundAt ? B.SoundAt.Get() : (W.Mesh.IsValid() ? W.Mesh.Get() : W.Mech.Get());
	const FVector Loc = At ? At->GetActorLocation() : GetActorLocation();
	S->PlayDelayed(B.Delay, B.Key, Loc, -1, B.VolumeScale, B.PitchScale);
	if (B.Key == FName(TEXT("Story.Selene.Lock")))
	{
		// 月光对准了：怀里月亮的两条持续音淡出，此后不再响。
		bSeleneLocked = true;
		S->StopLoop(DSD::LoopDroneLow, 1.0f);
		S->StopLoop(DSD::LoopDroneHigh, 1.0f);
	}
}

void ADysisSfxDirector::UpdateMechs(float Dt)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWarm = Now - StartTime > 1.0;   // 开局第一秒各机关在摆初始姿态，不出声

	for (FMechWatch& W : MechWatches)
	{
		AActor* Mech = W.Mech.Get();
		AActor* Mesh = W.Mesh.IsValid() ? W.Mesh.Get() : Mech;
		if (!Mech || !Mesh) continue;

		const FTransform Xf = Mesh->GetActorTransform();
		const bool bMovedNow = !Xf.GetLocation().Equals(W.LastXf.GetLocation(), 0.05)
		                    || !Xf.GetRotation().Equals(W.LastXf.GetRotation(), 1.0e-4);
		W.LastXf = Xf;

		auto FireWhen = [this, &W, Mech](EDysisMechSfxWhen When)
		{
			for (const FDysisMechSfxBinding& B : MechBindings)
			{
				if (B.Mech == Mech && B.When == When) FireBinding(B, W);
			}
		};

		// 激活：驱动器看 bActivated；石板这类没有激活标志的看“开始动”。
		if (const ADysisMechDriver* D = Cast<ADysisMechDriver>(Mech))
		{
			if (D->bActivated && !W.bWasActivated && bWarm) FireWhen(EDysisMechSfxWhen::Activate);
			W.bWasActivated = D->bActivated;
		}
		else if (bMovedNow && !W.bMoving && bWarm)
		{
			FireWhen(EDysisMechSfxWhen::Activate);
		}

		// 动的时候 / 停下。
		if (bMovedNow)
		{
			if (!W.bMoving && bWarm)
			{
				for (const FDysisMechSfxBinding& B : MechBindings)
				{
					if (B.Mech == Mech && B.When == EDysisMechSfxWhen::Moving && S) S->StartLoop(W.LoopId, B.Key, Xf.GetLocation(), 0.2f, B.VolumeScale);
				}
			}
			W.bMoving = true;
			W.bEverMoved = true;
			W.StillSeconds = 0.0f;
			if (S) S->SetLoopLocation(W.LoopId, Xf.GetLocation());
		}
		else if (W.bMoving)
		{
			W.StillSeconds += Dt;
			if (W.StillSeconds > 0.15f)
			{
				W.bMoving = false;
				if (S) S->StopLoop(W.LoopId, 0.25f);
				if (bWarm) FireWhen(EDysisMechSfxWhen::Arrive);
			}
		}
	}
}

void ADysisSfxDirector::UpdateLevers()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	for (FLeverWatch& W : LeverWatches)
	{
		ADysisLeverActor* L = W.Lever.Get();
		if (!L || L->IsPulled() == W.bPulled) continue;
		W.bPulled = L->IsPulled();
		const FVector Loc = L->GetActorLocation();
		if (!L->SluiceName.IsNone())
		{
			// 水闸：闸轮转 2.6 s，转到 2.2 s 闸到位，瀑布开始流；4.0 s 时接上瀑布的两条循环。
			S->Play(TEXT("Mech.Sluice.Wheel"), Loc);
			if (W.bPulled)
			{
				S->PlayDelayed(2.2f, TEXT("Mech.Waterfall.Start"), AnchorLocation(TEXT("Waterfall"), Loc));
				WaterfallLoopsAt = Now + 4.0;
				if (!Water.IsValid()) SetWaterfall(true, true);   // 关卡里没有水面 Actor：拉杆就是瀑布开关
			}
			else if (!Water.IsValid())
			{
				SetWaterfall(false, false);
			}
		}
		else
		{
			S->Play(W.bPulled ? FName(TEXT("Mech.Lever.Pull")) : FName(TEXT("Mech.Lever.Push")), Loc);
		}
	}
}

void ADysisSfxDirector::UpdateBeams()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	for (FBeamWatch& W : BeamWatches)
	{
		ADysisBeamActor* B = W.Beam.Get();
		if (!B) continue;
		const bool bWalk = B->IsWalkableNow();
		if (bWalk != W.bWalkable)
		{
			const double PrevChange = W.ChangedAt;
			W.bWalkable = bWalk;
			W.ChangedAt = Now;
			// 灭了至少半秒再亮才算“显形”（光路在临界角附近闪的时候不连响）；开局两秒内不算。
			W.bPendingReveal = bWalk && (Now - PrevChange > 0.5) && (Now - StartTime > 2.0);
		}
		if (W.bPendingReveal && bWalk && Now - W.ChangedAt >= 0.25)
		{
			W.bPendingReveal = false;
			if (B->bPrologueBeam)
			{
				if (!bPrologueRevealPlayed)
				{
					bPrologueRevealPlayed = true;
					S->Play(TEXT("Light.RevealOpening"), B->WindowCm);
				}
			}
			else
			{
				S->Play(TEXT("Light.Reveal"), (B->WindowCm + B->GetTipCm()) * 0.5);
			}
		}
	}
}

void ADysisSfxDirector::UpdateStatues()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const double Now = GetWorld()->GetTimeSeconds();

	if (ADysisMirrorSource* M = Mirror.Get())
	{
		// 三相像：转一格。
		if (M->GetCurrentSlot() != MirrorSlot)
		{
			MirrorSlot = M->GetCurrentSlot();
			S->Play(TEXT("Mech.Statue.Turn"), AnchorLocation(TEXT("Mirror"), M->GetActorLocation()));
		}
		// 镜子醒来：镜面重新被阳光照到一半以上（白天、之前暗了至少 2 秒）。
		const bool bLit = M->CanMirrorBeamWalk();
		const UDysisTimeComponent* Time = GetPlayerPawn() ? GetPlayerPawn()->FindComponentByClass<UDysisTimeComponent>() : nullptr;
		if (!bLit && bMirrorLit) MirrorDarkSince = Now;
		if (bLit && !bMirrorLit && Now - MirrorDarkSince > 2.0 && Now - StartTime > 2.0 && (!Time || !Time->bNight))
		{
			S->Play(TEXT("Story.Mirror.Awaken"), AnchorLocation(TEXT("Mirror"), M->GetActorLocation()));
		}
		bMirrorLit = bLit;
	}

	if (ADysisPrismActor* P = Prism.Get())
	{
		// 棱镜：转到第几格播第几个音（红→紫）；第八格没有颜色，只是转台的石头声。
		if (P->GetCurrentSlot() != PrismSlot)
		{
			PrismSlot = P->GetCurrentSlot();
			const FVector Loc = AnchorLocation(TEXT("Prism"), P->GetActorLocation());
			if (PrismSlot >= 0 && PrismSlot < 7) S->Play(TEXT("Mech.Prism.Turn"), Loc, PrismSlot);
			else S->Play(TEXT("Mech.Statue.Turn"), Loc);
		}
	}
}

void ADysisSfxDirector::UpdateApple()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	ADysisGoldenApple* G = Apple.Get();
	if (!G) return;
	const uint8 StateNow = static_cast<uint8>(G->State);
	if (StateNow == AppleState) return;
	const uint8 Carried = static_cast<uint8>(EAppleState::Carried);
	const uint8 Placed = static_cast<uint8>(EAppleState::Placed);
	if (StateNow == Carried)
	{
		// 接住最后一缕光：取下金苹果，之后一直捧着（很轻的循环）。
		S->Play(TEXT("Story.Apple.Take"), G->GetActorLocation());
		S->StartLoop(DSD::LoopApple, TEXT("Story.Apple.HoldLoop"), G->GetActorLocation(), 3.0f);
	}
	else if (StateNow == Placed)
	{
		S->StopLoop(DSD::LoopApple, 2.0f);
		S->Play(TEXT("Story.Apple.Place"), G->GetActorLocation());
	}
	AppleState = StateNow;
}

void ADysisSfxDirector::UpdateStones()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	for (FStoneWatch& W : StoneWatches)
	{
		ADysisLuminousStone* Stone = W.Stone.Get();
		if (!Stone) continue;
		const float Glow = Stone->GetGlow();
		if (!W.bLit && Glow >= 0.95f)
		{
			W.bLit = true;
			S->Play(TEXT("Moon.Stone.Appear"), Stone->GetActorLocation());
		}
		else if (W.bLit && Glow < Stone->WalkableGlow)
		{
			W.bLit = false;
			S->Play(TEXT("Moon.Stone.Vanish"), Stone->GetActorLocation());
		}
	}
}

void ADysisSfxDirector::SetWaterfall(bool bOn, bool bFromSluice)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	if (bOn == bWaterfallOn || !S) return;
	bWaterfallOn = bOn;
	const double Now = GetWorld()->GetTimeSeconds();
	if (bOn)
	{
		if (!bFromSluice || WaterfallLoopsAt < Now) WaterfallLoopsAt = Now;   // 读档/别的原因开的：直接接循环
	}
	else
	{
		S->Play(TEXT("Mech.Waterfall.Stop"), AnchorLocation(TEXT("Waterfall"), GetActorLocation()));
		S->StopLoop(DSD::LoopFallNear, 1.0f);
		S->StopLoop(DSD::LoopFallFar, 1.0f);
		WaterfallLoopsAt = -1.0;
	}
}

void ADysisSfxDirector::UpdateWater()
{
	ADysisMoonSurface* W = Water.Get();
	if (!W) return;
	const uint8 State = static_cast<uint8>(W->State);
	const uint8 FlowOut = static_cast<uint8>(EDysisWaterState::FlowOut);
	const uint8 Ripple = static_cast<uint8>(EDysisWaterState::Ripple);
	if (!bWaterKnown)
	{
		bWaterKnown = true;
		WaterState = State;
		if (State == FlowOut) SetWaterfall(true, false);
		return;
	}
	if (State == WaterState) return;

	const double Now = GetWorld()->GetTimeSeconds();
	if (State == FlowOut) SetWaterfall(true, WaterfallLoopsAt > Now);
	else if (WaterState == FlowOut) SetWaterfall(false, false);
	if (State == Ripple && bMechanisms && Sfx.IsValid())
	{
		// 水皱成道：月光大道（影桥）接上。
		Sfx->Play(TEXT("Story.ShadowBridge.Join"), AnchorLocation(TEXT("Pool"), W->GetActorLocation()));
	}
	WaterState = State;
}

void ADysisSfxDirector::UpdateRoofSteps(float Dt)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWarm = Now - StartTime > 1.0;
	float MaxSpeed = 0.0f;
	FVector MovingSum = FVector::ZeroVector;
	int32 MovingNum = 0;

	auto Track = [&](TArray<FStepWatch>& Steps, bool bDown)
	{
		for (int32 k = 0; k < Steps.Num(); ++k)
		{
			FStepWatch& W = Steps[k];
			AActor* Step = W.Step.Get();
			if (!Step) continue;
			const float Z = static_cast<float>(Step->GetActorLocation().Z);
			const float Speed = FMath::Abs(Z - W.LastZ) / FMath::Max(Dt, 1.0e-3f);
			W.LastZ = Z;
			if (Speed > 1.0f)
			{
				if (bDown && !bStairsLowerPlayed && bWarm)
				{
					// 入夜：下行梯开始降。
					bStairsLowerPlayed = true;
					const int32 Mid = Steps.Num() / 2;
					const AActor* MidStep = Steps.IsValidIndex(Mid) ? Steps[Mid].Step.Get() : Step;
					S->Play(TEXT("Mech.Stairs.Lower"), MidStep ? MidStep->GetActorLocation() : Step->GetActorLocation());
				}
				W.bMoving = true;
				W.bEverMoved = true;
				W.StillSeconds = 0.0f;
				MaxSpeed = FMath::Max(MaxSpeed, Speed);
				MovingSum += Step->GetActorLocation();
				++MovingNum;
			}
			else if (W.bMoving)
			{
				W.StillSeconds += Dt;
				if (W.StillSeconds > 0.15f)
				{
					W.bMoving = false;
					// 下行梯：第 k 级落平到底，播第 k 个音。
					if (bDown && bWarm) S->Play(TEXT("Mech.Steps.Settle"), Step->GetActorLocation(), k);
				}
			}
		}
	};
	Track(UpWatches, false);
	Track(DnWatches, true);

	// 上行：踏步跟着人走过的角度升起；进度每过 1/16 落一个音（第 1 级最低、第 16 级最高），一个接一个。
	if (UpWatches.Num() > 0 && bWarm)
	{
		const int32 Last = UpWatches.Num() - 1;
		const float RingZ = UDysisSkyLibrary::DysisConst(TEXT("RING_Y")) * 100.0f;
		const float Span = 45.0f * static_cast<float>(UpWatches.Num());
		const float T = FMath::Clamp((UpWatches[Last].LastZ - RingZ) / FMath::Max(Span, 1.0f), 0.0f, 1.0f);
		for (int32 k = 0; k < UpWatches.Num(); ++k)
		{
			const float Threshold = static_cast<float>(k + 1) / static_cast<float>(UpWatches.Num());
			if (!RiseFired[k] && T >= Threshold - 0.002f)
			{
				RiseFired[k] = true;
				RiseQueue.Add(k);
			}
			else if (RiseFired[k] && T < Threshold - 1.0f / static_cast<float>(UpWatches.Num()) - 0.03f)
			{
				RiseFired[k] = false;   // 人往回走、踏步落下去了：下次再升起时再响
			}
		}
	}
	if (RiseQueue.Num() > 0 && Now >= NextRiseAt)
	{
		const int32 k = RiseQueue[0];
		RiseQueue.RemoveAt(0);
		if (const AActor* Step = UpWatches.IsValidIndex(k) ? UpWatches[k].Step.Get() : nullptr)
			S->Play(TEXT("Mech.Steps.Rise"), Step->GetActorLocation(), k);
		NextRiseAt = Now + 0.12;
	}

	// 有踏步在动：循环，音量跟速度（踏步最快 60 cm/s）。
	if (MovingNum > 0 && bWarm)
	{
		StepsLoopIdle = 0.0f;
		const FVector Center = MovingSum / static_cast<double>(MovingNum);
		const float Scale = FMath::Clamp(MaxSpeed / 40.0f, 0.15f, 1.0f);
		EnsureLoop(DSD::LoopSteps, TEXT("Mech.Steps.MoveLoop"), Center, Scale, 0.15f, 0.2f);
		S->SetLoopLocation(DSD::LoopSteps, Center);
	}
	else if (S->IsLoopActive(DSD::LoopSteps))
	{
		StepsLoopIdle += Dt;
		if (StepsLoopIdle > 0.3f) S->StopLoop(DSD::LoopSteps, 0.4f);
	}
}

void ADysisSfxDirector::UpdateIris(float Dt)
{
	// 光圈叶片：任何一片在动 → 叶片移动的循环放在叶片中心；都停了 → 停。
	UDysisSfxSubsystem* S = Sfx.Get();
	if (IrisWatches.Num() == 0) return;
	const double Now = GetWorld()->GetTimeSeconds();
	bool bAnyMoving = false;
	FVector Sum = FVector::ZeroVector;
	int32 Num = 0;
	for (FStepWatch& W : IrisWatches)
	{
		AActor* Blade = W.Step.Get();
		if (!Blade) continue;
		const FVector L = Blade->GetActorLocation();
		Sum += L;
		++Num;
		const float Z = static_cast<float>(L.Z);
		if (FMath::Abs(Z - W.LastZ) > 0.05f) bAnyMoving = true;
		W.LastZ = Z;
	}
	if (Num == 0 || Now - StartTime < 1.0) return;
	const FVector Center = Sum / static_cast<double>(Num);
	if (bAnyMoving)
	{
		EnsureLoop(DSD::LoopIris, TEXT("Mech.Iris.MoveLoop"), Center, 1.0f, 0.2f, 0.3f);
	}
	else if (S->IsLoopActive(DSD::LoopIris))
	{
		S->StopLoop(DSD::LoopIris, 0.6f);
	}
	(void)Dt;
}

void ADysisSfxDirector::UpdateGate()
{
	// 屋顶桥门：接光之前转动 = 打开（现在导入姿态就是开着的，接上开门动画后自动有声）。
	const AActor* Door = FindAnchor(TEXT("BridgeDoor"));
	if (!Door || !bDoorKnown) return;
	const FRotator R = Door->GetActorRotation();
	if (!R.Equals(DoorRot, 0.5f))
	{
		if (!bCaught && GetWorld()->GetTimeSeconds() - StartTime > 1.0) Sfx->Play(TEXT("Mech.Gate.Open"), Door->GetActorLocation());
		DoorRot = R;
	}
}

void ADysisSfxDirector::UpdateProximity(float Dt)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const APawn* Pawn = GetPlayerPawn();
	if (!Pawn) return;
	const UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>();
	const bool bNight = Time && Time->bNight;
	const FVector P = Pawn->GetActorLocation();

	// 虹门：白天穿过虹门，彩虹亮一下（每次经过最多一次）。
	if (const AActor* Arch = FindAnchor(TEXT("RainbowArch")))
	{
		const FVector A = Arch->GetActorLocation();
		const double D2 = FVector::DistSquared2D(A, P);
		if (!bInRainbowGate && D2 < FMath::Square(300.0) && FMath::Abs(A.Z - P.Z) < 400.0)
		{
			bInRainbowGate = true;
			if (!bNight) S->Play(TEXT("Story.RainbowGate"), A);
		}
		else if (bInRainbowGate && D2 > FMath::Square(700.0))
		{
			bInRainbowGate = false;
		}
	}

	// 塞勒涅梦话：白天、她还没醒（靛色还没入眼）、人在浮雕 7 m 以内，隔 10–18 秒说一段。
	if (const AActor* Relief = FindAnchor(TEXT("SeleneRelief")))
	{
		if (!bNight && !bIndigoFired && FVector::DistSquared(Relief->GetActorLocation(), P) < FMath::Square(700.0f))
		{
			SleepTalkTimer -= Dt;
			if (SleepTalkTimer <= 0.0f)
			{
				S->Play(TEXT("Story.Selene.SleepTalk"), Relief->GetActorLocation());
				SleepTalkTimer = FMath::FRandRange(10.0f, 18.0f);
			}
		}
		else
		{
			SleepTalkTimer = FMath::Max(SleepTalkTimer, 3.0f);
		}
	}
}

void ADysisSfxDirector::UpdateIndoor(float Dt, bool bSnap)
{
	const APawn* Pawn = GetPlayerPawn();
	if (!Pawn) return;
	const UDysisSfxSettings* Cfg = UDysisSfxSettings::Get();
	const UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>();
	const FString Zone = Time ? (Time->GetZoneOverride().IsEmpty() ? Time->Zone : Time->GetZoneOverride()) : FString();

	// 殿内外：殿外的区域名或屋顶以上 = 殿外。开局第一秒直接到位，之后进出殿平滑过渡。
	bool bIndoor = !Zone.IsEmpty() && PlayerFoot().Z < Cfg->RoofHeightCm;
	for (const FString& Z : Cfg->OutdoorZones)
	{
		if (Zone.Equals(Z, ESearchCase::IgnoreCase)) { bIndoor = false; break; }
	}
	const float Want = bIndoor ? 1.0f : 0.0f;
	Indoor = bSnap ? Want : FMath::FInterpConstantTo(Indoor, Want, Dt, 1.0f / FMath::Max(Cfg->IndoorBlendSeconds, 0.05f));

	// 殿内混响跟着走（播放子系统按这个给每个声音送混响）。
	if (UDysisSfxSubsystem* S = Sfx.Get()) S->SetReverbEnvironment(Indoor);
}

void ADysisSfxDirector::UpdateAmbience(float Dt)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const UDysisSfxSettings* Cfg = UDysisSfxSettings::Get();
	const APawn* Pawn = GetPlayerPawn();
	if (!Pawn) return;
	const UDysisTimeComponent* Time = Pawn->FindComponentByClass<UDysisTimeComponent>();
	const bool bNight = Time && Time->bNight;
	const FString Zone = Time ? (Time->GetZoneOverride().IsEmpty() ? Time->Zone : Time->GetZoneOverride()) : FString();
	const FVector Foot = PlayerFoot();
	const double Now = GetWorld()->GetTimeSeconds();

	// 殿内程度 Indoor 由 UpdateIndoor 每帧算好。
	DayAmount = FMath::FInterpConstantTo(DayAmount, bNight ? 0.0f : 1.0f, Dt, 1.0f / Cfg->BirdsNightFadeSeconds);
	const FVector Here = Pawn->GetActorLocation();

	// 远处海浪：一直在；进殿压低、低通到 800 Hz。
	EnsureLoop(DSD::LoopSeaFar, TEXT("Amb.Sea.Far"), Here, FMath::Lerp(1.0f, 0.3f, Indoor), 0.3f, 3.0f);
	S->SetLoopLowPass(DSD::LoopSeaFar, DSD::LowPassFor(Indoor, 800.0f));

	// 近处海浪：殿外、越低越响（岛上、崖脚）。
	const float LowFactor = FMath::Clamp(1.0f - (static_cast<float>(Foot.Z) - 300.0f) / 1800.0f, 0.15f, 1.0f);
	EnsureLoop(DSD::LoopSeaClose, TEXT("Amb.Sea.Close"), Here, (1.0f - Indoor) * LowFactor, 0.5f, 3.0f);

	// 海风：越高越响；进殿压低、低通到 600 Hz。
	const float H = DSD::SmoothStep01(Cfg->WindLowCm, Cfg->WindHighCm, static_cast<float>(Foot.Z));
	EnsureLoop(DSD::LoopWind, TEXT("Amb.RoofWind"), Here, H * FMath::Lerp(1.0f, 0.2f, Indoor), 0.5f, 2.0f);
	S->SetLoopLowPass(DSD::LoopWind, DSD::LowPassFor(Indoor, 600.0f));

	// 殿内底噪。
	EnsureLoop(DSD::LoopRoom, TEXT("Amb.RoomTone"), Here, Indoor, 0.5f, 2.0f);

	// 白天鸟鸣：入夜淡出后停掉；殿内很远、很闷。
	if (DayAmount > 0.001f)
	{
		EnsureLoop(DSD::LoopBirds, TEXT("Amb.DayBirds"), Here, DayAmount * FMath::Lerp(1.0f, 0.3f, Indoor), 0.5f, 3.0f);
		S->SetLoopLowPass(DSD::LoopBirds, DSD::LowPassFor(Indoor, 1500.0f));
	}
	else if (S->IsLoopActive(DSD::LoopBirds))
	{
		S->StopLoop(DSD::LoopBirds, 1.0f);
	}

	// 水池：一直在（3D）；夜里潮水涨起来响一点。
	const FVector PoolLoc = AnchorLocation(TEXT("Pool"), Water.IsValid() ? Water->GetActorLocation() : FVector::ZeroVector);
	EnsureLoop(DSD::LoopPool, TEXT("Amb.Pool"), PoolLoc, bNight ? 1.4f : 1.0f, 3.0f, 3.0f);

	// 瀑布：开闸以后（闸轮 4 秒那一刻）接上近、远两层；水雾跟着淡入。
	const FVector FallLoc = AnchorLocation(TEXT("Waterfall"), PoolLoc);
	if (bWaterfallOn && WaterfallLoopsAt >= 0.0 && Now >= WaterfallLoopsAt)
	{
		EnsureLoop(DSD::LoopFallNear, TEXT("Amb.Waterfall.Near"), FallLoc, 1.0f, 0.5f, 1.0f);
		EnsureLoop(DSD::LoopFallFar, TEXT("Amb.Waterfall.Far"), FallLoc, 1.0f, 0.5f, 1.0f);
	}
	MistAmount = FMath::FInterpConstantTo(MistAmount, bWaterfallOn ? 1.0f : 0.0f, Dt, 1.0f / 4.0f);
	if (MistAmount > 0.001f) EnsureLoop(DSD::LoopMist, TEXT("Amb.Mist"), PoolLoc, MistAmount, 0.3f, 2.0f);
	else if (S->IsLoopActive(DSD::LoopMist)) S->StopLoop(DSD::LoopMist, 1.0f);

	// 月桥上、夜里：塞勒涅怀里月亮的两条持续音（月光对准以后不再响）。
	if (bMechanisms)
	{
		const bool bDrone = bNight && !bSeleneLocked && Zone.Equals(TEXT("moonbr"), ESearchCase::IgnoreCase);
		const FVector GoddessLoc = AnchorLocation(TEXT("Goddess"), FallLoc);
		if (bDrone)
		{
			EnsureLoop(DSD::LoopDroneLow, TEXT("Story.Selene.DroneLow"), GoddessLoc, 0.6f, 1.0f, 2.0f);
			EnsureLoop(DSD::LoopDroneHigh, TEXT("Story.Selene.DroneHigh"), GoddessLoc, 0.5f, 1.0f, 2.0f);
		}
		else
		{
			if (S->IsLoopActive(DSD::LoopDroneLow)) S->StopLoop(DSD::LoopDroneLow, 1.5f);
			if (S->IsLoopActive(DSD::LoopDroneHigh)) S->StopLoop(DSD::LoopDroneHigh, 1.5f);
		}
	}

	// 湿石判定交给脚步组件。
	if (UDysisFootstepComponent* F = Footsteps.Get()) F->SetWetSource(bWaterfallOn, FallLoc);
}

// ───────────────────────── 关卡标题 ─────────────────────────

int32 ADysisSfxDirector::LevelOf(const FString& Zone, bool bNight)
{
	// 灰盒 levelOf（prototype/temple/index.html）：区域 → 第几关（0=序，1–5=日1–日5，6–10=月1–月5）。
	auto Is = [&Zone](const TCHAR* Z) { return Zone.Equals(Z, ESearchCase::IgnoreCase); };
	if (!bNight)
	{
		if (Is(TEXT("out")) || Is(TEXT("beam:isle"))) return 0;
		if (Is(TEXT("L0")) || Is(TEXT("beam:b1")) || Is(TEXT("wfback"))) return 1;
		if (Is(TEXT("L1")) || Is(TEXT("beam:b2")) || Is(TEXT("rainbow")) || Is(TEXT("sill"))) return 2;
		if (Is(TEXT("L2")) || Zone.StartsWith(TEXT("beam:h")) || Is(TEXT("beam:mirror")) || Is(TEXT("ledge"))) return 3;
		if (Is(TEXT("L3"))) return 4;
		if (Is(TEXT("beam:oculus")) || Is(TEXT("rbridge")) || Is(TEXT("crown"))) return 5;
		return -1;
	}
	if (Is(TEXT("crown")) || Is(TEXT("rbridge"))) return 6;
	if (Is(TEXT("L3")) || Is(TEXT("tun:TS"))) return 7;
	if (Is(TEXT("L2")) || Is(TEXT("tun:TR"))) return 8;
	if (Is(TEXT("L1")) || Is(TEXT("moonbr"))) return 9;
	if (Is(TEXT("L0")) || Is(TEXT("pav")) || Is(TEXT("gbridge")) || Is(TEXT("shadowbr")) || Is(TEXT("wfback"))) return 10;
	return -1;
}

void ADysisSfxDirector::UpdateTitles()
{
	const APawn* Pawn = GetPlayerPawn();
	const UDysisTimeComponent* Time = Pawn ? Pawn->FindComponentByClass<UDysisTimeComponent>() : nullptr;
	if (!Time || GetWorld()->GetTimeSeconds() - StartTime < 1.0) return;
	const int32 Level = LevelOf(Time->Zone, Time->bNight);
	if (Level > SeenLevel)
	{
		SeenLevel = Level;
		// 文件顺序：序、日1–日5、日落之后、月1–月5 → 第 6 个是“日落之后”，月 n 要往后挪一个。
		Sfx->Play(TEXT("UI.Title"), FVector::ZeroVector, Level <= 5 ? Level : Level + 1);
	}
}

// ───────────────────────── 界面 ─────────────────────────

void ADysisSfxDirector::UpdateUI()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextUIPollAt) return;
	NextUIPollAt = Now + 0.1;   // 互动提示要打一条射线：10 Hz 足够

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// 互动提示：从“没有可摸的”变成“正对着一个有提示的东西”。
	if (const UDysisInteractComponent* Interact = Pawn ? Pawn->FindComponentByClass<UDysisInteractComponent>() : nullptr)
	{
		AActor* Target = nullptr;
		bool bShow = false;
		if (Interact->CanInteractNow(Target))
			if (const IDysisInteractable* I = Cast<IDysisInteractable>(Target))
				bShow = !I->GetInteractPrompt().IsEmpty();
		if (bShow && (!bPromptShown || PromptTarget.Get() != Target)) S->Play(TEXT("UI.Prompt"), FVector::ZeroVector);
		bPromptShown = bShow;
		PromptTarget = bShow ? Target : nullptr;
	}

	// 对话翻到下一句（自动翻或按 E）。
	if (const ADysisHUD* HUD = PC ? Cast<ADysisHUD>(PC->GetHUD()) : nullptr)
	{
		UDysisDialogueComponent* D = HUD->ActiveDialogue;
		if (D && D->IsPlaying())
		{
			if (LastDialogue.Get() != D) { LastDialogue = D; LastDialogueLine = D->CurrentLine; }
			else if (D->CurrentLine > LastDialogueLine) { S->Play(TEXT("UI.DialogueNext"), FVector::ZeroVector); LastDialogueLine = D->CurrentLine; }
		}
		else
		{
			LastDialogue = nullptr;
			LastDialogueLine = -1;
		}
	}
}

void ADysisSfxDirector::HandleNotification(const FText& Text)
{
	if (bUIHooks && Sfx.IsValid() && !Text.IsEmpty()) Sfx->Play(TEXT("UI.Text"), FVector::ZeroVector);
}

// ───────────────────────── 订阅的事件 ─────────────────────────

void ADysisSfxDirector::HandleLightCaught()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	bCaught = true;
	if (!S) return;
	// 桥门转回去关上（卡住那一下在 1.37 s）。
	const AActor* Door = FindAnchor(TEXT("BridgeDoor"));
	S->Play(TEXT("Mech.Gate.Close"), Door ? Door->GetActorLocation() : GetActorLocation());
	if (Door) DoorRot = Door->GetActorRotation();
	// “日落之后·入夜”标题（灰盒：接光后 5 秒）；月1 的标题就不再单独响了。
	if (bTitles && !bDuskTitlePlayed)
	{
		bDuskTitlePlayed = true;
		S->PlayDelayed(5.0f, TEXT("UI.Title"), FVector::ZeroVector, 6);
		SeenLevel = FMath::Max(SeenLevel, 6);
	}
}

void ADysisSfxDirector::HandleNicheCollected(EDysisNiche Niche)
{
	UDysisSfxSubsystem* S = Sfx.Get();
	if (!S) return;
	FVector Loc = GetActorLocation();
	for (const TWeakObjectPtr<ADysisNicheActor>& N : Niches)
	{
		if (N.IsValid() && N->Niche == Niche) { Loc = N->GetActorLocation(); break; }
	}
	switch (Niche)
	{
	case EDysisNiche::Rainbow:
		S->Play(TEXT("Mech.Niche.DoorsOpen"), Loc);
		S->PlayDelayed(0.5f, TEXT("Story.Shard.Rainbow"), Loc);
		break;
	case EDysisNiche::Sun:  S->Play(TEXT("Story.Shard.Sun"), Loc); break;
	case EDysisNiche::Moon: S->Play(TEXT("Story.Shard.Moon"), Loc); break;
	}
	// 三片集齐：正十二面体与星座亮起。
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		if (UDysisSaveSubsystem* Save = GI->GetSubsystem<UDysisSaveSubsystem>())
			if (Save->GetCurrent() && Save->GetCurrent()->AllNichesCollected())
				S->PlayDelayed(3.6f, TEXT("Story.Dodecahedron"), Loc);
}

void ADysisSfxDirector::HandleIndigo()
{
	bIndigoFired = true;
	if (UDysisSfxSubsystem* S = Sfx.Get())
	{
		const FVector Loc = AnchorLocation(TEXT("SeleneRelief"), Prism.IsValid() ? Prism->GetActorLocation() : GetActorLocation());
		S->PlayDelayed(0.3f, TEXT("Story.Selene.Eyes"), Loc);
	}
}

void ADysisSfxDirector::HandleRainbowAligned()
{
	UDysisSfxSubsystem* S = Sfx.Get();
	const ADysisRainbowAlign* R = RainbowAlign.Get();
	if (!S || !R) return;
	const FVector ReliefLoc = AnchorLocation(TEXT("IrisRelief"), R->ReliefHeadCm);
	S->Play(TEXT("Story.IrisRelief.Awaken"), ReliefLoc);
	S->PlayDelayed(1.2f, TEXT("Story.RainbowBridge.Appear"), AnchorLocation(TEXT("RainbowBridge"), ReliefLoc));
}
