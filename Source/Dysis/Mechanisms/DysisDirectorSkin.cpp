// 日落回廊 · 低模外观：场景美术 2026-10-08 的低模（/Game/Dysis/LowPoly_v001/Meshes/LP_<原模型名>）只管看，
// 碰撞、挡光、能踩的面仍然用原模型——所以光路的判定、人能走到哪，和灰盒核对过的结果一点不变。
// 做法：给每个建筑部件挂一个不带碰撞的低模组件（挂在原模型的组件上，位置大小自然一样），原模型自己不再画出来。
// 机关部件（SM_Mech_*）先不换：它们的材质、显隐、缩放有别的代码在管。
// 控制台 Dysis.LowPoly 0|1 可以当场切回原模型对比。
#include "DysisDirector.h"
#include "Components/StaticMeshComponent.h"
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
