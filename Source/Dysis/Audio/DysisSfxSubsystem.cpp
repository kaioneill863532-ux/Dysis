// 狄西斯的日落回廊 · 音效播放子系统
#include "Audio/DysisSfxSubsystem.h"

#include "AudioDevice.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundSubmix.h"

namespace
{
	constexpr float GDysisSfxLowPassOffHz = 20000.0f;
}

// ───────────────────────── 生命周期 ─────────────────────────

UDysisSfxSubsystem* UDysisSfxSubsystem::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDysisSfxSubsystem>() : nullptr;
}

bool UDysisSfxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDysisSfxSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	EditedHandle = UDysisSfxSettings::OnEventEdited.AddUObject(this, &UDysisSfxSubsystem::OnSettingsEdited);
	StartPreload();
}

void UDysisSfxSubsystem::Deinitialize()
{
	UDysisSfxSettings::OnEventEdited.Remove(EditedHandle);
	for (TPair<FName, FLoop>& Pair : Loops)
	{
		if (UAudioComponent* C = Pair.Value.Comp.Get()) C->Stop();
	}
	Loops.Reset();
	LoopRefs.Reset();
	Pending.Reset();
	if (PreloadHandle.IsValid()) PreloadHandle->ReleaseHandle();
	PreloadHandle.Reset();
	if (ReverbLoadHandle.IsValid()) ReverbLoadHandle->ReleaseHandle();
	ReverbLoadHandle.Reset();
	Super::Deinitialize();
}

TStatId UDysisSfxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDysisSfxSubsystem, STATGROUP_Tickables);
}

void UDysisSfxSubsystem::StartPreload()
{
	// 开局异步把所有声音载进来（第一次播时就不会卡）；没导入的文件跳过（Dysis.Sfx.Check 会列出来）。
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	TArray<FSoftObjectPath> Paths;
	for (const FDysisSfxEvent& E : S->Events)
	{
		for (const TSoftObjectPtr<USoundBase>& Snd : E.Sounds)
		{
			const FSoftObjectPath& P = Snd.ToSoftObjectPath();
			if (P.IsValid() && FPackageName::DoesPackageExist(P.GetLongPackageName()))
			{
				Paths.AddUnique(P);
			}
		}
	}
	// 殿内混响单独先载（几个小资产，开局很快就好，不用等 265 个声音）。
	const FSoftObjectPath& ReverbPath = S->ReverbSubmix.ToSoftObjectPath();
	if (ReverbPath.IsValid() && FPackageName::DoesPackageExist(ReverbPath.GetLongPackageName()))
	{
		ReverbLoadHandle = Streamable.RequestAsyncLoad(TArray<FSoftObjectPath>{ ReverbPath });
	}
	if (Paths.Num() > 0)
	{
		PreloadHandle = Streamable.RequestAsyncLoad(Paths);
	}
	UE_LOG(LogTemp, Display, TEXT("DysisSfx: %d 项音效，预载 %d 个声音文件"), S->Events.Num(), Paths.Num());
}

double UDysisSfxSubsystem::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

bool UDysisSfxSubsystem::IsDebug() const
{
	return bDebug || UDysisSfxSettings::Get()->bShowDebug;
}

void UDysisSfxSubsystem::DebugPrint(const FString& Line) const
{
	if (!IsDebug()) return;
	UE_LOG(LogTemp, Display, TEXT("DysisSfx: %s"), *Line);
	if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(140, 210, 255), Line);
}

// ───────────────────────── 一次性 ─────────────────────────

float UDysisSfxSubsystem::BaseVolume(const FDysisSfxEvent& Event) const
{
	return Event.Volume * UDysisSfxSettings::Get()->GetCategoryVolume(Event.Category);
}

USoundBase* UDysisSfxSubsystem::ResolveSound(const FDysisSfxEvent& Event, int32 Variant, FKeyState& State)
{
	const int32 Num = Event.Sounds.Num();
	if (Num == 0) return nullptr;

	int32 Idx = 0;
	if (Variant >= 0)
	{
		Idx = Variant % Num;
	}
	else if (Num > 1)
	{
		Idx = FMath::RandRange(0, Num - 1);
		if (Event.bNoRepeat && Idx == State.LastIndex)
		{
			Idx = (Idx + 1 + FMath::RandRange(0, Num - 2)) % Num;
		}
	}
	State.LastIndex = Idx;

	USoundBase* Sound = Event.Sounds[Idx].Get();
	if (!Sound)
	{
		Sound = Event.Sounds[Idx].LoadSynchronous();
		if (Sound) Loaded.AddUnique(Sound);
	}
	if (!Sound && !State.bWarnedMissing)
	{
		State.bWarnedMissing = true;
		UE_LOG(LogTemp, Warning, TEXT("DysisSfx: %s 的声音文件没找到（%s）——先跑 Art/Audio/ue_import_sfx.py 导入"),
			*Event.Key.ToString(), *Event.Sounds[Idx].ToString());
	}
	return Sound;
}

USoundAttenuation* UDysisSfxSubsystem::GetAttenuation(const FDysisSfxEvent& Event)
{
	const FVector2f Want(Event.InnerRadiusCm, Event.FalloffDistanceCm);
	TObjectPtr<USoundAttenuation>* Found = Attenuations.Find(Event.Key);
	const FVector2f* Have = AttenuationParams.Find(Event.Key);
	if (Found && *Found && Have && Have->Equals(Want, 0.5f))
	{
		return *Found;
	}

	USoundAttenuation* Att = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& A = Att->Attenuation;
	A.bAttenuate = true;
	A.bSpatialize = true;
	A.AttenuationShape = EAttenuationShape::Sphere;
	A.AttenuationShapeExtents = FVector(FMath::Max(Event.InnerRadiusCm, 0.0f), 0.0f, 0.0f);
	A.FalloffDistance = FMath::Max(Event.FalloffDistanceCm, 1.0f);
	A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	A.StereoSpread = 200.0f;   // 立体声素材：左右两声道分开 2 m，近处有宽度、远处收成一点
	// 远处少一点高频（空气吸收），近处不动。
	A.bAttenuateWithLPF = true;
	A.LPFRadiusMin = FMath::Max(Event.InnerRadiusCm, 0.0f) + 0.3f * A.FalloffDistance;
	A.LPFRadiusMax = FMath::Max(Event.InnerRadiusCm, 0.0f) + A.FalloffDistance;
	A.LPFFrequencyAtMin = GDysisSfxLowPassOffHz;
	A.LPFFrequencyAtMax = 5000.0f;

	Attenuations.Add(Event.Key, Att);
	AttenuationParams.Add(Event.Key, Want);
	return Att;
}

UAudioComponent* UDysisSfxSubsystem::PlayInternal(FName Key, const FVector* Location, USceneComponent* AttachTo, int32 Variant,
	float VolumeScale, float PitchScale, bool bAudition)
{
	UWorld* World = GetWorld();
	const FDysisSfxEvent* Event = UDysisSfxSettings::Get()->FindEvent(Key);
	if (!World || !Event)
	{
		if (!Event) DebugPrint(FString::Printf(TEXT("（没有这一项：%s）"), *Key.ToString()));
		return nullptr;
	}
	if (!bAudition && !Event->bEnabled) return nullptr;

	FKeyState& State = KeyStates.FindOrAdd(Key);
	const double T = Now();
	if (!bAudition && Event->CooldownSeconds > 0.0f && T - State.LastPlayTime < Event->CooldownSeconds)
	{
		return nullptr;
	}

	USoundBase* Sound = ResolveSound(*Event, Variant, State);
	if (!Sound) return nullptr;

	// 同时最多几个：多了就停掉最早的。
	State.Active.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& C) { return !C.IsValid() || !C->IsPlaying(); });
	while (State.Active.Num() >= FMath::Max(Event->MaxInstances, 1))
	{
		if (UAudioComponent* Oldest = State.Active[0].Get()) Oldest->Stop();
		State.Active.RemoveAt(0);
	}

	const float VJ = Event->VolumeJitter > 0.0f ? FMath::FRandRange(-Event->VolumeJitter, Event->VolumeJitter) : 0.0f;
	const float PJ = Event->PitchJitter > 0.0f ? FMath::FRandRange(-Event->PitchJitter, Event->PitchJitter) : 0.0f;
	const float Volume = FMath::Max(BaseVolume(*Event) * VolumeScale * (1.0f + VJ), 0.0f);
	const float Pitch = FMath::Clamp(Event->Pitch * PitchScale * (1.0f + PJ), 0.1f, 4.0f);

	UAudioComponent* Comp = nullptr;
	const bool b2D = bAudition || Event->b2D || (!Location && !AttachTo);
	if (b2D)
	{
		Comp = UGameplayStatics::SpawnSound2D(World, Sound, Volume, Pitch);
	}
	else if (AttachTo)
	{
		Comp = UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
			true, Volume, Pitch, 0.0f, GetAttenuation(*Event));
	}
	else
	{
		Comp = UGameplayStatics::SpawnSoundAtLocation(World, Sound, *Location, FRotator::ZeroRotator, Volume, Pitch, 0.0f, GetAttenuation(*Event));
	}

	State.LastPlayTime = T;
	if (Comp)
	{
		State.Active.Add(Comp);
		// 混响：试听按殿内算（在哪儿都听得到这一项的混响）；跟着组件走的按组件现在的位置算远近。
		const FVector AttachedAt = AttachTo ? AttachTo->GetComponentLocation() : FVector::ZeroVector;
		const FVector* Where = b2D ? nullptr : (AttachTo ? &AttachedAt : Location);
		ApplyReverb(Comp, *Event, Where, bAudition ? 1.0f : ReverbIndoor);
	}
	DebugPrint(FString::Printf(TEXT("♪ %s  %s  #%d  音量 %.2f 快慢 %.2f%s"), *Key.ToString(), *Event->Label, State.LastIndex + 1, Volume, Pitch,
		b2D ? TEXT("  2D") : TEXT("")));
	return Comp;
}

UAudioComponent* UDysisSfxSubsystem::Play(FName Key, const FVector& Location, int32 Variant, float VolumeScale, float PitchScale)
{
	return PlayInternal(Key, &Location, nullptr, Variant, VolumeScale, PitchScale, false);
}

UAudioComponent* UDysisSfxSubsystem::PlayAttached(FName Key, USceneComponent* AttachTo, int32 Variant, float VolumeScale, float PitchScale)
{
	return AttachTo ? PlayInternal(Key, nullptr, AttachTo, Variant, VolumeScale, PitchScale, false) : nullptr;
}

void UDysisSfxSubsystem::PlayDelayed(float DelaySeconds, FName Key, const FVector& Location, int32 Variant, float VolumeScale, float PitchScale)
{
	if (DelaySeconds <= 0.0f)
	{
		Play(Key, Location, Variant, VolumeScale, PitchScale);
		return;
	}
	FPending P;
	P.At = Now() + DelaySeconds;
	P.Key = Key;
	P.Location = Location;
	P.Variant = Variant;
	P.VolumeScale = VolumeScale;
	P.PitchScale = PitchScale;
	Pending.Add(P);
}

UAudioComponent* UDysisSfxSubsystem::PlayAudition(FName Key, int32 Variant)
{
	return PlayInternal(Key, nullptr, nullptr, Variant, 1.0f, 1.0f, true);
}

// ───────────────────────── 循环 ─────────────────────────

bool UDysisSfxSubsystem::StartLoop(FName LoopId, FName Key, const FVector& Location, float FadeInSeconds, float VolumeScale)
{
	if (FLoop* Existing = Loops.Find(LoopId))
	{
		if (Existing->Comp.IsValid() && Existing->Comp->IsPlaying())
		{
			// 已经在响：取消正在进行的淡出，换成新的目标。
			Existing->bStopping = false;
			Existing->Target = VolumeScale;
			Existing->Rate = 1.0f / FMath::Max(FadeInSeconds, 0.05f);
			return true;
		}
		LoopRefs.Remove(Existing->Comp.Get());
		Loops.Remove(LoopId);
	}

	UWorld* World = GetWorld();
	const FDysisSfxEvent* Event = UDysisSfxSettings::Get()->FindEvent(Key);
	if (!World || !Event) return false;

	FKeyState& State = KeyStates.FindOrAdd(Key);
	USoundBase* Sound = ResolveSound(*Event, -1, State);
	if (!Sound) return false;

	const float Pitch = FMath::Clamp(Event->Pitch, 0.1f, 4.0f);
	UAudioComponent* Comp = Event->b2D
		? UGameplayStatics::SpawnSound2D(World, Sound, 0.001f, Pitch, 0.0f, nullptr, false, false)
		: UGameplayStatics::SpawnSoundAtLocation(World, Sound, Location, FRotator::ZeroRotator, 0.001f, Pitch, 0.0f, GetAttenuation(*Event), nullptr, false);
	if (!Comp) return false;

	FLoop L;
	L.Comp = Comp;
	L.Key = Key;
	L.Scale = 0.0f;
	L.Target = VolumeScale;
	L.Rate = 1.0f / FMath::Max(FadeInSeconds, 0.05f);
	Loops.Add(LoopId, L);
	LoopRefs.Add(Comp);
	DebugPrint(FString::Printf(TEXT("∞ 开循环 %s ← %s  %s"), *LoopId.ToString(), *Key.ToString(), *Event->Label));
	return true;
}

void UDysisSfxSubsystem::SetLoopScale(FName LoopId, float VolumeScale, float BlendSeconds)
{
	if (FLoop* L = Loops.Find(LoopId))
	{
		if (L->bStopping) return;
		L->Target = FMath::Max(VolumeScale, 0.0f);
		L->Rate = 1.0f / FMath::Max(BlendSeconds, 0.02f);
	}
}

void UDysisSfxSubsystem::SetLoopPitchScale(FName LoopId, float PitchScale)
{
	if (FLoop* L = Loops.Find(LoopId)) L->PitchScale = FMath::Clamp(PitchScale, 0.1f, 4.0f);
}

void UDysisSfxSubsystem::SetLoopLowPass(FName LoopId, float CutoffHz)
{
	if (FLoop* L = Loops.Find(LoopId))
	{
		if (UAudioComponent* C = L->Comp.Get())
		{
			const bool bOn = CutoffHz < GDysisSfxLowPassOffHz - 1.0f;
			C->SetLowPassFilterEnabled(bOn);
			if (bOn) C->SetLowPassFilterFrequency(FMath::Max(CutoffHz, 50.0f));
		}
	}
}

void UDysisSfxSubsystem::SetLoopLocation(FName LoopId, const FVector& Location)
{
	if (FLoop* L = Loops.Find(LoopId))
		if (UAudioComponent* C = L->Comp.Get())
			C->SetWorldLocation(Location);
}

void UDysisSfxSubsystem::StopLoop(FName LoopId, float FadeOutSeconds)
{
	if (FLoop* L = Loops.Find(LoopId))
	{
		if (!L->bStopping) DebugPrint(FString::Printf(TEXT("∞ 停循环 %s（%.1f 秒淡出）"), *LoopId.ToString(), FadeOutSeconds));
		L->bStopping = true;
		L->Target = 0.0f;
		L->Rate = 1.0f / FMath::Max(FadeOutSeconds, 0.02f);
	}
}

bool UDysisSfxSubsystem::IsLoopActive(FName LoopId) const
{
	const FLoop* L = Loops.Find(LoopId);
	return L && !L->bStopping && L->Comp.IsValid() && L->Comp->IsPlaying();
}

// ───────────────────────── 混响 ─────────────────────────

USoundSubmixBase* UDysisSfxSubsystem::GetReverbSubmix()
{
	if (ReverbSubmix) return ReverbSubmix;
	if (bReverbTried) return nullptr;

	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	USoundSubmixBase* Submix = S->ReverbSubmix.Get();   // 开局预载完就已经在内存里
	if (!Submix)
	{
		if (ReverbLoadHandle.IsValid() && ReverbLoadHandle->IsLoadingInProgress()) return nullptr;   // 还在载：下次再试
		bReverbTried = true;
		const FSoftObjectPath& P = S->ReverbSubmix.ToSoftObjectPath();
		if (P.IsValid() && FPackageName::DoesPackageExist(P.GetLongPackageName()))
		{
			Submix = S->ReverbSubmix.LoadSynchronous();
		}
		if (!Submix)
		{
			UE_LOG(LogTemp, Warning, TEXT("DysisSfx: 殿内混响还没生成（%s）——声音照常播，只是不带混响；跑一次 Art/Audio/ue_import_sfx.py 就有了"),
				*P.ToString());
			return nullptr;
		}
	}
	bReverbTried = true;
	ReverbSubmix = Submix;

	// 接进这个世界的音频设备（已经接过的会被忽略/刷新）。
	if (UWorld* World = GetWorld())
	{
		if (FAudioDevice* Device = World->GetAudioDeviceRaw())
		{
			Device->RegisterSoundSubmix(Submix, true);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("DysisSfx: 殿内混响就绪（%s）"), *Submix->GetPathName());
	return ReverbSubmix;
}

float UDysisSfxSubsystem::ReverbSendFor(const FDysisSfxEvent& Event, const FVector* Location, float Indoor01) const
{
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	const float Room = FMath::Lerp(S->OutdoorReverbAmount, S->ReverbAmount, FMath::Clamp(Indoor01, 0.0f, 1.0f));
	float Send = FMath::Max(Event.ReverbSend, 0.0f) * FMath::Max(Room, 0.0f);
	if (Send <= 0.0f) return 0.0f;

	// 远处的声音多带一点混响：干声随距离变小，混响在殿里到处差不多一样响。
	if (Location && !Event.b2D && S->ReverbDistanceBoost > 0.0f)
	{
		if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			const float Dist = static_cast<float>(FVector::Dist(Cam->GetCameraLocation(), *Location));
			const float Far = FMath::Clamp((Dist - Event.InnerRadiusCm) / FMath::Max(Event.FalloffDistanceCm, 1.0f), 0.0f, 1.0f);
			Send *= 1.0f + S->ReverbDistanceBoost * Far;
		}
	}
	return FMath::Min(Send, 4.0f);
}

void UDysisSfxSubsystem::ApplyReverb(UAudioComponent* Comp, const FDysisSfxEvent& Event, const FVector* Location, float Indoor01)
{
	if (!Comp) return;
	const float Send = ReverbSendFor(Event, Location, Indoor01);
	if (Send <= 0.0f) return;
	if (USoundSubmixBase* Submix = GetReverbSubmix())
	{
		Comp->SetSubmixSend(Submix, Send);
	}
}

// ───────────────────────── Tick ─────────────────────────

void UDysisSfxSubsystem::Tick(float DeltaTime)
{
	// 到点的延迟播放。
	if (Pending.Num() > 0)
	{
		const double T = Now();
		TArray<FPending> Due;
		for (int32 i = Pending.Num() - 1; i >= 0; --i)
		{
			if (Pending[i].At <= T)
			{
				Due.Add(Pending[i]);
				Pending.RemoveAt(i);
			}
		}
		for (int32 i = Due.Num() - 1; i >= 0; --i)
		{
			const FPending& P = Due[i];
			Play(P.Key, P.Location, P.Variant, P.VolumeScale, P.PitchScale);
		}
	}

	// 循环：淡入淡出 + 每帧按设置里的音量/快慢刷新（滑块改了立刻听到）。
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	TArray<FName> Dead;
	for (TPair<FName, FLoop>& Pair : Loops)
	{
		FLoop& L = Pair.Value;
		UAudioComponent* C = L.Comp.Get();
		if (!C)
		{
			Dead.Add(Pair.Key);
			continue;
		}
		L.Scale = FMath::FInterpConstantTo(L.Scale, L.Target, DeltaTime, L.Rate);
		if (L.bStopping && L.Scale <= 0.001f)
		{
			C->Stop();
			Dead.Add(Pair.Key);
			continue;
		}
		const FDysisSfxEvent* E = S->FindEvent(L.Key);
		const float Base = (E && E->bEnabled) ? BaseVolume(*E) : 0.0f;
		C->SetVolumeMultiplier(FMath::Max(Base * L.Scale, 0.0001f));
		C->SetPitchMultiplier(FMath::Clamp((E ? E->Pitch : 1.0f) * L.PitchScale, 0.1f, 4.0f));

		// 混响跟着殿内外、远近、滑块变（变化够大才重发）。
		if (E)
		{
			const FVector Here = C->GetComponentLocation();
			const float Send = ReverbSendFor(*E, E->b2D ? nullptr : &Here, ReverbIndoor);
			if (FMath::Abs(Send - L.LastSend) > 0.01f)
			{
				if (USoundSubmixBase* Submix = GetReverbSubmix())
				{
					C->SetSubmixSend(Submix, Send);
					L.LastSend = Send;
				}
			}
		}
	}
	for (const FName& Id : Dead)
	{
		if (const FLoop* L = Loops.Find(Id)) LoopRefs.Remove(L->Comp.Get());
		Loops.Remove(Id);
	}
	LoopRefs.RemoveAll([](const TObjectPtr<UAudioComponent>& C) { return C == nullptr; });
}

void UDysisSfxSubsystem::OnSettingsEdited(FName Key)
{
	// 衰减参数可能变了：下次播时重建。
	if (!Key.IsNone()) AttenuationParams.Remove(Key);
	else AttenuationParams.Reset();

	// PIE 里拖滑块 → 试听这一项（循环项不用试听：它正在响的话已经在变了）。
	const UDysisSfxSettings* S = UDysisSfxSettings::Get();
	if (!Key.IsNone() && S->bAuditionOnEdit)
	{
		if (const FDysisSfxEvent* E = S->FindEvent(Key))
		{
			bool bLoopRunning = false;
			for (const TPair<FName, FLoop>& Pair : Loops)
			{
				if (Pair.Value.Key == Key && Pair.Value.Comp.IsValid()) { bLoopRunning = true; break; }
			}
			if (!bLoopRunning && !E->bLoop) PlayAudition(Key);
		}
	}
}

int32 UDysisSfxSubsystem::CheckAllAssets(TArray<FString>* OutMissing)
{
	int32 Missing = 0, Total = 0;
	for (const FDysisSfxEvent& E : UDysisSfxSettings::Get()->Events)
	{
		if (E.Sounds.Num() == 0)
		{
			++Missing;
			const FString Line = FString::Printf(TEXT("%s：没有声音文件"), *E.Key.ToString());
			UE_LOG(LogTemp, Warning, TEXT("DysisSfx Check: %s"), *Line);
			if (OutMissing) OutMissing->Add(Line);
		}
		for (const TSoftObjectPtr<USoundBase>& Snd : E.Sounds)
		{
			++Total;
			USoundBase* Sound = Snd.LoadSynchronous();
			if (!Sound)
			{
				++Missing;
				const FString Line = FString::Printf(TEXT("%s：%s"), *E.Key.ToString(), *Snd.ToString());
				UE_LOG(LogTemp, Warning, TEXT("DysisSfx Check: 缺 %s"), *Line);
				if (OutMissing) OutMissing->Add(Line);
			}
			else
			{
				Loaded.AddUnique(Sound);
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("DysisSfx Check: %d 个声音文件，缺 %d 个"), Total, Missing);
	return Missing;
}

// ───────────────────────── 蓝图库 ─────────────────────────

void UDysisSfxLibrary::PlayDysisSfx(const UObject* WorldContextObject, FName Key, FVector Location, int32 Variant, float VolumeScale, float PitchScale)
{
	if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(WorldContextObject)) Sfx->Play(Key, Location, Variant, VolumeScale, PitchScale);
}

void UDysisSfxLibrary::PlayDysisUISound(const UObject* WorldContextObject, EDysisUISound Sound)
{
	static const FName Keys[] = {
		FName(TEXT("UI.Hover")), FName(TEXT("UI.Confirm")), FName(TEXT("UI.Back")), FName(TEXT("UI.StartGame")),
		FName(TEXT("UI.ShardHover.Sun")), FName(TEXT("UI.ShardHover.Moon")), FName(TEXT("UI.ShardHover.Rainbow")) };
	const int32 Idx = static_cast<int32>(Sound);
	if (Idx < 0 || Idx >= static_cast<int32>(UE_ARRAY_COUNT(Keys))) return;
	if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(WorldContextObject)) Sfx->Play(Keys[Idx], FVector::ZeroVector);
}

void UDysisSfxLibrary::StartDysisSfxLoop(const UObject* WorldContextObject, FName LoopId, FName Key, FVector Location, float FadeInSeconds)
{
	if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(WorldContextObject)) Sfx->StartLoop(LoopId, Key, Location, FadeInSeconds);
}

void UDysisSfxLibrary::StopDysisSfxLoop(const UObject* WorldContextObject, FName LoopId, float FadeOutSeconds)
{
	if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(WorldContextObject)) Sfx->StopLoop(LoopId, FadeOutSeconds);
}

// ───────────────────────── 控制台 ─────────────────────────

namespace DysisSfxConsole
{
	static bool SetCategoryVolume(UDysisSfxSettings* S, const FString& Name, float V)
	{
		if (Name.Equals(TEXT("Master"), ESearchCase::IgnoreCase)) { S->MasterVolume = V; return true; }
		if (Name.Equals(TEXT("Footsteps"), ESearchCase::IgnoreCase)) { S->FootstepsVolume = V; return true; }
		if (Name.Equals(TEXT("Player"), ESearchCase::IgnoreCase)) { S->PlayerVolume = V; return true; }
		if (Name.Equals(TEXT("Mechanism"), ESearchCase::IgnoreCase)) { S->MechanismVolume = V; return true; }
		if (Name.Equals(TEXT("Story"), ESearchCase::IgnoreCase)) { S->StoryVolume = V; return true; }
		if (Name.Equals(TEXT("Ambience"), ESearchCase::IgnoreCase)) { S->AmbienceVolume = V; return true; }
		if (Name.Equals(TEXT("UI"), ESearchCase::IgnoreCase)) { S->UIVolume = V; return true; }
		return false;
	}

	static FDysisSfxEvent* FindMutable(UDysisSfxSettings* S, const FString& Key)
	{
		for (FDysisSfxEvent& E : S->Events)
		{
			if (E.Key.ToString().Equals(Key, ESearchCase::IgnoreCase)) return &E;
		}
		return nullptr;
	}

	static void Play(const TArray<FString>& Args, UWorld* World)
	{
		UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(World);
		if (!Sfx || Args.Num() < 1) { UE_LOG(LogTemp, Warning, TEXT("用法：Dysis.Sfx.Play <事件名> [第几个]（要在 PIE 里）")); return; }
		const int32 Variant = Args.Num() > 1 ? FCString::Atoi(*Args[1]) - 1 : -1;
		if (!Sfx->PlayAudition(FName(*Args[0]), Variant))
			UE_LOG(LogTemp, Warning, TEXT("Dysis.Sfx.Play：%s 没播出来（名字对吗？文件导入了吗？）"), *Args[0]);
	}

	static void List(const TArray<FString>& Args, UWorld*)
	{
		const UDysisSfxSettings* S = UDysisSfxSettings::Get();
		const FString Filter = Args.Num() > 0 ? Args[0] : FString();
		for (const FDysisSfxEvent& E : S->Events)
		{
			if (!Filter.IsEmpty() && !E.Key.ToString().Contains(Filter) && !E.Label.Contains(Filter)) continue;
			UE_LOG(LogTemp, Display, TEXT("%-28s %-30s 音量 %.2f 快慢 %.2f 混响 %.2f %s %d 个文件%s"), *E.Key.ToString(), *E.Label, E.Volume, E.Pitch,
				E.ReverbSend, E.b2D ? TEXT("2D") : TEXT("3D"), E.Sounds.Num(), E.bEnabled ? TEXT("") : TEXT("（关）"));
		}
	}

	static void Volume(const TArray<FString>& Args, UWorld*)
	{
		UDysisSfxSettings* S = UDysisSfxSettings::GetMutable();
		if (Args.Num() < 2) { UE_LOG(LogTemp, Warning, TEXT("用法：Dysis.Sfx.Volume <事件名|Master|Footsteps|Player|Mechanism|Story|Ambience|UI> 0.8")); return; }
		const float V = FMath::Clamp(FCString::Atof(*Args[1]), 0.0f, 4.0f);
		if (SetCategoryVolume(S, Args[0], V)) { UE_LOG(LogTemp, Display, TEXT("%s 音量 = %.2f"), *Args[0], V); return; }
		if (FDysisSfxEvent* E = FindMutable(S, Args[0])) { E->Volume = V; UE_LOG(LogTemp, Display, TEXT("%s 音量 = %.2f"), *Args[0], V); return; }
		UE_LOG(LogTemp, Warning, TEXT("没有 %s 这一项（Dysis.Sfx.List 看全部）"), *Args[0]);
	}

	static void Pitch(const TArray<FString>& Args, UWorld*)
	{
		UDysisSfxSettings* S = UDysisSfxSettings::GetMutable();
		if (Args.Num() < 2) { UE_LOG(LogTemp, Warning, TEXT("用法：Dysis.Sfx.Pitch <事件名> 1.1")); return; }
		if (FDysisSfxEvent* E = FindMutable(S, Args[0]))
		{
			E->Pitch = FMath::Clamp(FCString::Atof(*Args[1]), 0.25f, 4.0f);
			UE_LOG(LogTemp, Display, TEXT("%s 快慢 = %.2f"), *Args[0], E->Pitch);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("没有 %s 这一项"), *Args[0]);
	}

	static void Reverb(const TArray<FString>& Args, UWorld* World)
	{
		UDysisSfxSettings* S = UDysisSfxSettings::GetMutable();
		if (Args.Num() == 1)
		{
			S->ReverbAmount = FMath::Clamp(FCString::Atof(*Args[0]), 0.0f, 3.0f);
		}
		else if (Args.Num() >= 2)
		{
			FDysisSfxEvent* E = FindMutable(S, Args[0]);
			if (!E) { UE_LOG(LogTemp, Warning, TEXT("没有 %s 这一项"), *Args[0]); return; }
			E->ReverbSend = FMath::Clamp(FCString::Atof(*Args[1]), 0.0f, 3.0f);
			UE_LOG(LogTemp, Display, TEXT("%s 混响 = %.2f"), *Args[0], E->ReverbSend);
		}
		UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(World);
		const bool bReady = Sfx && Sfx->GetReverbSubmix();
		UE_LOG(LogTemp, Display, TEXT("殿内混响 %.2f，殿外混响 %.2f，现在殿内程度 %.2f，混响 Submix %s"), S->ReverbAmount, S->OutdoorReverbAmount,
			Sfx ? Sfx->GetReverbEnvironment() : 0.0f, bReady ? TEXT("就绪") : (Sfx ? TEXT("没有（跑一次 ue_import_sfx.py）") : TEXT("（要在 PIE 里看）")));
	}

	static void Enable(const TArray<FString>& Args, UWorld*)
	{
		UDysisSfxSettings* S = UDysisSfxSettings::GetMutable();
		if (Args.Num() < 2) { UE_LOG(LogTemp, Warning, TEXT("用法：Dysis.Sfx.Enable <事件名> 0|1")); return; }
		if (FDysisSfxEvent* E = FindMutable(S, Args[0]))
		{
			E->bEnabled = FCString::Atoi(*Args[1]) != 0;
			UE_LOG(LogTemp, Display, TEXT("%s %s"), *Args[0], E->bEnabled ? TEXT("开") : TEXT("关"));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("没有 %s 这一项"), *Args[0]);
	}

	static void Debug(const TArray<FString>& Args, UWorld* World)
	{
		if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(World))
		{
			const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Sfx->IsDebug();
			Sfx->SetDebug(bOn);
			UE_LOG(LogTemp, Display, TEXT("Dysis.Sfx.Debug %s"), bOn ? TEXT("开") : TEXT("关"));
		}
	}

	static void Check(const TArray<FString>&, UWorld* World)
	{
		if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(World)) Sfx->CheckAllAssets();
		else UE_LOG(LogTemp, Warning, TEXT("Dysis.Sfx.Check 要在 PIE 里跑"));
	}

	static void Save(const TArray<FString>&, UWorld*)
	{
		const bool bOk = UDysisSfxSettings::GetMutable()->SaveToDefaultIni();
		UE_LOG(LogTemp, Display, TEXT("Dysis.Sfx.Save：%s"), bOk ? TEXT("已存进 Config/DefaultGame.ini") : TEXT("存失败（文件只读？）"));
	}

	static void ResetAll(const TArray<FString>&, UWorld*)
	{
		UDysisSfxSettings::GetMutable()->ResetToFactory();
		UE_LOG(LogTemp, Display, TEXT("Dysis.Sfx.ResetAll：已恢复出厂值（要存进 ini 再输 Dysis.Sfx.Save）"));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdPlay(TEXT("Dysis.Sfx.Play"), TEXT("Dysis.Sfx.Play <事件名> [第几个]  在耳边试听"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Play));
	static FAutoConsoleCommandWithWorldAndArgs CmdList(TEXT("Dysis.Sfx.List"), TEXT("Dysis.Sfx.List [过滤]  列出所有音效项"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&List));
	static FAutoConsoleCommandWithWorldAndArgs CmdVolume(TEXT("Dysis.Sfx.Volume"), TEXT("Dysis.Sfx.Volume <事件名|分类|Master> 0.8"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Volume));
	static FAutoConsoleCommandWithWorldAndArgs CmdPitch(TEXT("Dysis.Sfx.Pitch"), TEXT("Dysis.Sfx.Pitch <事件名> 1.1  改快慢"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Pitch));
	static FAutoConsoleCommandWithWorldAndArgs CmdReverb(TEXT("Dysis.Sfx.Reverb"), TEXT("Dysis.Sfx.Reverb [事件名] [0.8]  殿内混响：看状态 / 改总量 / 改单项"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Reverb));
	static FAutoConsoleCommandWithWorldAndArgs CmdEnable(TEXT("Dysis.Sfx.Enable"), TEXT("Dysis.Sfx.Enable <事件名> 0|1"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Enable));
	static FAutoConsoleCommandWithWorldAndArgs CmdDebug(TEXT("Dysis.Sfx.Debug"), TEXT("Dysis.Sfx.Debug [0|1]  屏幕上显示每次播了哪一项"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Debug));
	static FAutoConsoleCommandWithWorldAndArgs CmdCheck(TEXT("Dysis.Sfx.Check"), TEXT("检查每个声音文件都导入了"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Check));
	static FAutoConsoleCommandWithWorldAndArgs CmdSave(TEXT("Dysis.Sfx.Save"), TEXT("把当前音效数值存进 Config/DefaultGame.ini"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Save));
	static FAutoConsoleCommandWithWorldAndArgs CmdReset(TEXT("Dysis.Sfx.ResetAll"), TEXT("音效数值恢复出厂（再 Dysis.Sfx.Save 才写 ini）"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResetAll));
}
