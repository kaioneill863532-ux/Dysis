// 日落回廊 · 低模外观：场景美术 2026-10-08 的低模（/Game/Dysis/LowPoly_v001/Meshes/LP_<原模型名>）只管看，
// 碰撞、挡光、能踩的面仍然用原模型——所以光路的判定、人能走到哪，和灰盒核对过的结果一点不变。
// 做法：给每个建筑部件挂一个不带碰撞的低模组件（挂在原模型的组件上，位置大小自然一样），原模型自己不再画出来。
// 机关部件（SM_Mech_*）先不换：它们的材质、显隐、缩放有别的代码在管。
// 控制台 Dysis.LowPoly 0|1 可以当场切回原模型对比。
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Dysis.LowPoly 0|1：建筑用低模（1，默认）还是原模型（0）
	FAutoConsoleCommandWithWorldAndArgs GDysisLowPoly(TEXT("Dysis.LowPoly"), TEXT("建筑的样子：Dysis.LowPoly 1 = 低模，0 = 原模型（碰撞一直是原模型的）"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			for (TActorIterator<ADysisDirector> It(World); It; ++It) It->DebugSetLowPoly(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
		}));
}

void ADysisDirector::SetupLowPolySkin()
{
	SkinComps.Reset();
	SkinOrigs.Reset();
	TSet<const AActor*> Done;
	for (const TPair<FName, TWeakObjectPtr<AActor>>& P : Pieces)
	{
		AActor* A = P.Value.Get();
		const FString Tag = P.Key.ToString();
		if (!A || Done.Contains(A) || Tag.StartsWith(TEXT("SM_Mech_")) || Tag.StartsWith(TEXT("SM_MARK_"))) continue;
		Done.Add(A);
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps)
		{
			const UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
			if (!Mesh || !C->IsVisible() || !Mesh->GetPathName().StartsWith(TEXT("/Game/Dysis/Temple_v0_12/"))) continue;
			const FString Name = Mesh->GetName();
			UStaticMesh* Low = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Dysis/LowPoly_v001/Meshes/LP_%s.LP_%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Low) continue;
			UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(A);
			S->SetMobility(EComponentMobility::Movable);        // 运行时才挂上去的；原模型会动的话它跟着动
			S->SetStaticMesh(Low);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			S->SetGenerateOverlapEvents(false);
			S->SetCanEverAffectNavigation(false);
			S->SetCastShadow(C->CastShadow);
			S->SetupAttachment(C);
			S->RegisterComponent();
			SkinComps.Add(S);
			SkinOrigs.Add(C);
		}
	}
	bLowPolyShown = false;
	DebugSetLowPoly(SkinComps.Num() > 0);
	UE_LOG(LogTemp, Log, TEXT("Dysis 低模外观：%d 个建筑部件换上了低模（碰撞仍是原模型）"), SkinComps.Num());
}

void ADysisDirector::DebugSetLowPoly(bool bShown)
{
	bLowPolyShown = bShown && SkinComps.Num() > 0;
	for (int32 i = 0; i < SkinComps.Num(); ++i)
	{
		// 只切“画不画”：原模型不画的时候碰撞照旧（挡人、挡光、能踩都还是它）
		if (SkinComps[i]) SkinComps[i]->SetVisibility(bLowPolyShown);
		if (SkinOrigs[i]) SkinOrigs[i]->SetVisibility(!bLowPolyShown);
	}
}

FString ADysisDirector::DescribeSkin() const
{
	int32 Shown = 0, OrigShown = 0, OrigSolid = 0;
	for (int32 i = 0; i < SkinComps.Num(); ++i)
	{
		if (SkinComps[i] && SkinComps[i]->IsVisible()) ++Shown;
		if (SkinOrigs[i] && SkinOrigs[i]->IsVisible()) ++OrigShown;
		if (SkinOrigs[i] && SkinOrigs[i]->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ++OrigSolid;
	}
	return FString::Printf(TEXT("{\"pieces\":%d,\"lowShown\":%d,\"origShown\":%d,\"origSolid\":%d,\"on\":%s}"),
		SkinComps.Num(), Shown, OrigShown, OrigSolid, bLowPolyShown ? TEXT("true") : TEXT("false"));
}

// ───────────────────────── 机关部件：和墙同色；暗门、封石退到和墙面齐平 ─────────────────────────

namespace
{
	// 嵌在内墙里、解开以前不该让人一眼看出来的那几件（2026-10-10 用户：墙面上突出来、没和墙融成一体的机关要和墙面重合）。
	// 灰盒里它们比墙面凸出十几厘米（塞勒涅的浮雕板凸出将近三十厘米，棱镜分出来的色光其实落在它背后的墙上，被它挡着看不见）。
	// 做法：原件不画了（碰撞、挡光、会动的逻辑都还是它），另挂一份一样的模型，沿半径往墙里退到“朝殿心的那一面比墙面只凸出 SkinProud 厘米”。
	// Follow：跟着另一件退同样多（月亮浮雕上的银盘跟着浮雕板走，不然会悬在半空）。
	// Relief：这一件是浮雕——花纹比底板凸出这么多厘米，齐平的是底板，花纹仍然凸在墙面外。
	struct FFlushSpec { const TCHAR* Tag; const TCHAR* Follow; float Relief = 0.0f; };
	const FFlushSpec SkinFlush[] = {
		// （墙里两段楼梯下口的封石、天鹅浮雕后面的门、月之龛的盖板原来也在这张表里。2026-10-10 第八轮改成照洞口现做的门板——
		//   这几块方石板比洞口大一圈，退平以后还凸出墙面 1 cm，用户看着像墙上鼓出来一块。见 DysisDirectorDetails.cpp）
		{ TEXT("SM_Mech_MoonRelief_Block"), nullptr }, { TEXT("SM_Mech_MoonRelief_Disk"), TEXT("SM_Mech_MoonRelief_Block") },   // 月亮浮雕（挡着楼梯上口）和它上面的银盘
		{ TEXT("SM_Mech_Selene_Relief"), nullptr, 12.0f },                                             // 塞勒涅的浮雕：底板退平，眼睛和眼眶的花纹留在墙面外
	};
	constexpr float SkinProud = 1.0f;

	/** 这个部件朝殿心的那一面离殿心多远（把模型的包围盒投到它所在方位的半径方向上，取最里面的）。 */
	double SkinInnerFace(const UStaticMeshComponent* C, FVector& OutRadial)
	{
		const FBoxSphereBounds B = C->GetStaticMesh()->GetBounds();
		const FTransform& T = C->GetComponentTransform();
		const FVector Center = T.TransformPosition(B.Origin);
		OutRadial = FVector(Center.X, Center.Y, 0.0).GetSafeNormal();
		double Inner = 1.0e9;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Corner = T.TransformPosition(B.Origin + FVector((i & 1) ? B.BoxExtent.X : -B.BoxExtent.X, (i & 2) ? B.BoxExtent.Y : -B.BoxExtent.Y, (i & 4) ? B.BoxExtent.Z : -B.BoxExtent.Z));
			Inner = FMath::Min(Inner, FVector::DotProduct(Corner, OutRadial));
		}
		return Inner;
	}
}

void ADysisDirector::SetupMechLook()
{
	for (UStaticMeshComponent* C : FlushComps) if (C) C->DestroyComponent();
	FlushComps.Reset(); FlushOf.Reset(); MechRecolored = 0;
	UMaterialInterface* Warm = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_WarmWhite.MI_MATCH_WarmWhite"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Sand = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Dysis/ReferenceMatch_20261009/Materials/MI_MATCH_Sand.MI_MATCH_Sand"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Warm || !Sand) return;

	// ① 石头做的机关部件换成和墙一样的两种颜色（墙原来用的也是这两种石头材质：M_stone → 暖白，M_stoneDark → 沙色）
	TSet<const AActor*> Done;
	for (const TPair<FName, TWeakObjectPtr<AActor>>& P : Pieces)
	{
		AActor* A = P.Value.Get();
		if (!A || Done.Contains(A) || !P.Key.ToString().StartsWith(TEXT("SM_Mech_"))) continue;
		Done.Add(A);
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps)
			for (int32 Slot = 0; C && Slot < C->GetNumMaterials(); ++Slot)
			{
				const UMaterialInterface* Cur = C->GetMaterial(Slot);
				const FName Name = Cur ? Cur->GetFName() : NAME_None;
				if (Name == TEXT("M_stone")) { C->SetMaterial(Slot, Warm); ++MechRecolored; }
				// 双子那一段加厚的墙是殿里的墙的一部分：和墙一个颜色（2026-10-10 用户：殿里的建筑不要两种颜色）
				else if (Name == TEXT("M_stoneDark")) { C->SetMaterial(Slot, P.Key == FName(TEXT("SM_Mech_Twins_ThickWall")) ? Warm : Sand); ++MechRecolored; }
			}
	}

	// ② 暗门、封石、浮雕板：退到和墙面齐平
	TMap<FName, FVector> OffsetOf;
	for (const FFlushSpec& Spec : SkinFlush)
	{
		AActor* A = Piece(Spec.Tag);
		UStaticMeshComponent* C = A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
		if (!C || !C->GetStaticMesh()) continue;
		FVector Offset = FVector::ZeroVector;
		if (Spec.Follow)
		{
			if (const FVector* O = OffsetOf.Find(Spec.Follow)) Offset = *O;
		}
		else
		{
			FVector Radial;
			const double Inner = SkinInnerFace(C, Radial);
			Offset = Radial * FMath::Max(0.0, (DysisGB::R_IN - SkinProud) - Inner - Spec.Relief);
		}
		OffsetOf.Add(Spec.Tag, Offset);
		if (Offset.IsNearlyZero(0.05)) continue;
		UStaticMeshComponent* V = NewObject<UStaticMeshComponent>(A);
		V->SetMobility(EComponentMobility::Movable);
		V->SetStaticMesh(C->GetStaticMesh());
		V->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		V->SetGenerateOverlapEvents(false);
		V->SetCanEverAffectNavigation(false);
		V->SetCastShadow(C->CastShadow);
		V->SetupAttachment(C);
		V->RegisterComponent();
		V->SetRelativeLocation(C->GetComponentTransform().InverseTransformVector(Offset));
		for (int32 Slot = 0; Slot < C->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* M = C->GetMaterial(Slot);
			// 门本来是浮雕的颜色，和墙不一样：也换成墙的颜色
			if (M && M->GetFName() == TEXT("M_relief") && FName(Spec.Tag) == TEXT("SM_Mech_Swan_Door")) M = Warm;
			V->SetMaterial(Slot, M);
		}
		C->SetVisibility(false);     // 原件不画；它的碰撞、位置、显隐（整个 Actor 藏起来）照旧，这一份跟着它
		FlushComps.Add(V);
		FlushOf.Add(Spec.Tag, V);
	}
	UE_LOG(LogTemp, Log, TEXT("Dysis 机关的样子：%d 个面换成墙的颜色，%d 件退到和墙面齐平"), MechRecolored, FlushComps.Num());
}

UStaticMeshComponent* ADysisDirector::VisualOf(FName PieceTag) const
{
	if (const TWeakObjectPtr<UStaticMeshComponent>* V = FlushOf.Find(PieceTag))
		if (V->IsValid()) return V->Get();
	const AActor* A = Piece(PieceTag);
	return A ? A->FindComponentByClass<UStaticMeshComponent>() : nullptr;
}

FString ADysisDirector::DescribeMechLook() const
{
	FString Items;
	for (const TPair<FName, TWeakObjectPtr<UStaticMeshComponent>>& P : FlushOf)
		if (const UStaticMeshComponent* V = P.Value.Get())
		{
			const UStaticMeshComponent* Orig = Cast<UStaticMeshComponent>(V->GetAttachParent());
			const double Back = Orig ? FVector::Dist(V->GetComponentLocation(), Orig->GetComponentLocation()) : 0.0;
			FVector Radial;
			const double Inner = SkinInnerFace(V, Radial);
			if (!Items.IsEmpty()) Items += TEXT(",");
			Items += FString::Printf(TEXT("{\"tag\":\"%s\",\"back\":%.1f,\"inner\":%.1f,\"origShown\":%s,\"solid\":%s}"), *P.Key.ToString(), Back, Inner,
				Orig && Orig->IsVisible() ? TEXT("true") : TEXT("false"), Orig && Orig->GetCollisionEnabled() != ECollisionEnabled::NoCollision ? TEXT("true") : TEXT("false"));
		}
	return FString::Printf(TEXT("{\"recolored\":%d,\"flush\":[%s]}"), MechRecolored, *Items);
}
