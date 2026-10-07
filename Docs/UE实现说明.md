# UE 实现：日月轨道 + “站在哪里决定几点”

UE 5.8 里“日月在天上的固定轨道”和“时间由玩家站的位置决定”这部分功能的说明。光柱、光阶、光照后出现的路、机关、材质、特效等由程序老师实现。

## 代码在哪

| 文件 | 是什么 |
|---|---|
| `Source/Dysis/Sky/DysisSkyLibrary.h/.cpp` | `UDysisSkyLibrary`：日月方向、平行光朝向、`DysisTimeAt`、`DysisZoneOf` |
| `Source/Dysis/Sky/DysisSkyData.generated.h` | 常数、各层时间线、墙里楼梯、光路高度、月桥点（由灰盒标准答案 `golden_time.json` 生成，**不要手改**） |
| `Source/Dysis/Sky/DysisSkyActor.h/.cpp` | `ADysisSkyActor`：太阳光（Atmosphere Sun Light 0）+ 月光（1）+ 月亮圆盘，`SetTime(H)` |
| `Source/Dysis/Sky/DysisTimeComponent.h/.cpp` | `UDysisTimeComponent`：挂在玩家身上，每帧算 H；控制台 `Dysis.Go`、`Dysis.Where` |
| `Source/Dysis/Player/DysisCharacter.h/.cpp`、`DysisGameMode.h/.cpp` | 测试用的第一人称玩家（WASD、鼠标、空格跳、Shift 跑）和 GameMode。组员有自己的 Character 就不用它，把 `UDysisTimeComponent` 加过去即可 |
| `Content/Dysis/Maps/Dysis_Temple` | 神殿关卡：建筑、区域 Tag、DysisSky、SkyAtmosphere、SkyLight、HeightFog、PlayerStart，GameMode = DysisGameMode；**玩法 Actor 48 个**（大纲 `Dysis_Gameplay` 文件夹：光柱×8、水面/月光大道、屋顶踏步、拉杆/石板、机关驱动×21、三龛、金苹果/接光、BGM、开场导演——由 `Art/Models/temple-v0.12/ue_import_gameplay.py` 摆放，幂等可重跑） |
| `Content/Dysis/Sky/M_DysisMoonDisc` | 月亮圆盘材质（无光照、半透明，标量参数 Opacity） |
| `Content/Dysis/Audio/Music/M_*.uasset` ×4 | BGM（SoundWave，**循环已开**，统一挂音量组 `SC_Dysis_Music`；官方脚本 `bgm及导入代码/DysisMusic/ue_import_music.py` 已跑过，核对报告在 `Saved/Dysis_Music_Import.txt`）；管理器在关卡里找 `DysisMusic`（已指向 Music/ 的 Long/Full 两首） |
| `Content/Dysis/UI/Menu/` ×7 | 主菜单按钮图（开始游戏/设置/退出游戏/返回游戏/返回主界面/两个选择标）；`DysisHUD` 主菜单直接引用 |
| `Content/Dysis/UI/InGame/` ×9 | 游戏内 UI 图（文本框/反馈与提示/收藏品树枝/三种碎片/Dysis/Iris/设置）——对话底图、通知底图、碎片收集栏 |
| `Content/Dysis/UI/Portraits/` ×4 | 对话立绘（赫利俄斯/狄西斯/塞勒涅/伊莉丝）——按台词行首人名自动挂出 |
| `Content/Dysis/UI/References/Mockups/` ×8 | 示意图（主界面/设置/局内两款/四角色），美术参考不进游戏画面（设置页 v1 拿设置示意图占位） |
| `Content/Dysis/UI/Fonts/SourceHanSerifSC-*.uasset` | 思源宋体 FontFace（Regular/Bold，`tools/import_fonts.py` 导入）；`DysisHUD` BeginPlay 自动加载 Regular 画全部 UI 文字，缺资产退回引擎默认字体 |

`Source/Dysis/Dysis.Build.cs` 里加了 `PublicIncludePaths.Add(ModuleDirectory);`，子文件夹按 `"Sky/xxx.h"` 引用。

## 关卡是怎么搭的

1. 普通关卡（不用 World Partition）`/Game/Dysis/Maps/Dysis_Temple`。
2. 用 `Art/Models/temple-v0.12/ue_import_temple.py` 导入建筑并核对（331/331 通过）。
3. 按区域表给 Actor 打 Tag `DysisZone=<区域名>`（大纲名字打包后就没了，游戏里读 Tag）。
4. 摆天：DysisSky、SkyAtmosphere、SkyLight（实时捕捉）、HeightFog、PlayerStart（门廊台阶上）。


## 给程序的接口

**`UDysisTimeComponent`**（挂在玩家 Character 上，Tick Group = PostPhysics）

| | |
|---|---|
| `H`、`Sticky`、`bNight`、`Zone`、`FootCm` | 当前时刻、上一刻、是不是夜里、脚下区域、算时间用的脚底位置（只读） |
| `SetNight(bool)` | 接住最后一缕光的那一刻调用：`Sticky = H`（同灰盒 `catchLight`） |
| `SetForcedTime(H)` / `ClearForcedTime()` | 过场钉住时刻（同灰盒 `state.forceH`） |
| `SetZoneOverride("beam:b1")` / `ClearZoneOverride()` | 光柱、月石、影桥、虹桥这些“光做的路”站上去时告诉组件区域名 |
| `OnTimeChanged(H)` | H 变了就广播 |
| `SkyActor` | 要摆的天；不填就找关卡里第一个 `ADysisSkyActor` |
| `bFootOnFloorSurface` | 默认开：脚底高度取竖直往下打到的面（见“和交接文档不一样的地方”第 1 条） |

**`UDysisSkyLibrary`**（蓝图里在 Dysis|Sky 下）：`DysisSunDir(H)`、`DysisMoonDir(H)`、`DysisLightRotation(Dir)`、`DysisTimeAt(Zone, PosCm, bNight, Sticky)`、
`DysisZoneOf(Ground, FootCm)`、`DysisAzAlt(Dir)`、`DysisClockHours(H)`、`DysisConst("H_TOP")`（灰盒常数按名字取）。

**`ADysisSkyActor`**：`SetTime(H)`、`IsSunMain()`、`GetMainLightRotation()`、`GetSunDir()`、`GetMoonDir()`、`GetAltitudes()`、`GetMoonDiscOpacity()`；
`PreviewH` 在编辑器里直接拖就能看某个时刻的天；`SunLuxPerGreyboxUnit`、`MoonLuxPerGreyboxUnit`、颜色是给美术按 UE 曝光调的（切换时机和方向是写死的规则）。

**`ADysisHUD`（PIE 里你会看到的）**：开局主菜单（↑↓ 选、Enter 确认：开始游戏/设置/退出；设置页 Backspace 返回，菜单开着开场对话不开始）；
游戏内：交互提示、对话框（文本框图+按行首人名挂立绘）、通知（反馈与提示图）、右上角碎片收集（拿到哪颗亮哪颗）、F3 调试面板。字体思源宋体，缺资产逐处退回默认不崩。

**控制台（PIE 里）**：`Dysis.Go <X> <Y> <Z> [night]` 把脚底传送到 UE 厘米坐标，站稳后打印区域、H、钟点、主光、Pitch/Yaw；`Dysis.Where` 打印当前状态。

## 实现说明

- 所有计算用 double；`DysisTimeAt` 按交接文档用 float 进出（H 最大 ~200，float 误差 ~1e-5，远小于 1e-3）。
- 天：两盏平行光各自一直朝着自己的天体（太阳 / 月亮）。太阳高度 > −0.8° 时太阳是主光、月光强度 0；否则反过来。亮度按灰盒公式，再乘换算系数。
- 月亮圆盘是一个无光照半透明的球，放在月亮方向 4 km 处，角直径 1.36°（和灰盒 8000 远、半径 95 一样）；不透明度按灰盒公式。星空没做（可选）。
- 区域：读 Actor 的 Tag `DysisZone=<区域名>`；`SM_Wall` 的 Tag 是 `wall`，脚底半径 15.5–16.8 m 时按方位角分出 `tun:TS`（250°–330°）/ `tun:TR`（105°–185°）；没有 Tag 的按脚底高度兜底（灰盒 `zoneOf`）。
- 每帧：强制时刻 > 在地上按脚下算 > 空中保持 `Sticky`；没有平滑。

## 要注意的地方

1. **脚底高度**：从脚上 0.4 m 竖直往下打一条射线，取正下方的面（打不到才用“胶囊底 − 离地距离”）。
2. **屋顶升降踏步**（`SM_Mech_RoofSteps_Up*`）是按**升起**的姿态导入的（第 1 级 30.75 → 最高一级 ~38 m，见机关清单的轴心列）。`pie_spots.json` 里屋顶的点高度都是 30.3，站过去会掉到四层。环道的时间只看方位角，所以测试脚本对 `crown` 的点改成站到那个 XY 上真实的踏面。踏步怎么升降是机关方面需要处理的地方。
3. **虹桥** `SM_Mech_RainbowBridge_Light` 是不挡东西的，站不上去，`rainbow` 区域只能由程序用 `SetZoneOverride` 给；`shadowbr`（影桥）没有网格，同样。
4. **夜里的亮度**：UE 的自动曝光会把月光下的殿内提得和白天差不多亮（见截图 20）。方向和切换时机是对的；夜里要多暗请美术调 `MoonLuxPerGreyboxUnit` 或给关卡加 PostProcessVolume 限制曝光范围。
5. 三块红色方位标记是导入时确认位置方向用的，可以删了。
