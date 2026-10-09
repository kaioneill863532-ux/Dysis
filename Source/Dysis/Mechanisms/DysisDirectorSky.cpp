// 日落回廊 · 夜里几样“看的”东西：金苹果的光、瀑布上的月虹、结局的星座。都不参与光路和机关的判定。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisConstellations.generated.h"
#include "World/DysisWorldState.h"
#include "Sky/DysisSkyLibrary.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisDialogueComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// 金苹果的光（灰盒 appleLight：暖色的点光，照 6 m）。换算 2.0 是拿灰盒的画面对出来的（夜里的曝光也是那样对的）
	constexpr float SkyAppleGain = 2.0f;
	const FLinearColor SkyAppleWarm = FLinearColor(FColor(0xff, 0xc0, 0x70)), SkyAppleCool = FLinearColor(FColor(0xcf, 0xdc, 0xff));

	// 月虹：立在瀑布前面的一片透明片（瀑布在方位 116.25°–139.75°，水帘在 r = 12.45 m）
	constexpr float BowAz = 128.0f, BowR = 1040.0f;          // 片子立在水帘前面 2 m 的空里（再靠外、再宽就插进回廊的栏杆了）
	constexpr float BowW = 1400.0f, BowH = 760.0f;           // 片子的宽、高
	constexpr float BowBottomZ = -45.0f;                     // 片子的底边 = 水面；虹的圆心就在底边上
	constexpr float BowRadius = 640.0f, BowBand = 42.0f;     // 虹的半径、色带的半宽
	constexpr float BowAltLo0 = 12.0f, BowAltLo1 = 20.0f, BowAltHi0 = 40.0f, BowAltHi1 = 50.0f;   // 月亮“不高不低”：高度 20°–40° 最清楚，12° 以下、50° 以上没有

	// 结局的星座
	constexpr float ConDistCm = 420000.0f;                   // 在月亮圆盘后面、满天的星星前面
	constexpr float ConCamZ = 400.0f, ConCamFov = 94.0f;     // 镜头：殿心正上方、水亭顶上（亭顶 3.7 m），笔直朝天——圆眼在画面正中，占画面高的四分之三
	constexpr float ConSpinDegPerSec = 1.1f;                 // 星座和满天的星一起绕着头顶慢慢转
	constexpr float ConCamSeconds = 3.0f;                    // 镜头抬起来用多久
	constexpr float ConStarsAt = 1.2f, ConStarEach = 0.07f, ConStarFade = 1.2f;   // 星一颗接一颗亮起来
	constexpr float ConLinesAt = 3.4f, ConLineSeconds = 0.36f;                    // 线一条接一条画出来
	constexpr float ConHold = 4.2f;                                              // 画完以后停多久
	constexpr float ConLineWidthDeg = 0.075f, ConLineGapDeg = 0.32f;
	const FLinearColor ConStarColor(0.92f, 0.96f, 1.0f), ConLineColor(0.62f, 0.76f, 1.0f);

	UInstancedStaticMeshComponent* SkyMakeInstances(AActor* Owner, UStaticMesh* Mesh, int32 SortPriority)
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Owner);
		C->SetupAttachment(Owner->GetRootComponent());
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bUseAsOccluder = false;
		C->NumCustomDataFloats = 3;
		C->SetTranslucentSortPriority(SortPriority);
		C->SetStaticMesh(Mesh);
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(FVector::ZeroVector, FQuat::Identity);   // 摆在殿心：里面每颗星的位置就是它在天上的方向 × 距离，整组绕竖轴转就是星空在转
		C->SetVisibility(false);
		return C;
	}
}

void ADysisDirector::SetupSkyFx()
{
	// 金苹果的光
	if (Apple && !AppleLight)
	{
		AppleLight = NewObject<UPointLightComponent>(this);
		AppleLight->SetupAttachment(Apple);
		AppleLight->SetMobility(EComponentMobility::Movable);
		AppleLight->SetIntensityUnits(ELightUnits::Candelas);
		AppleLight->SetAttenuationRadius(600.0f);
		AppleLight->SetSourceRadius(12.0f);
		AppleLight->SetCastShadows(false);
		AppleLight->SetIntensity(0.0f);
		AppleLight->RegisterComponent();
	}
	AppleLightColor = SkyAppleWarm;
	if (AppleLight) AppleLight->SetLightColor(AppleLightColor);

	// 月虹
	if (PlaneMesh && !MoonbowPlane)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Night/M_DysisMoonbow.M_DysisMoonbow")))
		{
			MoonbowPlane = NewObject<UStaticMeshComponent>(this);
			MoonbowPlane->SetupAttachment(RootComponent);
			MoonbowPlane->SetMobility(EComponentMobility::Movable);
			MoonbowPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MoonbowPlane->SetCastShadow(false);
			MoonbowPlane->SetStaticMesh(PlaneMesh);
			MoonbowPlane->RegisterComponent();
			// 片子横着（本地 X）沿墙的方向，竖着（本地 Y）朝下，面朝殿心
			const FVector Out = DysisGB::PolarCm(BowAz, 1.0f, 0.0f).GetSafeNormal(), Along(-Out.Y, Out.X, 0.0);
			MoonbowPlane->SetWorldLocationAndRotation(DysisGB::PolarCm(BowAz, BowR, BowBottomZ + BowH * 0.5f), FRotationMatrix::MakeFromXY(Along, FVector(0.0, 0.0, -1.0)).ToQuat());
			MoonbowPlane->SetWorldScale3D(FVector(BowW / 100.0, BowH / 100.0, 1.0));
			MoonbowMID = UMaterialInstanceDynamic::Create(Mat, this);
			MoonbowMID->SetScalarParameterValue(TEXT("W"), BowW);
			MoonbowMID->SetScalarParameterValue(TEXT("H"), BowH);
			MoonbowMID->SetScalarParameterValue(TEXT("Radius"), BowRadius);
			MoonbowMID->SetScalarParameterValue(TEXT("Band"), BowBand);
			MoonbowMID->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
			MoonbowMID->SetScalarParameterValue(TEXT("Glow"), 0.2f);   // 月虹是淡淡的一道白
			MoonbowPlane->SetMaterial(0, MoonbowMID);
			MoonbowPlane->SetVisibility(false);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Dysis 月虹：找不到材质 /Game/Dysis/Night/M_DysisMoonbow（跑一遍 Art/Night/ue_make_sky_materials.py）"));
		}
	}

	// 结局的星座：星和线各一组小片，先都藏着
	if (PlaneMesh && !ConStars)
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Sky/M_DysisStar.M_DysisStar")))
		{
			ConStars = SkyMakeInstances(this, PlaneMesh, -80);
			ConLines = SkyMakeInstances(this, PlaneMesh, -85);
			ConStarMID = UMaterialInstanceDynamic::Create(Mat, this);
			ConLineMID = UMaterialInstanceDynamic::Create(Mat, this);
			ConLineMID->SetScalarParameterValue(TEXT("Line"), 1.0f);
			ConStars->SetMaterial(0, ConStarMID);
			ConLines->SetMaterial(0, ConLineMID);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Dysis 星座：找不到材质 /Game/Dysis/Sky/M_DysisStar（跑一遍 Art/Night/ue_make_sky_materials.py）"));
		}
	}

	bSeleneTalked = bSeleneTalkStarted = false;
	MoonbowK = 0.0f;
	StarShowT = -1.0f;
	bEndingAll = false;
}

// ───────────────────────── 金苹果的光 ─────────────────────────

void ADysisDirector::UpdateAppleLight(float Dt)
{
	if (!AppleLight) return;
	float Units = 0.0f;
	if (!bCaught) Units = CrownUp > 0.98f ? 6.0f : 0.0f;                    // 屋顶上等着被取下来的时候
	else if (!bApplePlaced) Units = 0.8f;                                    // 夜里拿在手边：照亮身边一小圈
	else
	{
		// 放上月托以后：越来越亮，颜色从暖变成月白
		AppleLightColor = FMath::Lerp(AppleLightColor, SkyAppleCool, FMath::Min(1.0f, Dt));
		Units = 10.0f + 20.0f * DysisGB::Smoothstep(0.5f, 3.0f, FinaleT);
	}
	AppleLight->SetIntensity(Units * SkyAppleGain);
	AppleLight->SetLightColor(AppleLightColor);
}

// ───────────────────────── 月虹 ─────────────────────────

void ADysisDirector::UpdateMoonbow(float Dt)
{
	// 伊莉丝和塞勒涅的那段话：放过、并且放完了才算（“等你升到东边不高不低的地方就来看看瀑布”）
	if (SeleneDialogue)
	{
		if (SeleneDialogue->IsPlaying()) bSeleneTalkStarted = true;
		else if (bSeleneTalkStarted) bSeleneTalked = true;
	}
	float Want = 0.0f;
	const UDysisTimeComponent* Time = PlayerTime();
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	if (bCaught && bSeleneTalked && Time && State && State->FlowK > 0.5f)
	{
		const float Alt = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(UDysisSkyLibrary::DysisMoonDir(Time->H).Z, -1.0, 1.0)));
		Want = DysisGB::Smoothstep(BowAltLo0, BowAltLo1, Alt) * (1.0f - DysisGB::Smoothstep(BowAltHi0, BowAltHi1, Alt));
	}
	MoonbowK = DysisGB::Toward(MoonbowK, Want, 0.45f, Dt);
	if (MoonbowPlane && MoonbowMID)
	{
		MoonbowPlane->SetVisibility(MoonbowK > 0.002f);
		MoonbowMID->SetScalarParameterValue(TEXT("Opacity"), MoonbowK);
	}
}

// ───────────────────────── 结局的星座 ─────────────────────────

void ADysisDirector::StartStarShow()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const UDysisTimeComponent* Time = PlayerTime();
	if (!PC || !Time || !ConStars || !ConLines || StarShowT >= 0.0f) return;
	StarShowT = 0.0f;

	// 镜头：在殿心正上方（水亭顶上）笔直朝天看，圆眼正好在画面正中。画面的上边朝着月亮那一侧，两个星座一左一右摆在圆眼里
	const FVector Moon = UDysisSkyLibrary::DysisMoonDir(Time->H);
	const FVector CamLoc(0.0, 0.0, ConCamZ);
	const FRotator CamRot(89.99f, FMath::RadiansToDegrees(FMath::Atan2(Moon.Y, Moon.X)) + 180.0f, 0.0f);
	if (!EndingCamera)
	{
		EndingCamera = World->SpawnActor<ACameraActor>(CamLoc, CamRot);
		if (UCameraComponent* Cam = EndingCamera ? EndingCamera->GetCameraComponent() : nullptr)
		{
			Cam->bConstrainAspectRatio = false;
			Cam->SetFieldOfView(ConCamFov);
			// 夜里的曝光是定在人的镜头上的，照样搬过来
			if (const APawn* Pawn = PC->GetPawn())
				if (const UCameraComponent* Mine = Pawn->FindComponentByClass<UCameraComponent>())
				{
					Cam->PostProcessSettings = Mine->PostProcessSettings;
					Cam->PostProcessBlendWeight = 1.0f;
				}
		}
	}
	if (EndingCamera)
	{
		EndingCamera->SetActorLocationAndRotation(CamLoc, CamRot);
		PC->SetViewTargetWithBlend(EndingCamera, ConCamSeconds, VTBlend_Cubic);
	}
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);

	// 圆眼从镜头这里看有多大（半张角）
	const UDysisWorldState* State = UDysisWorldState::Get(this);
	const float Aperture = FMath::Max(FMath::Atan2(State ? State->IrisACm : 1100.0f, FMath::Max(100.0f, UDysisSkyLibrary::DysisConst(TEXT("CEIL")) * 100.0f - float(CamLoc.Z))), FMath::DegreesToRadians(10.0f));
	const FRotationMatrix CamM(CamRot);
	const FVector Fwd = CamM.GetUnitAxis(EAxis::X), Right = CamM.GetUnitAxis(EAxis::Y), Up = CamM.GetUnitAxis(EAxis::Z);

	ConStarInfo.Reset(); ConLineInfo.Reset();
	ConStars->ClearInstances(); ConLines->ClearInstances();
	float LineAt = ConLinesAt;
	auto AddConstellation = [&](const FDysisConStar* Stars, int32 NumStars, const FDysisConLine* Lines, int32 NumLines, float Side)
	{
		// 这个星座的中心：偏左或偏右 0.46 个圆眼，再往离月亮远的那边让一点；大小 0.40 个圆眼
		const FVector C = (Fwd + Right * (Side * FMath::Tan(Aperture * 0.46f)) - Up * FMath::Tan(Aperture * 0.10f)).GetSafeNormal();
		const FVector R1 = (Right - C * FVector::DotProduct(Right, C)).GetSafeNormal();
		const FVector U1 = FVector::CrossProduct(C, R1) * (FVector::DotProduct(FVector::CrossProduct(C, R1), Up) < 0.0 ? -1.0 : 1.0);
		const double Spread = FMath::Tan(Aperture * 0.40f);
		const int32 Base = ConStarInfo.Num();
		for (int32 i = 0; i < NumStars; ++i)
		{
			FConStar S;
			S.Dir = (C + (R1 * Stars[i].X + U1 * Stars[i].Y) * Spread).GetSafeNormal();
			const float Bright = 1.0f - FMath::Clamp((Stars[i].Mag - 1.0f) / 3.5f, 0.0f, 1.0f);   // 越亮的星越大、越亮
			S.Gain = FMath::Lerp(0.9f, 2.4f, Bright);
			S.At = ConStarsAt + ConStarEach * ConStarInfo.Num();
			const double Size = 2.0 * ConDistCm * FMath::Tan(FMath::DegreesToRadians(FMath::Lerp(0.42f, 0.95f, Bright) * 0.5f));
			ConStars->AddInstance(FTransform(FRotationMatrix::MakeFromZ(-S.Dir).Rotator(), S.Dir * ConDistCm, FVector(Size / 100.0)), /*bWorldSpace*/ false);
			ConStarInfo.Add(S);
		}
		for (int32 i = 0; i < NumLines; ++i)
		{
			FConLine L;
			L.A = Base + Lines[i].A; L.B = Base + Lines[i].B;
			L.At = LineAt; LineAt += ConLineSeconds;
			ConStarInfo[L.B].PulseAt = FMath::Min(ConStarInfo[L.B].PulseAt, L.At + ConLineSeconds);
			ConLines->AddInstance(FTransform(FQuat::Identity, ConStarInfo[L.A].Dir * ConDistCm, FVector::ZeroVector), /*bWorldSpace*/ false);
			ConLineInfo.Add(L);
		}
	};
	AddConstellation(GDysisGeminiStars, UE_ARRAY_COUNT(GDysisGeminiStars), GDysisGeminiLines, UE_ARRAY_COUNT(GDysisGeminiLines), -1.0f);
	AddConstellation(GDysisCygnusStars, UE_ARRAY_COUNT(GDysisCygnusStars), GDysisCygnusLines, UE_ARRAY_COUNT(GDysisCygnusLines), 1.0f);
	StarShowEnd = LineAt + ConHold;
	ConStars->SetVisibility(true);
	ConLines->SetVisibility(true);
	UpdateEndingStars(0.0f);
}

void ADysisDirector::UpdateEndingStars(float Dt)
{
	if (StarShowT < 0.0f || !ConStars || !ConLines) return;
	StarShowT += Dt;
	const float T = StarShowT;
	for (int32 i = 0; i < ConStarInfo.Num(); ++i)
	{
		const FConStar& S = ConStarInfo[i];
		// 亮起来；线画到它的时候闪一下；之后一直轻轻地眨
		float B = S.Gain * DysisGB::Smoothstep(0.0f, ConStarFade, T - S.At);
		if (T > S.PulseAt) B *= 1.0f + 1.4f * FMath::Exp(-(T - S.PulseAt) * 2.6f);
		B *= 1.0f + 0.10f * FMath::Sin(T * 1.9f + i * 1.7f);
		const float Data[3] = { ConStarColor.R * B, ConStarColor.G * B, ConStarColor.B * B };
		ConStars->SetCustomData(i, MakeArrayView(Data, 3), i == ConStarInfo.Num() - 1);
	}
	const double Width = 2.0 * ConDistCm * FMath::Tan(FMath::DegreesToRadians(ConLineWidthDeg * 0.5f));
	const double Gap = ConDistCm * FMath::Tan(FMath::DegreesToRadians(ConLineGapDeg));
	for (int32 i = 0; i < ConLineInfo.Num(); ++i)
	{
		const FConLine& L = ConLineInfo[i];
		const float P = DysisGB::Smoothstep(0.0f, 1.0f, (T - L.At) / ConLineSeconds);
		const FVector A = ConStarInfo[L.A].Dir * ConDistCm, B = ConStarInfo[L.B].Dir * ConDistCm, Dir = (B - A).GetSafeNormal();
		const FVector From = A + Dir * Gap, To = FMath::Lerp(From, B - Dir * Gap, double(P)), Mid = (From + To) * 0.5;
		const double Len = FVector::Dist(From, To);
		// 线：一条细长的片，本地 X 沿着线，面朝天空中心；从第一颗星朝第二颗星长出去
		const FTransform Xf(FRotationMatrix::MakeFromXZ(Dir, -Mid).Rotator(), Mid, P > 0.001f ? FVector(Len / 100.0, Width / 100.0, 1.0) : FVector::ZeroVector);
		ConLines->UpdateInstanceTransform(i, Xf, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ i == ConLineInfo.Num() - 1, /*bTeleport*/ true);
		const float G = 0.55f * (1.0f + 0.08f * FMath::Sin(T * 1.3f + i));
		const float Data[3] = { ConLineColor.R * G, ConLineColor.G * G, ConLineColor.B * G };
		ConLines->SetCustomData(i, MakeArrayView(Data, 3), i == ConLineInfo.Num() - 1);
	}
	// 星座和满天的星一起绕着头顶慢慢转（开头一两秒慢慢转起来）
	{
		const float Spin = ConSpinDegPerSec * (T - 1.5f * (1.0f - FMath::Exp(-T / 1.5f)));
		const FQuat Rot(FVector::UpVector, FMath::DegreesToRadians(Spin));
		ConStars->SetWorldRotation(Rot);
		ConLines->SetWorldRotation(Rot);
		if (const UDysisTimeComponent* Time = PlayerTime())
			if (ADysisSkyActor* Sky = Time->SkyActor.Get()) Sky->SetStarSpin(Spin);
	}
	// 亮度的换算和满天的星星用同一个数（ADysisSkyActor::StarGain）
	const float Glow = 6.0f;
	if (ConStarMID) { ConStarMID->SetScalarParameterValue(TEXT("Glow"), Glow); ConStarMID->SetScalarParameterValue(TEXT("Opacity"), 1.0f); }
	if (ConLineMID) { ConLineMID->SetScalarParameterValue(TEXT("Glow"), Glow); ConLineMID->SetScalarParameterValue(TEXT("Opacity"), 1.0f); }
}

bool ADysisDirector::StarShowDone() const
{
	return StarShowT >= StarShowEnd;
}

// ───────────────────────── 测试用 ─────────────────────────

void ADysisDirector::DebugSetSeleneTalked(bool bTalked)
{
	bSeleneTalked = bSeleneTalkStarted = bTalked;
}

void ADysisDirector::DebugPlayEnding(bool bAllShards)
{
	bCaught = true;
	bSunNicheOpen = bIrisNicheOpen = bMoonShard = bAllShards;
	bApplePlaced = true;
	bEndingStarted = bEndingDone = false;
	FinaleT = 3.5f;
}

FString ADysisDirector::DescribeSkyFx() const
{
	return FString::Printf(TEXT("{\"seleneTalked\":%s,\"moonbow\":%.3f,\"moonbowShown\":%s,\"appleLight\":%.2f,\"starShow\":%.2f,\"starShowEnd\":%.2f,\"conStars\":%d,\"conLines\":%d,\"endingAll\":%s}"),
		bSeleneTalked ? TEXT("true") : TEXT("false"), MoonbowK, MoonbowPlane && MoonbowPlane->IsVisible() ? TEXT("true") : TEXT("false"),
		AppleLight ? AppleLight->Intensity : -1.0f, StarShowT, StarShowEnd, ConStarInfo.Num(), ConLineInfo.Num(), bEndingAll ? TEXT("true") : TEXT("false"));
}
