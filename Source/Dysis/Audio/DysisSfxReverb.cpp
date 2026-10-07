// 狄西斯的日落回廊 · 殿内卷积混响（编辑器里一键生成）
// ue_import_sfx.py 调 UDysisSfxLibrary::BuildTempleReverb：读 Art/Audio/ir/IR_Temple_Rotunda.wav（圆殿冲激响应，5 秒立体声），
// 在 /Game/Dysis/Audio/Reverb 下生成三个资产（已有就更新）：
//   IR_Temple_Rotunda        冲激响应（Synthesis 插件的 AudioImpulseResponse）
//   SubmixFX_TempleReverb    卷积混响效果（只出湿声）
//   Submix_TempleReverb      混响 Submix（挂上面的效果；每个声音按“混响多少”送一路过来）
// 音量对齐预览网页：网页里的卷积器按 IR 的能量自动归一（WebAudio normalize），再乘 0.42 的湿声。
// 这里把同样的倍数直接乘进 IR 采样，UE 那边归一化 0 dB、湿声 0 dB、干声 −96 dB，于是送 1.0 = 预览版的混响量。
// Synthesis 的类用反射找（编译期不依赖插件）：插件没开、或引擎改了名字，只会返回“失败：……”，不影响工程编译和其他音效。
#include "Audio/DysisSfxSubsystem.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundSubmix.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace DysisSfxReverbBuild
{
	const TCHAR* const ImpulseResponseClassPath = TEXT("/Script/Synthesis.AudioImpulseResponse");
	const TCHAR* const ConvolutionPresetClassPath = TEXT("/Script/Synthesis.SubmixEffectConvolutionReverbPreset");

	struct FWav
	{
		int32 NumChannels = 0;
		int32 SampleRate = 0;
		TArray<float> Samples;   // 交错存放（和 AudioImpulseResponse 一样）
	};

	static uint16 ReadU16(const uint8* P) { return static_cast<uint16>(P[0] | (P[1] << 8)); }
	static uint32 ReadU32(const uint8* P) { return uint32(P[0]) | (uint32(P[1]) << 8) | (uint32(P[2]) << 16) | (uint32(P[3]) << 24); }

	/** 读 wav：16/24/32 位整数、32 位浮点（含 WAVE_FORMAT_EXTENSIBLE）。 */
	static bool ParseWav(const TArray<uint8>& Bytes, FWav& Out, FString& Error)
	{
		const int32 Size = Bytes.Num();
		const uint8* D = Bytes.GetData();
		if (Size < 12 || FMemory::Memcmp(D, "RIFF", 4) != 0 || FMemory::Memcmp(D + 8, "WAVE", 4) != 0)
		{
			Error = TEXT("不是 wav 文件（是 Git LFS 指针的话，先 git lfs pull）");
			return false;
		}

		int32 Format = 0, Bits = 0, BlockAlign = 0, DataBytes = 0;
		const uint8* Data = nullptr;
		int32 Pos = 12;
		while (Pos + 8 <= Size)
		{
			const uint8* Chunk = D + Pos;
			const int32 Body = Pos + 8;
			const int32 Len = static_cast<int32>(FMath::Min<int64>(ReadU32(Chunk + 4), Size - Body));
			if (FMemory::Memcmp(Chunk, "fmt ", 4) == 0 && Len >= 16)
			{
				Format = ReadU16(D + Body);
				Out.NumChannels = ReadU16(D + Body + 2);
				Out.SampleRate = static_cast<int32>(ReadU32(D + Body + 4));
				BlockAlign = ReadU16(D + Body + 12);
				Bits = ReadU16(D + Body + 14);
				if (Format == 0xFFFE && Len >= 26) Format = ReadU16(D + Body + 24);   // EXTENSIBLE：子格式 GUID 的头两个字节
			}
			else if (FMemory::Memcmp(Chunk, "data", 4) == 0)
			{
				Data = D + Body;
				DataBytes = Len;
			}
			Pos = Body + Len + (Len & 1);
		}
		if (!Data || Out.NumChannels <= 0 || Out.SampleRate <= 0 || BlockAlign <= 0)
		{
			Error = TEXT("wav 里没找到 fmt / data 块");
			return false;
		}

		const int32 BytesPer = Bits / 8;
		const bool bFloat = Format == 3 && Bits == 32;
		const bool bPcm = Format == 1 && (Bits == 16 || Bits == 24 || Bits == 32);
		if ((!bFloat && !bPcm) || BlockAlign < Out.NumChannels * BytesPer)
		{
			Error = FString::Printf(TEXT("不支持的 wav 格式（格式 %d，%d 位）——存成 16/24 位 PCM 或 32 位浮点"), Format, Bits);
			return false;
		}
		const int32 Frames = DataBytes / BlockAlign;
		if (Frames <= 0)
		{
			Error = TEXT("wav 是空的");
			return false;
		}

		Out.Samples.SetNumUninitialized(Frames * Out.NumChannels);
		for (int32 F = 0; F < Frames; ++F)
		{
			const uint8* Frame = Data + static_cast<int64>(F) * BlockAlign;
			for (int32 C = 0; C < Out.NumChannels; ++C)
			{
				const uint8* P = Frame + C * BytesPer;
				float V = 0.0f;
				if (bFloat)
				{
					const uint32 U = ReadU32(P);
					FMemory::Memcpy(&V, &U, sizeof(V));
				}
				else if (Bits == 16)
				{
					V = static_cast<float>(static_cast<int16>(ReadU16(P))) / 32768.0f;
				}
				else if (Bits == 24)
				{
					const int32 S = static_cast<int32>((uint32(P[0]) << 8) | (uint32(P[1]) << 16) | (uint32(P[2]) << 24)) >> 8;
					V = static_cast<float>(S) / 8388608.0f;
				}
				else
				{
					V = static_cast<float>(static_cast<double>(static_cast<int32>(ReadU32(P))) / 2147483648.0);
				}
				Out.Samples[F * Out.NumChannels + C] = V;
			}
		}
		return true;
	}

	/** WebAudio ConvolverNode（normalize = true）给 IR 乘的倍数——预览网页就是这么听的（Chromium Reverb.cpp 同款算法）。 */
	static double WebAudioNormalizeScale(const FWav& Wav)
	{
		constexpr double GainCalibration = 0.0012589254117941673;   // 10^(−58/20)
		constexpr double GainCalibrationSampleRate = 44100.0;
		constexpr double MinPower = 0.000125;

		double Power = 0.0;
		for (const float V : Wav.Samples) Power += static_cast<double>(V) * static_cast<double>(V);
		double Rms = FMath::Sqrt(Power / static_cast<double>(FMath::Max(Wav.Samples.Num(), 1)));
		if (!(Rms >= MinPower) || Rms > 1.0e30) Rms = MinPower;

		double Scale = GainCalibration / Rms;
		Scale *= GainCalibrationSampleRate / static_cast<double>(Wav.SampleRate);
		if (Wav.NumChannels == 4) Scale *= 0.5;   // 真立体声补偿
		return Scale;
	}

	/** /Game/Dysis/Audio/Reverb/<Name>：有就拿来（类要对），没有就新建。 */
	static UObject* FindOrCreateAsset(UClass* Class, const TCHAR* Name, bool& bOutCreated, FString& Error)
	{
		bOutCreated = false;
		const FString PackageName = FString::Printf(TEXT("%s/%s"), DysisSfxReverb::Folder, Name);
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, Name);

		UObject* Existing = FindObject<UObject>(nullptr, *ObjectPath);
		if (!Existing && FPackageName::DoesPackageExist(PackageName))
		{
			Existing = LoadObject<UObject>(nullptr, *ObjectPath);
		}
		if (Existing)
		{
			if (!Existing->IsA(Class))
			{
				Error = FString::Printf(TEXT("%s 已经存在、但不是 %s（删掉或改名后再跑）"), *ObjectPath, *Class->GetName());
				return nullptr;
			}
			Existing->Modify();
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackageName);
		UObject* Created = Package ? NewObject<UObject>(Package, Class, FName(Name), RF_Public | RF_Standalone | RF_Transactional) : nullptr;
		if (!Created)
		{
			Error = FString::Printf(TEXT("建不了 %s"), *ObjectPath);
			return nullptr;
		}
		bOutCreated = true;
		return Created;
	}

	static bool SetIntProp(UObject* Obj, const TCHAR* Name, int32 Value)
	{
		FIntProperty* P = FindFProperty<FIntProperty>(Obj->GetClass(), Name);
		if (P) P->SetPropertyValue_InContainer(Obj, Value);
		return P != nullptr;
	}

	static bool SetFloatProp(void* Container, const UStruct* Struct, const TCHAR* Name, float Value)
	{
		FFloatProperty* P = FindFProperty<FFloatProperty>(Struct, Name);
		if (P) P->SetPropertyValue_InContainer(Container, Value);
		return P != nullptr;
	}

	static bool SetBoolProp(void* Container, const UStruct* Struct, const TCHAR* Name, bool bValue)
	{
		FBoolProperty* P = FindFProperty<FBoolProperty>(Struct, Name);
		if (P) P->SetPropertyValue_InContainer(Container, bValue);
		return P != nullptr;
	}

	static bool SetObjectProp(UObject* Obj, const TCHAR* Name, UObject* Value)
	{
		FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(Obj->GetClass(), Name);
		if (P) P->SetObjectPropertyValue_InContainer(Obj, Value);
		return P != nullptr;
	}

	static bool SetFloatArrayProp(UObject* Obj, const TCHAR* Name, const TArray<float>& Values)
	{
		FArrayProperty* P = FindFProperty<FArrayProperty>(Obj->GetClass(), Name);
		if (!P || !CastField<FFloatProperty>(P->Inner)) return false;
		FScriptArrayHelper Helper(P, P->ContainerPtrToValuePtr<void>(Obj));
		Helper.Resize(Values.Num());
		if (Values.Num() > 0) FMemory::Memcpy(Helper.GetRawPtr(0), Values.GetData(), sizeof(float) * Values.Num());
		return true;
	}

	/** 调一个只有一个对象参数的 UFUNCTION（SetImpulseResponse）。 */
	static bool CallObjectSetter(UObject* Obj, const TCHAR* FuncName, UObject* Arg)
	{
		UFunction* Func = Obj->FindFunction(FName(FuncName));
		if (!Func || Func->ParmsSize == 0) return false;
		TArray<uint8> Params;
		Params.SetNumZeroed(Func->ParmsSize);
		bool bSet = false;
		for (TFieldIterator<FProperty> It(Func); It && !bSet; ++It)
		{
			FObjectPropertyBase* P = CastField<FObjectPropertyBase>(*It);
			if (P && P->HasAnyPropertyFlags(CPF_Parm) && !P->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				P->SetObjectPropertyValue_InContainer(Params.GetData(), Arg);
				bSet = true;
			}
		}
		if (bSet) Obj->ProcessEvent(Func, Params.GetData());
		return bSet;
	}
}
#endif

FString UDysisSfxLibrary::BuildTempleReverb(const FString& IrWavFile)
{
#if WITH_EDITOR
	namespace R = DysisSfxReverbBuild;

	UClass* IrClass = FindObject<UClass>(nullptr, R::ImpulseResponseClassPath);
	UClass* PresetClass = FindObject<UClass>(nullptr, R::ConvolutionPresetClassPath);
	if (!IrClass || !PresetClass || !PresetClass->IsChildOf(USoundEffectSubmixPreset::StaticClass()))
	{
		return TEXT("失败：没找到 Synthesis 插件的卷积混响（Dysis.uproject 里已经加了 Synthesis：重启编辑器、编译后再跑一次）");
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *IrWavFile))
	{
		return FString::Printf(TEXT("失败：读不了 %s"), *IrWavFile);
	}
	R::FWav Wav;
	FString Error;
	if (!R::ParseWav(Bytes, Wav, Error))
	{
		return FString::Printf(TEXT("失败：%s（%s）"), *Error, *IrWavFile);
	}
	if (Wav.NumChannels != 1 && Wav.NumChannels != 2 && Wav.NumChannels != 4)
	{
		return FString::Printf(TEXT("失败：IR 要 1、2 或 4 声道（这个是 %d 声道）"), Wav.NumChannels);
	}

	// 音量对齐预览版：WebAudio 的归一化 × 湿声 0.42，直接乘进采样。
	const float Gain = static_cast<float>(R::WebAudioNormalizeScale(Wav) * static_cast<double>(DysisSfxReverb::PreviewWetGain));
	for (float& V : Wav.Samples) V *= Gain;
	FString Notes;

	// 1) 冲激响应
	bool bNewIr = false;
	UObject* Ir = R::FindOrCreateAsset(IrClass, DysisSfxReverb::ImpulseResponseName, bNewIr, Error);
	if (!Ir) return TEXT("失败：") + Error;
	const bool bIrOk = R::SetFloatArrayProp(Ir, TEXT("ImpulseResponse"), Wav.Samples)
		&& R::SetIntProp(Ir, TEXT("NumChannels"), Wav.NumChannels)
		&& R::SetIntProp(Ir, TEXT("SampleRate"), Wav.SampleRate);
	if (!bIrOk)
	{
		return TEXT("失败：AudioImpulseResponse 的属性名对不上（引擎版本改了？）——按 Art/Audio/README.md 的“混响”一节手动建");
	}
	if (!R::SetFloatProp(Ir, IrClass, TEXT("NormalizationVolumeDb"), 0.0f)) Notes += TEXT("；IR 的归一化没设上（手动设成 0 dB）");
	R::SetBoolProp(Ir, IrClass, TEXT("bTrueStereo"), Wav.NumChannels == 4);
	Ir->PostEditChange();

	// 2) 卷积混响效果：只出湿声（干声由每个声音自己的正常输出负责）。
	bool bNewPreset = false;
	USoundEffectSubmixPreset* Preset = Cast<USoundEffectSubmixPreset>(R::FindOrCreateAsset(PresetClass, DysisSfxReverb::PresetName, bNewPreset, Error));
	if (!Preset) return TEXT("失败：") + (Error.IsEmpty() ? FString(TEXT("建不了卷积混响效果")) : Error);
	if (!R::CallObjectSetter(Preset, TEXT("SetImpulseResponse"), Ir) && !R::SetObjectProp(Preset, TEXT("ImpulseResponse"), Ir))
	{
		return TEXT("失败：卷积混响效果上找不到 ImpulseResponse（引擎版本改了？）——按 README 手动建");
	}
	FStructProperty* SettingsProp = FindFProperty<FStructProperty>(PresetClass, TEXT("Settings"));
	void* Settings = SettingsProp ? SettingsProp->ContainerPtrToValuePtr<void>(Preset) : nullptr;
	const bool bLevelsOk = Settings
		&& R::SetFloatProp(Settings, SettingsProp->Struct, TEXT("WetVolumeDb"), 0.0f)
		&& R::SetFloatProp(Settings, SettingsProp->Struct, TEXT("DryVolumeDb"), -96.0f);
	if (Settings) R::SetBoolProp(Settings, SettingsProp->Struct, TEXT("bBypass"), false);
	if (!bLevelsOk) Notes += TEXT("；效果的湿声/干声没设上（手动设：Wet 0 dB、Dry −96 dB）");
	Preset->PostEditChange();

	// 3) Submix：父级留空 = 直接进主输出。
	bool bNewSubmix = false;
	USoundSubmix* Submix = Cast<USoundSubmix>(R::FindOrCreateAsset(USoundSubmix::StaticClass(), DysisSfxReverb::SubmixName, bNewSubmix, Error));
	if (!Submix) return TEXT("失败：") + (Error.IsEmpty() ? FString(TEXT("建不了混响 Submix")) : Error);
	Submix->SubmixEffectChain.Reset();
	Submix->SubmixEffectChain.Add(Preset);
	Submix->PostEditChange();

	auto Finish = [](UObject* Obj, bool bNew)
	{
		Obj->MarkPackageDirty();
		if (bNew) FAssetRegistryModule::AssetCreated(Obj);
	};
	Finish(Ir, bNewIr);
	Finish(Preset, bNewPreset);
	Finish(Submix, bNewSubmix);

	const float Seconds = static_cast<float>(Wav.Samples.Num() / Wav.NumChannels) / static_cast<float>(Wav.SampleRate);
	return FString::Printf(TEXT("成功：%s（IR %.2f 秒、%d 声道、%d Hz，乘 %.3f 对齐预览版）%s"), *Submix->GetPathName(), Seconds,
		Wav.NumChannels, Wav.SampleRate, Gain, *Notes);
#else
	(void)IrWavFile;
	return TEXT("失败：只能在编辑器里生成混响资产");
#endif
}
