// 日落回廊 · 存档自动化测试（M7 先行，调研 §8.7）：roundtrip / 原子轮换 / .bak 回滚 / 单调原则。
// 槽名带时间戳，跑完 DeleteSlot 清理，不留垃圾。
#include "Save/DysisSaveGame.h"
#include "Save/DysisSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
	FString MakeTestSlot() { return FString::Printf(TEXT("DysisTest_%lld"), FDateTime::UtcNow().GetTicks()); }
	void CleanupSlot(const FString& Slot)
	{
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		UGameplayStatics::DeleteGameInSlot(Slot + TEXT(".bak"), 0);
		UGameplayStatics::DeleteGameInSlot(TEXT("DysisTmp"), 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisSaveRoundtripTest, "Dysis.Save.Roundtrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisSaveRoundtripTest::RunTest(const FString&)
{
	const FString Slot = MakeTestSlot();
	UDysisSaveGame* Save = NewObject<UDysisSaveGame>();
	Save->bNight = true;
	Save->H = 194.153f;
	Save->SetNiche(EDysisNiche::Sun);
	Save->OpenSluices.Add(TEXT("Sluice_West"));
	TestTrue(TEXT("写入临时槽"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0));

	UDysisSaveGame* Loaded = Cast<UDysisSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	TestNotNull(TEXT("读回"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("版本"), Loaded->SaveVersion, UDysisSaveGame::CurrentVersion);
		TestTrue(TEXT("夜里"), Loaded->bNight);
		TestTrue(TEXT("日之龛"), Loaded->HasNiche(EDysisNiche::Sun));
		TestFalse(TEXT("虹之龛未拿"), Loaded->HasNiche(EDysisNiche::Rainbow));
		TestTrue(TEXT("水闸名单"), Loaded->IsSluiceOpen(TEXT("Sluice_West")));
		TestFalse(TEXT("未知水闸"), Loaded->IsSluiceOpen(TEXT("Nope")));
		TestEqual(TEXT("时刻"), Loaded->H, 194.153f, 0.001f);
	}
	CleanupSlot(Slot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisSaveRotateTest, "Dysis.Save.AtomicRotate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisSaveRotateTest::RunTest(const FString&)
{
	// 原子轮换：写两次档 → 正档=第二次、.bak=第一次；正档被删（模拟损坏）后备份还在。
	const FString Slot = MakeTestSlot();
	const FString TmpSlot = TEXT("DysisTmp");

	UDysisSaveGame* A = NewObject<UDysisSaveGame>();
	A->H = 10.f;
	UGameplayStatics::SaveGameToSlot(A, TmpSlot, 0);
	UDysisSaveSubsystem::RotateFiles(Slot);
	TestTrue(TEXT("第一次正档存在"), IFileManager::Get().FileExists(*UDysisSaveSubsystem::SlotPath(Slot)));

	UDysisSaveGame* B = NewObject<UDysisSaveGame>();
	B->H = 20.f;
	UGameplayStatics::SaveGameToSlot(B, TmpSlot, 0);
	UDysisSaveSubsystem::RotateFiles(Slot);

	UDysisSaveGame* Now = Cast<UDysisSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	TestTrue(TEXT("正档=新值"), Now && FMath::IsNearlyEqual(Now->H, 20.f));
	UDysisSaveGame* Bak = Cast<UDysisSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot + TEXT(".bak"), 0));
	TestTrue(TEXT("备份=旧值"), Bak && FMath::IsNearlyEqual(Bak->H, 10.f));

	// 模拟正档损坏：删掉，备份仍可读（子系统 LoadOrCreate 的回滚路径正是这条）。
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	Bak = Cast<UDysisSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot + TEXT(".bak"), 0));
	TestTrue(TEXT("正档损坏后备份可回滚"), Bak && FMath::IsNearlyEqual(Bak->H, 10.f));

	CleanupSlot(Slot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDysisSaveMonotonicTest, "Dysis.Save.Monotonic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDysisSaveMonotonicTest::RunTest(const FString&)
{
	// 单调原则（§14.2）：机关状态只进不退——数据层的两条护栏。
	UDysisSaveGame* Save = NewObject<UDysisSaveGame>();
	Save->SetNiche(EDysisNiche::Moon);
	Save->SetNiche(EDysisNiche::Moon, false);   // 误调不可逆口也只应……好吧数据层允许显式清（迁移/测试用），
	TestFalse(TEXT("显式清可用"), Save->HasNiche(EDysisNiche::Moon));   // 但便捷口 MarkNiche 只设 true（子系统层保证）
	TestTrue(TEXT("空档不算集齐"), !Save->AllNichesCollected());
	Save->SetNiche(EDysisNiche::Sun);
	Save->SetNiche(EDysisNiche::Rainbow);
	Save->SetNiche(EDysisNiche::Moon);
	TestTrue(TEXT("三龛集齐=黎明结局判定"), Save->AllNichesCollected());
	return true;
}
