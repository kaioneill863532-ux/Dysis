// 日落回廊 · 殿外的配景和夕照的配色（场景美术 2026-10-09 的外景 ReferenceMatch_20261009）。全是看的东西，不参与光路和机关的判定。
//   · 山、石、草坡、云 61 件 + 山花上的日月徽：模型和调色板材质是美术的（/Game/Dysis/ReferenceMatch_20261009、LowPoly_v001），
//     摆放清单在 DysisSceneryData.generated.h（由 Art/Scenery/gen_scenery_data.py 从他的清单生成）。
//   · 建筑（低模）换上他那版的配色：暖白、沙色、赭石；山花深一档，各层地面偏灰粉；台地顶上是沙地、外圈是草、朝小岛那边留一条路。
// 和美术那版不一样的地方——我们的世界是灰盒 v0.12 的：神殿立在海里的台地上，开局在台地下面 12 m 的小岛上，日月是真的在走：
//   1. 他那版把海关了、铺了一圈陆地（地形壳）。陆地会把小岛和开场那道光埋掉，所以地形壳没搬。
//      台地上放得下的石头和草坡照他的位置放；台地外面的石头沉到海里当礁石，草坡不要；小岛和它通到台地的那一条线上不放东西。
//   2. 他那版的夕照是另放的一盏主光，和真的太阳不在一个方向，远山正好挡在我们的太阳落下去的那一边。
//      我们的光路、影子都靠真的太阳，所以远山、巨岩和云整体绕殿心转了 SceneryFarTurnDeg，把日落和月出的方向让出来。
//      他的六朵云只摆在一小段天边（给那台固定镜头看的）；这里同一组云在西北、正北再各摆一份，抬高放大了一些——
//      都避开太阳整个下午走的那一段天、月亮刚升起来的那一段天和主界面上日月转的那一片。
//   3. 都不挡光。台地上的石头挡人、挡镜头；草坡、礁石、山、云什么都不挡。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "DysisSceneryData.generated.h"
#include "Sky/DysisSkyActor.h"
#include "Sky/DysisTimeComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const TCHAR* ScnRoot = TEXT("/Game/Dysis/ReferenceMatch_20261009");
	// 台地（SM_Terrain 的顶面）：圆心不在殿心，朝背着小岛的方向偏了 7 m；半径 39 m
	const FVector2D ScnTerraceC(-647.0, -268.0);
	constexpr float ScnTerraceR = 3906.0f;
	// 小岛（顶面的圆心在方位 23.7°、离殿心 52.6 m）：从殿心朝它去的这一条线两边各 ScnLaneHalfW 以内不放东西（开场的光从这里伸到岛上）
	const FVector2D ScnIsleC(4815.0, 2114.0);
	constexpr float ScnLaneHalfW = 1100.0f;
	// 石头的模型大约是半径 85 cm、半高 93 cm 的一块（没缩放时）
	constexpr float ScnRockR = 85.0f, ScnRockH = 93.0f;
	// 那一组云再多摆几处：各自在 SceneryFarTurnDeg 之外再转多少度（0 = 美术那一组，转完在东南的山上面；150、200 = 西北、正北的海上）
	const float ScnCloudTurns[] = { 0.0f, 150.0f, 200.0f };

	UMaterialInterface* ScnPalette(const FString& Name)
	{
		const FString Path = FString::Printf(TEXT("%s/Materials/%s.%s"), ScnRoot, *Name, *Name);
		return LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

void ADysisDirector::SetupScenery()
{
	for (UStaticMeshComponent* C : SceneryComps) if (C) C->DestroyComponent();
	SceneryComps.Reset();
	SceneryOnTerrace = ScenerySeaRocks = ScenerySkipped = ScenerySolid = 0;

	static const TCHAR* MeshNames[] = { TEXT("SM_MATCH_RockA"), TEXT("SM_MATCH_RockB"), TEXT("SM_MATCH_Mountain"), TEXT("SM_MATCH_Cloud") };
	UStaticMesh* Meshes[UE_ARRAY_COUNT(MeshNames)] = {};
	for (int32 i = 0; i < UE_ARRAY_COUNT(MeshNames); ++i)
		Meshes[i] = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/Meshes/%s.%s"), ScnRoot, MeshNames[i], MeshNames[i]), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Meshes[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("Dysis 配景：找不到 %s 下的模型，殿外的山石云没放"), ScnRoot);
		return;
	}
	// 云：三个面组各一档颜色，所有的云共用这三份材质（颜色每帧按时刻写，见 UpdateScenery）
	if (CloudMIDs.Num() == 0)
		if (UMaterialInterface* Cloud = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/Sky/M_DysisCloud.M_DysisCloud")))
			for (int32 i = 0; i < 3; ++i) CloudMIDs.Add(UMaterialInstanceDynamic::Create(Cloud, this));

	auto Place = [this](UStaticMesh* Mesh, const FVector& Loc, float Yaw, const FVector& Scale, bool bShadow, bool bSolid)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetupAttachment(RootComponent);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(bShadow);
		C->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bSolid)
		{
			C->SetCollisionObjectType(ECC_WorldStatic);
			C->SetCollisionResponseToAllChannels(ECR_Ignore);          // 不挡光（光路的判定走 Visibility）
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			C->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		}
		C->RegisterComponent();
		C->SetWorldLocationAndRotation(Loc, FRotator(0.0f, Yaw, 0.0f));
		C->SetWorldScale3D(Scale);
		SceneryComps.Add(C);
		return C;
	};

	const FVector2D IsleDir = ScnIsleC.GetSafeNormal();
	for (const FDysisSceneryItem& It : GDysisScenery)
	{
		UStaticMesh* Mesh = Meshes[int32(It.Mesh)];
		if (!Mesh) continue;
		FVector Loc = It.Loc;
		float Yaw = It.Yaw;
		bool bShadow = false, bSolid = false;
		const bool bCloud = It.Kind == EDysisSceneryKind::Cloud;
		const bool bFar = bCloud || It.Kind == EDysisSceneryKind::FarMountain || It.Kind == EDysisSceneryKind::MidMountain || It.Kind == EDysisSceneryKind::Monolith;
		if (bCloud)
		{
			for (const float Turn : ScnCloudTurns)
			{
				FVector At = It.Loc.RotateAngleAxis(SceneryFarTurnDeg + Turn, FVector::UpVector);
				At.Z *= SceneryCloudLift;
				UStaticMeshComponent* C = Place(Mesh, At, It.Yaw + SceneryFarTurnDeg + Turn, It.Scale * SceneryCloudScale, false, false);
				for (int32 Slot = 0; Slot < 3 && CloudMIDs.IsValidIndex(Slot); ++Slot) C->SetMaterial(Slot, CloudMIDs[Slot]);
			}
			continue;
		}
		if (bFar)
		{
			Loc = Loc.RotateAngleAxis(SceneryFarTurnDeg, FVector::UpVector);
			Yaw += SceneryFarTurnDeg;
		}
		else
		{
			const bool bGrass = It.Kind == EDysisSceneryKind::GrassBank || It.Kind == EDysisSceneryKind::EdgeGrassBank;
			const float Radius = ScnRockR * float(FMath::Max(It.Scale.X, It.Scale.Y));
			if (FVector2D::Distance(FVector2D(Loc), ScnTerraceC) + Radius * 0.5f <= ScnTerraceR)
			{
				++SceneryOnTerrace;
				bShadow = true;
				bSolid = !bGrass;
				if (bSolid) ++ScenerySolid;
			}
			else
			{
				// 台地外面：草坡不要；石头沉到海里当礁石（露出水面的部分 = 半高 × (1 + ScenerySeaRockRise)），小岛那一条线上的不要
				const FVector2D P(Loc);
				const bool bInLane = FVector2D::DotProduct(P, IsleDir) > 0.0 && FMath::Abs(FVector2D::CrossProduct(P, IsleDir)) < ScnLaneHalfW + Radius;
				if (bGrass || bInLane) { ++ScenerySkipped; continue; }
				Loc.Z = DysisGB::SEA_Z + ScenerySeaRockRise * ScnRockH * float(It.Scale.Z);
				bShadow = true;
				++ScenerySeaRocks;
			}
		}
		UStaticMeshComponent* C = Place(Mesh, Loc, Yaw, It.Scale, bShadow, bSolid);
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const FString Name = Slot == 0 ? FString::Printf(TEXT("MI_MATCH_%s"), It.Palette) : FString::Printf(TEXT("MI_MATCH_%s_%d"), It.Palette, Slot);
			if (UMaterialInterface* M = ScnPalette(Name)) C->SetMaterial(Slot, M);
		}
	}
	// 山花上的日月徽（美术在低模那一版加的小装饰：一个赭石色的环，模型自带位置，和建筑一样转 90°）
	if (UStaticMesh* Emblem = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Dysis/LowPoly_v001/Meshes/LP_Pediment_SunMoon.LP_Pediment_SunMoon"), nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UStaticMeshComponent* C = Place(Emblem, FVector::ZeroVector, 90.0f, FVector::OneVector, true, false);
		if (UMaterialInterface* M = ScnPalette(TEXT("MI_MATCH_Ochre"))) C->SetMaterial(0, M);
	}
	ApplyPalette();
	UpdateScenery();
	UE_LOG(LogTemp, Log, TEXT("Dysis 配景：放了 %d 件（台地上 %d、礁石 %d，没放 %d），%d 个建筑部件的面换了配色"),
		SceneryComps.Num(), SceneryOnTerrace, ScenerySeaRocks, ScenerySkipped, PaletteSlots);
}

void ADysisDirector::ApplyPalette()
{
	// 建筑（低模那一层）换成美术那版外景的配色。低模自带的材质叫 LP_<颜色>，对应的新材质叫 MI_MATCH_<颜色>；有几处他单独定过
	PaletteSlots = 0;
	for (UStaticMeshComponent* S : SkinComps)
	{
		const AActor* Part = S ? S->GetOwner() : nullptr;
		if (!Part) continue;
		FString Label;
		for (const FName& Tag : Part->Tags) if (Tag.ToString().StartsWith(TEXT("SM_"))) { Label = Tag.ToString(); break; }
		for (int32 Slot = 0; Slot < S->GetNumMaterials(); ++Slot)
		{
			const UMaterialInterface* Cur = S->GetMaterial(Slot);
			FString Name = Cur ? Cur->GetName() : FString();
			if (!Name.RemoveFromStart(TEXT("LP_"))) continue;
			Name = TEXT("MI_MATCH_") + Name;
			if (Label.StartsWith(TEXT("SM_Floor_L"))) Name = TEXT("MI_MATCH_InteriorFloor");                              // 各层的地面
			else if (Label == TEXT("SM_Portico_Roof")) Name = Slot == 0 ? TEXT("MI_MATCH_PedimentStone") : TEXT("MI_MATCH_WarmWhite");   // 门廊的山花和屋面深一档，线脚还是浅的
			else if (Label == TEXT("SM_Terrain")) Name = Slot == 0 ? TEXT("M_MATCH_Ground") : TEXT("MI_MATCH_Peach");      // 台地：顶上沙地 / 草 / 路，侧面的崖壁
			else if (Label == TEXT("SM_Island")) Name = TEXT("MI_MATCH_Peach");                                             // 开局的小岛
			if (UMaterialInterface* M = ScnPalette(Name)) { S->SetMaterial(Slot, M); ++PaletteSlots; }
		}
	}
}

void ADysisDirector::UpdateScenery()
{
	// 云的颜色跟着天走：白天是天光照着的粉白，太阳低了偏橙红，入夜只剩一点月色
	const UDysisTimeComponent* Time = PlayerTime();
	const ADysisSkyActor* Sky = Time ? Time->SkyActor.Get() : nullptr;
	if (!Sky || CloudMIDs.Num() < 3) return;
	float SunAlt = 30.0f, MoonAlt = -30.0f;
	Sky->GetAltitudes(SunAlt, MoonAlt);
	const float Low = 1.0f - DysisGB::Smoothstep(2.0f, 25.0f, SunAlt);
	const float Night = DysisGB::Smoothstep(-2.0f, -10.0f, SunAlt), MoonUp = DysisGB::Smoothstep(0.0f, 12.0f, MoonAlt);
	// 三档：美术那版云的三个颜色（他的数 ÷ 天的亮度 12000）；太阳低的时候各自偏橙红
	static const FLinearColor Day[3] = { FLinearColor(0.875f, 0.633f, 0.692f), FLinearColor(0.725f, 0.517f, 0.642f), FLinearColor(0.542f, 0.492f, 0.650f) };
	static const FLinearColor Dusk[3] = { FLinearColor(1.25f, 0.56f, 0.36f), FLinearColor(0.86f, 0.40f, 0.42f), FLinearColor(0.46f, 0.32f, 0.52f) };
	for (int32 i = 0; i < 3; ++i)
	{
		const FLinearColor C = (Day[i] + (Dusk[i] - Day[i]) * Low) * (Sky->GetSkyGain() * SceneryCloudGain)
			+ FLinearColor(0.11f, 0.14f, 0.22f) * (Night * (0.4f + 0.6f * MoonUp) * (1.0f - 0.2f * i));   // 比夜空稍亮一点：月光照着的云，不是天上的一块黑
		CloudMIDs[i]->SetVectorParameterValue(TEXT("Color"), C);
	}
}

void ADysisDirector::RebuildScenery()
{
	SetupScenery();
}

FString ADysisDirector::DescribeScenery() const
{
	int32 Shown = 0, Solid = 0, BlockLight = 0;
	for (const UStaticMeshComponent* C : SceneryComps)
	{
		if (!C) continue;
		if (C->IsVisible()) ++Shown;
		if (C->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++Solid;
		if (C->GetCollisionEnabled() != ECollisionEnabled::NoCollision && C->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Ignore) ++BlockLight;
	}
	return FString::Printf(TEXT("{\"placed\":%d,\"shown\":%d,\"onTerrace\":%d,\"seaRocks\":%d,\"skipped\":%d,\"solid\":%d,\"blockLight\":%d,\"paletteSlots\":%d,\"clouds\":%d}"),
		SceneryComps.Num(), Shown, SceneryOnTerrace, ScenerySeaRocks, ScenerySkipped, Solid, BlockLight, PaletteSlots, CloudMIDs.Num());
}
