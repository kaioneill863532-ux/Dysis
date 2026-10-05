// 日落回廊 · P1-10 核查工具（调研 §3 P1-10）：Dysis.CheckNanite——列出关卡里开了 Nanite 的网格。
// 半透明/Additive 材质挂 Nanite 网格会被默认材质替换（仅日志警告、画面静默损坏）——
// 光束盒/月盘/水帘这类表现件必须非 Nanite。Windows 实测 M0 第一步就是跑它。
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/ConsoleManager.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GDysisCheckNanite(
		TEXT("Dysis.CheckNanite"),
		TEXT("列出关卡里开了 Nanite 的静态网格（P1-10：半透明表现件不得在列）"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (!World) return;
			int32 Total = 0, NaniteOn = 0;
			for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
			{
				const UStaticMeshComponent* C = It->GetStaticMeshComponent();
				const UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
				if (!M) continue;
				++Total;
				if (M->NaniteSettings.bEnabled)
				{
					++NaniteOn;
					UE_LOG(LogTemp, Display, TEXT("  Nanite：%s"), *It->GetName());
				}
			}
			UE_LOG(LogTemp, Display, TEXT("Dysis.CheckNanite：%d 个网格 Actor，其中 %d 开了 Nanite（名单见上；光束/月盘等半透明件若在列→关 Nanite）"),
				Total, NaniteOn);
		}));
}
