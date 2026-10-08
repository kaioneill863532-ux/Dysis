// 日落回廊 · “查看××”：文案表“查看显示”那一组——走近了旁边浮出 E，按一下在提示条里出一句描述。不动任何机关。
// （三块浮雕的“看看浮雕”在各自机关的文件里；这里是剩下的：雕像、浑天仪、两面石墙、开过的水闸。）
#include "DysisDirector.h"
#include "DysisGreybox.h"
#include "World/DysisWorldState.h"
#include "Sky/DysisTimeComponent.h"
#include "UI/DysisCopy.h"
#include "UI/DysisHUD.h"

namespace
{
	const FVector VwGoddess(-1016.0, 992.0, 0.0);           // 抱月女神像：瀑布后面的水槽里（方位 135.68°、半径 14.2 m）
	const FVector VwTriple(372.11, -1297.70, 1450.0);       // 托镜天使像（三相像）脚下的楼面
	const FVector VwCastor(-371.405, -1386.104, 600.0);     // 卡斯托耳
	constexpr float VwPolluxAz = 146.5f;                    // 藏着波吕丢刻斯的那面墙
	constexpr float VwMoonGateAz = 178.7052f;               // 月门（东南墙上刻着月轮的浮雕）
	constexpr float VwWallR = 1515.0f;                      // 贴着内墙面站的地方（墙面往里 0.35 m）
	const FVector VwPavilion(0.0, 0.0, 20.0);               // 水亭的浑天仪
	const FVector VwSluice(-469.73, 1065.01, 90.0);         // 水闸
	constexpr int32 VwTwinHidden = 0;                       // 双子：波吕丢刻斯还藏在墙里
}

void ADysisDirector::AddViewInteracts()
{
	// Text 是函数：有的东西解谜前后是两句话
	auto Add = [this](const TCHAR* Id, TFunction<FVector()> Pos, const FVector2D& Z, float Radius, TFunction<bool()> When, const TCHAR* Label, TFunction<const TCHAR*()> Text, TFunction<FVector()> Anchor)
	{
		FDysisInteract I;
		I.Id = Id;
		I.Pos = MoveTemp(Pos);
		I.ZRange = [Z]() { return Z; };
		I.RadiusCm = Radius;
		I.When = MoveTemp(When);
		I.Label = [Label]() { return FText::FromString(Label); };
		I.Act = [this, Text]() { ADysisHUD::Notify(GetWorld(), Text(), 5.2f); };
		I.Anchor = MoveTemp(Anchor);
		Interacts.Add(MoveTemp(I));
	};

	// 抱月女神像：亮起来以前、以后各一句
	Add(TEXT("viewGoddess"), []() { return VwGoddess; }, FVector2D(-60.0, 260.0), 420.0f, nullptr, DysisCopy::PromptViewStatue,
		[this]() { return bGoddessLit ? DysisCopy::ViewGoddessLit : DysisCopy::ViewGoddessIdle; }, []() { return VwGoddess + FVector(0.0, 0.0, 260.0); });

	// 托镜天使像（转它要去旁边的绞盘）
	Add(TEXT("viewTriple"), []() { return VwTriple; }, FVector2D(1420.0, 1650.0), 170.0f, nullptr, DysisCopy::PromptViewStatue,
		[]() { return DysisCopy::ViewTripleStatue; }, []() { return VwTriple + FVector(0.0, 0.0, 230.0); });

	// 卡斯托耳：双子并肩以前
	Add(TEXT("viewCastor"), []() { return VwCastor; }, FVector2D(580.0, 800.0), 170.0f, [this]() { return !bTwinsJoined; }, DysisCopy::PromptViewStatue,
		[]() { return DysisCopy::ViewCastor; }, []() { return VwCastor + FVector(0.0, 0.0, 190.0); });

	// 藏着波吕丢刻斯的墙：月光照开以前
	Add(TEXT("viewPolluxWall"), []() { return DysisGB::PolarCm(VwPolluxAz, VwWallR, 600.0f); }, FVector2D(580.0, 800.0), 150.0f, [this]() { return TwinState == VwTwinHidden; }, DysisCopy::PromptViewWall,
		[]() { return DysisCopy::ViewPolluxWall; }, []() { return DysisGB::PolarCm(VwPolluxAz, VwWallR + 30.0f, 760.0f); });

	// 月门：月光照开以前
	Add(TEXT("viewMoonGate"), []() { return DysisGB::PolarCm(VwMoonGateAz, VwWallR, 1450.0f); }, FVector2D(1420.0, 1650.0), 150.0f,
		[this]() { const FMoonstone* Ms = FindMoonstone(TEXT("moonRelief")); return Ms && Ms->K < 0.5f; }, DysisCopy::PromptViewWall,
		[]() { return DysisCopy::ViewMoonGate; }, []() { return DysisGB::PolarCm(VwMoonGateAz, VwWallR + 30.0f, 1610.0f); });

	// 殿顶的浑天仪：金苹果取下来以后（取下来以前按 E 是“取下金苹果”，那一句描述走近了自己出来，见 UpdateViews）
	Add(TEXT("viewArmillaryTop"), [this]() { return ArmTopCm(); }, FVector2D(3000.0, 3900.0), 180.0f, [this]() { return bCaught; }, DysisCopy::PromptViewArmillary,
		[]() { return DysisCopy::ViewArmillaryTopEmpty; }, [this]() { return ArmTopCm(); });

	// 水亭的浑天仪：还没带着金苹果来的时候、放上去以后（带着金苹果时按 E 是“放入金苹果”）
	Add(TEXT("viewArmillaryPav"), []() { return VwPavilion; }, FVector2D(-10.0, 180.0), 280.0f, [this]() { return !bCaught || bApplePlaced; }, DysisCopy::PromptViewArmillary,
		[this]() { return bApplePlaced ? DysisCopy::ViewArmillaryPavLit : DysisCopy::ViewArmillaryPavEmpty; }, []() { return VwPavilion + FVector(0.0, 0.0, 160.0); });

	// 开过的水闸：只能看（文案表“一层水闸再交互（无关闭选项）”）
	Add(TEXT("viewSluice"), []() { return VwSluice; }, FVector2D(60.0, 270.0), 190.0f, [this]() { const UDysisWorldState* S = UDysisWorldState::Get(this); return S && S->bSluiceOpen; }, DysisCopy::PromptViewWaterGate,
		[]() { return DysisCopy::FeedbackWaterGateReinteract; }, nullptr);
}

void ADysisDirector::UpdateViews()
{
	// 殿顶的浑天仪（金苹果还在上面）：头一次走到它跟前，那句描述自己出来
	if (bTopArmillaryTold || bCaught || !GameStarted() || CrownUp < 0.9f) return;
	const UDysisTimeComponent* Time = PlayerTime();
	if (!Time || Time->Zone != TEXT("crown")) return;
	const FVector Arm = ArmTopCm();
	if (FVector::Dist2D(Time->FootCm, Arm) < 330.0 && FMath::Abs(Time->FootCm.Z - (Arm.Z - 110.0)) < 160.0)
	{
		bTopArmillaryTold = true;
		ADysisHUD::Notify(GetWorld(), DysisCopy::ViewArmillaryTopApple, 5.5f);
	}
}
