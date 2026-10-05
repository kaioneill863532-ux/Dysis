// 日落回廊 · 游戏文案表（来自美术/设计侧提供的《游戏文案及其出现条件.xlsx》2026-10-05）。
// 全部台词、交互提示、查看描述、机关反馈按原表逐字誊入——不改动任何一个字。
// 代码引用方式：各 Actor 的 GetInteractPrompt() / DialogueComponent 的 Lines 从这里按索引取。
// 条件列对应代码里的触发口（GetInteractPrompt 在 CanInteractNow 时由 HUD 显示）。
//
// ⚠ 本文件是唯一文案真源（Single Source of Truth）——改动任何一个字必须和设计侧确认。
// 格式：[类型] [位置] [条件] [说话人] 文本
#pragma once

#include "CoreMinimal.h"

/**
 * 静态文案表——按章节索引取，不走 DataTable（零资产依赖，编译期检查）。
 * 各代码引用处在注释里标注"文案表§行号"。
 */
namespace DysisCopy
{
	// ═══════ 游戏剧情 ═══════

	/// 开场对话（赫利俄斯 × 狄西斯，游戏开场 13:29）——文案表第 1-10 行
	inline static const TCHAR* OpeningDialogue[] = {
		TEXT("赫利俄斯：你来了，狄西斯。"),
		TEXT("狄西斯：我来了，赫利俄斯。"),
		TEXT("狄西斯：殿顶那颗小小的太阳还挂在原来的地方吗？"),
		TEXT("赫利俄斯：还在那里。当我的马车驶近西边的海，最后一缕日光落在它的身上，它便是你的金苹果了。此后，天空就要由你交给塞勒涅。"),
		TEXT("狄西斯：月亮会借着它的光升起来，跟着我一路往下走。等把它放进水亭的月托，光就到她手里了。"),
		TEXT("赫利俄斯：还和往常一样：你沿回廊顺着时间走一步，我的马车才向前移一寸；你逆行或后退，我的马也得跟着退回去——"),
		TEXT("狄西斯：别让它们太累，我记得的。"),
		TEXT("赫利俄斯：现在水汽正足，我给你铺一条光路，你往前走它便会显现出来。"),
		TEXT("赫利俄斯：进了回廊要留心水汽，里面的光要有水雾托着才显得出来。"),
	};
	constexpr int32 OpeningDialogueCount = 9;

	/// 棱镜谜题对话（伊莉丝 × 塞勒涅 × 狄西斯，解开棱镜谜题后）——文案表第 11-23 行
	inline static const TCHAR* PrismDialogue[] = {
		TEXT("伊莉丝：醒醒，塞勒涅，是我。"),
		TEXT("塞勒涅：伊莉丝？天还亮着，你怎么唤得醒我？"),
		TEXT("伊莉丝：是狄西斯把靛色的光送到了你的眼睛里。"),
		TEXT("塞勒涅：狄西斯？她的时辰还没到，倒先来唤醒我了，难怪我梦见了许多颜色。我的夜里通常只有银灰。"),
		TEXT("伊莉丝：别的颜色你也看看吧，红色也很好看。"),
		TEXT("塞勒涅：太亮了，看了会睡不着。靛色就挺好，它是天黑以前最后的一层深蓝。看着它，我就知道自己该醒了。"),
		TEXT("伊莉丝：那我改天去夜里看你。月光下我也能出来，只是会淡一些，是白白的一道。"),
		TEXT("塞勒涅：月虹。上次见到你那个样子已经是很久以前了。"),
		TEXT("伊莉丝：今夜瀑布开着，水汽正足。等你升到东边不高不低的地方就来看看瀑布。"),
		TEXT("塞勒涅：说定了。狄西斯，你回头也一起来看看吧。"),
		TEXT("狄西斯：好，我会记得回头的。"),
	};
	constexpr int32 PrismDialogueCount = 11;

	/// 结局 A——未集齐三碎片（塞勒涅 × 狄西斯，金苹果放入月托）——文案表第 24-25 行
	inline static const TCHAR* EndingA[] = {
		TEXT("塞勒涅：月光送到了，狄西斯。"),
		TEXT("塞勒涅：一路辛苦。后面的天色就交给我吧。"),
	};
	constexpr int32 EndingACount = 2;

	/// 结局 B——集齐三碎片（塞勒涅 × 狄西斯）——文案表第 26-28 行
	inline static const TCHAR* EndingB[] = {
		TEXT("塞勒涅：月光送到了，狄西斯，连火、水、气你也一并带了回来。"),
		TEXT("塞勒涅：一路辛苦，满天星斗将为你闪烁。后面的天色就交给我吧。"),
		TEXT("狄西斯：替我向天鹅与双子致意。"),
	};
	constexpr int32 EndingBCount = 3;

	// ═══════ 游戏提示 ═══════

	/// 入夜提示（取下金苹果后）——文案表
	inline static const TCHAR* NightHint =
		TEXT("狄西斯：塞勒涅说过，她的月光能照出事物的另一面。");

	/// 下月桥后再次与瀑布/女神交互——文案表
	inline static const TCHAR* MoonbridgeHint =
		TEXT("月光穿过水帘，从塞勒涅怀里的月亮上扫过去了。刚才在月桥上走过的，是哪一步？");

	/// 坠落回档——文案表
	inline static const TCHAR* RespawnHint =
		TEXT("作为十二时辰之一，狄西斯轻松回到了上一个落脚点。她不好意思地看了看赫利俄斯和他的马。");

	// ═══════ 交互显示（按 E ）═══════

	inline static const TCHAR* PromptWaterGate    = TEXT("打开水闸");
	inline static const TCHAR* PromptRotateStatue  = TEXT("转动雕像");       // 三相机/少女像/天鹅像
	inline static const TCHAR* PromptViewRelief    = TEXT("看看浮雕");       // 天鹅/伊莉丝/塞勒涅浮雕
	inline static const TCHAR* PromptOpenSunNiche  = TEXT("打开日之龛");
	inline static const TCHAR* PromptOpenMoonNiche = TEXT("打开月之龛");
	inline static const TCHAR* PromptOpenIrisNiche = TEXT("打开虹之龛");
	inline static const TCHAR* PromptRotatePrism   = TEXT("转动棱镜");
	inline static const TCHAR* PromptPullPollux    = TEXT("拉出波吕丢刻斯雕像");
	inline static const TCHAR* PromptTakeApple     = TEXT("取下金苹果");
	inline static const TCHAR* PromptPlaceApple    = TEXT("放入金苹果");      // 取下后变为这个

	// ═══════ 机关反馈 ═══════

	/// 机关 AB 奇数次交互
	inline static const TCHAR* MechOddTrigger =
		TEXT("远处传来石板滑动的声音。西边的窗打开了，南边的窗关上了。");
	/// 机关 AB 偶数次交互
	inline static const TCHAR* MechEvenTrigger =
		TEXT("远处传来石板滑动的声音。西边的窗关上了，南边的窗打开了。");
	/// 打开虹之龛
	inline static const TCHAR* IrisNicheOpened =
		TEXT("你得到了一片彩虹碎片，气元素萦绕在你的指尖。");
	/// 打开日之龛
	inline static const TCHAR* SunNicheOpened =
		TEXT("你得到了一片太阳碎片，火元素熊熊燃烧着。");
	/// 打开月之龛
	inline static const TCHAR* MoonNicheOpened =
		TEXT("你得到了一片月亮碎片，水元素洗涤了你的身心。");

	// ═══════ 解谜后反馈 ═══════

	inline static const TCHAR* WaterGateOpened     = TEXT("水雾溋溋升起，一条光路出现在你眼前。");
	inline static const TCHAR* IrisPuzzleSolved    = TEXT("虹从浮雕中走下，在你眼前铺展开。似乎也可以这样去到另一边？");
	inline static const TCHAR* PrismPuzzleSolved   = TEXT("靛色光芒落进月神塞勒涅的青金石之眼。");
	inline static const TCHAR* AppleTaken          = TEXT("金苹果中盛满了日落夕阳，散发出耀眼的光芒。去下面的水亭，把它交给塞勒涅吧。");
	inline static const TCHAR* ApplePlaced         = TEXT("金苹果被放到了托盘上。流淌在其中的金黄逐渐褪色，将它化为一轮明月，递交给等候在天上的塞勒涅。");
	inline static const TCHAR* SwanPuzzleSolved    = TEXT("白天鹅静立在月光里，黑天鹅却已在墙上找到了它的同伴。浮雕缓缓沉下，天鹅座为你让出一条向下的通路。");
}
