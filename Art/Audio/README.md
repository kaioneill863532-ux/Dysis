# 音效：一键导入、自动接入、在 UE 里调

265 个音效文件（48 kHz、16 位、干声），分成 101 项“事件”（例如“石头上走一步”“拉杆拉下”“光路显形”），已经按游戏里的触发条件接好了。程序只需要做下面三步。

## 一键接入（三步）

1. **拉文件、编译。** WAV 走 Git LFS，先 `git lfs pull`。再编译工程，C++ 在 `Source/Dysis/Audio/`。
2. **跑导入脚本。** 二选一：
   - 编辑器里：工具 → 执行 Python 脚本…，选 `Art/Audio/ue_import_sfx.py`。
   - 命令行（无头）：
     ```
     UnrealEditor-Cmd Dysis.uproject -run=pythonscript -script="<绝对路径>/Art/Audio/ue_import_sfx.py"
     ```

   脚本做了这几件事：
   - 把 WAV 导入成 `/Game/Dysis/Audio/SFX/<编号_名字>/` 下的 SoundWave，循环素材设成 Looping。
   - 在 `Dysis_Temple` 里放一个音效总调度 `DysisSfx`（大纲文件夹 `Dysis_Audio`），按大纲名字把位置和机关绑定好。
   - 保存关卡，最后检查一遍。

   脚本可以重跑：没改过的 WAV 会跳过，旧的 `DysisSfx` 会删掉重放。
3. **提交** `Content/Dysis/Audio/SFX/` 和 `Dysis_Temple`。

**验证：** PIE 里按 `~` 输入 `Dysis.Sfx.Debug 1`，然后走一走、跳一跳、拉一拉机关。屏幕左上角会列出每次播了哪一项、多大声、多快。

**以后重跑了 `ue_import_gameplay.py` 或 `ue_import_temple.py`（它们会删掉、重新摆放 Actor）：**
- 再跑一次 `ue_import_sfx.py`，把绑定存进关卡。
- 忘了也能用。总调度开局发现绑定的东西不在了，会按名字重新找一遍。

## 在 UE 里调每一项的音量、快慢

**Project Settings → Game → Dysis 音效**：

| 区域 | 内容 |
|---|---|
| 总音量 | 音效总音量，以及“脚步 / 玩家动作 / 机关 / 剧情与奇观 / 环境 / 界面”六个分类的音量。 |
| 每一项 | 101 行，展开一行就有这些滑块和开关：<br>• 音量<br>• 快慢（UE 里速度和音高一起变：1.2 = 快 20%、高约 3 个半音）<br>• 音量随机 ±、快慢随机 ±<br>• 2D / 3D、满音量半径、衰减距离<br>• 最短间隔、同时最多几个、启用<br>• 声音文件（可以换成别的 SoundWave/SoundCue） |
| 脚步 | 走一步、快走一步的距离；什么区域、什么名字的地面算什么材质；浅水高度；湿石离瀑布多近。 |
| 环境 | 哪些区域算殿外、屋顶高度、进出殿的过渡时间、入夜鸟鸣淡出时间、海风从多高开始响。 |

- **PIE 运行时直接拖滑块**：马上生效，并在耳边试听这一项（循环项会在正在响的循环上直接变）。
- **改动的保存**：自动存进 `Config/DefaultGame.ini`，提交这个文件就等于把调好的数值交给大家。
- **出厂值**：来自 `Source/Dysis/Audio/DysisSfxDefaults.cpp`。ini 里缺的项会自动补上。

控制台（PIE 里按 `~`）：

| 命令 | 用途 |
|---|---|
| `Dysis.Sfx.Debug 1` | 屏幕上显示每次播了哪一项 |
| `Dysis.Sfx.Play Mech.Lever.Pull` | 在耳边试听一项（后面加数字 = 第几个版本） |
| `Dysis.Sfx.List 脚步` | 列出所有项（可以按名字或中文过滤） |
| `Dysis.Sfx.Volume Footsteps 0.8` / `Dysis.Sfx.Volume Foot.Stone.Walk 0.8` | 改分类或单项音量 |
| `Dysis.Sfx.Pitch Mech.Statue.Turn 1.08` | 改快慢 |
| `Dysis.Sfx.Enable Story.Selene.SleepTalk 0` | 关掉一项 |
| `Dysis.Sfx.Check` | 检查每个文件都导入了 |
| `Dysis.Sfx.Save` / `Dysis.Sfx.ResetAll` | 存进 ini / 恢复出厂值 |

## 什么时候响（已经接好的触发条件）

总调度 `ADysisSfxDirector` 不改玩法代码。它每秒 30 次看各个玩法 Actor 的公开状态，状态一变就在对应位置播放：
- 拉杆有没有拉下、机关有没有激活、光路能不能走、转到第几格……
- 接光、拿碎片、靛色入眼、虹醒这四件事，直接订阅它们已有的事件。

玩家脚步由它挂到玩家身上的 `UDysisFootstepComponent` 负责。玩家类本身没有改。

| 时刻 | 播什么（事件名） | 依据 |
|---|---|---|
| 走路、快走（Shift） | `Foot.<材质>.Walk/Run`，每 225 / 300 cm 一步 | 有移动输入、在地上、水平速度 > 60 |
| 快走里急停（石头上） | `Foot.Stone.Scuff` | 一半概率 |
| 起跳（含 coyote 跳） | `Player.Jump` | `JumpCurrentCount` 增加 |
| 落地 | `Player.Land.Stone/Light/Bronze`（浅水用水花） | 空中超过 0.6 s 的落地会更响 |
| 掉落 | `Player.Fall.Start` + `Player.Fall.Loop`，落地或回档时停 | 空中 > 0.45 s，下落速度 > 700，已经掉了 2.5 m |
| 回到落脚点 / 光被挡退回原位 | `Player.Respawn.Day/Night` | 一帧里挪了很远（`RespawnAtFoothold` / `RetreatFromBeam`） |
| 光路显形 | `Light.Reveal`（在光的中点） | 光柱 `IsWalkableNow()` 从否变是（灭了至少 0.5 s 再亮才算） |
| 开场光路 | `Light.RevealOpening` | `bPrologueBeam` 的光第一次能走 |
| 拉杆 A、B | `Mech.Lever.Pull/Push` | `IsPulled()` 变化 |
| 水闸 | `Mech.Sluice.Wheel` → 2.2 s `Mech.Waterfall.Start` → 4.0 s 接瀑布近、远两条循环；水雾淡入 | 有 `SluiceName` 的拉杆；水面 `FlowOut` |
| 关闸 | `Mech.Waterfall.Stop`，瀑布循环淡出 | 水面离开 `FlowOut` |
| 推拉石板 | `Mech.Slab.Slide` | 石板开始动 |
| 屋顶上行踏步 | `Mech.Steps.Rise` 第 1–16 个音，一个接一个；移动中 `Mech.Steps.MoveLoop`（音量跟速度） | 踏步升起的进度每过 1/16 |
| 入夜下行梯 | `Mech.Stairs.Lower`，每级落平 `Mech.Steps.Settle` 第 k 个音 | 下行踏步开始动、第 k 级停下 |
| 三相像转一格 | `Mech.Statue.Turn` | `GetCurrentSlot()` 变化 |
| 三相像镜子醒来 | `Story.Mirror.Awaken` | 白天镜面重新被照亮一半以上 |
| 日之龛开盖 | `Mech.SunNiche.Open` | `Mech_SunNicheLid` 激活 |
| 棱镜转一格 | `Mech.Prism.Turn` 第 1–7 个音（红→紫） | `GetCurrentSlot()` 变化（第 8 格用转台声） |
| 靛色入眼 | `Story.Selene.Eyes`（塞勒涅浮雕） | `OnIndigoOnTarget` |
| 塞勒涅梦话 | `Story.Selene.SleepTalk`，隔 10–18 s 一段 | 白天、还没醒、人在浮雕 7 m 内 |
| 虹醒 | `Story.IrisRelief.Awaken` → 1.2 s `Story.RainbowBridge.Appear` | `OnAligned` |
| 窗下石沿、棱镜铜柱 | `Mech.Sill.Extend`、`Mech.Prism.ColumnRise` | 对应的机关驱动激活 |
| 拿碎片 | `Story.Shard.Sun/Moon/Rainbow`；虹之龛先 `Mech.Niche.DoorsOpen` | `OnCollected` |
| 三片集齐 | 3.6 s 后 `Story.Dodecahedron` | 存档 `AllNichesCollected()` |
| 穿过虹门 | `Story.RainbowGate` | 白天走进虹门 3 m 内，每次经过一次 |
| 接住最后一缕光 | `Mech.Gate.Close`（桥门）；5 s 后“日落之后·入夜”标题 | `OnCaught` |
| 取下金苹果 | `Story.Apple.Take`，之后一直 `Story.Apple.HoldLoop` | 苹果 `State` → `Carried` |
| 放上金苹果 | 循环 2 s 淡出 + `Story.Apple.Place` | 苹果 `State` → `Placed`（`Place()` 现在还没有人调用，接上就有声） |
| 夜光石亮起 / 暗下去 | `Moon.Stone.Appear` / `Moon.Stone.Vanish` | `GetGlow()` 到 0.95 / 掉到 `WalkableGlow` 以下 |
| 月2：转女神像 → 变天鹅、浮雕沉、石门沉、TS 楼梯显现 | `Mech.Statue.Turn`（快 8%）、`Story.Swan.Transform`、`Mech.SwanRelief.Sink`、`Mech.Slab.Slide`、`Mech.WallStairs.RevealTS` | 对应机关驱动激活 |
| 月3：月亮浮雕隐去、TR 楼梯显现、月之龛透开 | `Moon.Wall.Vanish`、`Mech.WallStairs.RevealTR` | 同上 |
| 月4：推波吕丢刻斯 → 双子亮起 → 月桥伸出 | `Mech.Statue.PushStart/PushLoop/PushStop`、`Story.Twins.Light`、1.4 s 后 `Mech.MoonBridge.Extend` | 同上 |
| 月光大道接上 | `Story.ShadowBridge.Join` | 水面变 `Ripple` |
| 月5：月桥上、夜里 | `Story.Selene.DroneLow/High` 两条循环（女神像） | 区域 `moonbr` |
| 月5：半桥 | `Story.Selene.Lock` → 1 s `Story.Selene.Awaken` → 2.4 s `Mech.HalfBridge.Extend` | `Mech_HalfBridge` 激活 |
| 桥门打开 | `Mech.Gate.Open` | 接光前桥门转动（现在导入姿态就是开着的，接上开门动画以后自动有声） |
| 光圈叶片动 | `Mech.Iris.MoveLoop` | 叶片网格在动（现在还没有代码驱动它们） |
| 关卡标题 | `UI.Title`：序、日1–日5、月2–月5 | 第一次走进那一关的区域（灰盒 `levelOf` 的表） |
| 互动提示出现 | `UI.Prompt` | 准星对着一个有提示的东西 |
| 提示文字出现 | `UI.Text` | `ADysisHUD::ShowNotification`（HUD 加了一个静态广播，三行） |
| 对话下一句 | `UI.DialogueNext` | `ActiveDialogue->CurrentLine` 增加 |

环境声（都会自动混音）：

| 声音 | 怎么响 |
|---|---|
| 远处海浪 | 一直在；进殿压低、低通到 800 Hz |
| 近处海浪 | 殿外，越低越响 |
| 海风 | 越高越响（15–33 m）；进殿压低、低通到 600 Hz |
| 殿内底噪 | 殿内 |
| 白天鸟鸣 | 白天；入夜 8 s 淡出 |
| 水池 | 3D，在水池；夜里响一点 |
| 瀑布近 / 远、水雾 | 开闸以后 |

殿内外按时间系统的区域名判断：`out`、`beam:isle`、`crown`、`rbridge`，以及屋顶以上，算殿外。

脚下材质按下面的顺序判断：
1. 光柱 → 光路。
2. 夜光石 → 月石。
3. 区域名：
   - `beam:`、`rainbow` → 光路；
   - `moonbr` → 月石；
   - `shadowbr`、`gbridge` → 影桥。
4. 地面网格的名字：
   - MoonBridge 等 → 月石；
   - Bronze、Copper、`_Curb`、Armillary 等 → 青铜。
5. 水面以下：夜里是影桥，白天是浅水。
6. 开闸以后离瀑布 7 m 内 → 湿石。
7. 以上都不是 → 石头。

规则都能在设置页里加。

## 代码里自己播（新玩法、UMG）

```cpp
#include "Audio/DysisSfxSubsystem.h"
if (UDysisSfxSubsystem* Sfx = UDysisSfxSubsystem::Get(this))
{
    Sfx->Play(TEXT("Mech.Iris.Open"), GetActorLocation());          // 一次性（2D 的项忽略位置）
    Sfx->PlayDelayed(1.2f, TEXT("Story.Swan.Revert"), Loc);          // 过一会儿播
    Sfx->StartLoop(TEXT("MyLoop"), TEXT("Mech.Iris.MoveLoop"), Loc);  // 循环：SetLoopScale / SetLoopLowPass / StopLoop
}
```

蓝图和 UMG：
- `Play Dysis Sfx`
- `Play Dysis UI Sound`：悬停、确认、返回、开始游戏、碰到日/月/虹碎片。按钮的 OnHovered 和 OnClicked 里调它。
- `Start/Stop Dysis Sfx Loop`

表里有，但还没有玩法可以接的几项（等对应功能做出来，一行 `Play` 就有声）：
- `Mech.Iris.Open/Close`
- `Mech.Statue.PullOut`
- `Story.Swan.Revert`
- `Story.Waterfall.HoleOpen/Close`
- `Mech.WallStairs.Window`（已经用了整段的 RevealTS/TR，默认不用）

## 测试

- UE 自动化测试（Session Frontend → Automation → `Dysis.Sfx`）：
  - `Dysis.Sfx.Table`：101 项、265 个文件、名字不重复，代码里用到的每个事件名都在表里。
  - `Dysis.Sfx.Assets`：每个文件都导入了，Looping 对。需要先跑导入脚本。
  - `Dysis.Sfx.Levels`：区域 → 关卡标题的对照和灰盒一致。
- 没有 UE 的机器上：
  - `bash Art/Audio/tools/offline_check/check_cpp.sh`：对音效 C++ 和它引用的工程头文件做 clang 类型检查，警告当错误。
  - `python3 Art/Audio/tools/offline_check/test_import_script.py`：用假的 `unreal` 模块把导入脚本跑两遍并核对。

这两项都只是替身检查，最终以 UE 里编译、运行为准。

## 要在 UE 里确认的地方

这一版是在没有 UE 的环境里写的，**还没有在 UE 里编译和运行过**。上面两项离线检查都通过了。接入后请先：
1. **编译。** 如果有报错，多半是引擎 API 和 5.8 的细节差异，按报错改即可。
2. **跑导入脚本，确认最后一行是“全部通过”，并看它打印的绑定情况。** 如果有“缺锚点”或“没绑上”，说明关卡里的大纲名字和机关清单不一样：
   - 在 `DysisSfx` 的 Details 里手动指一下；
   - 或者改名字以后点“自动绑定”。
3. **PIE 里开 `Dysis.Sfx.Debug 1`，把序到月5 走一遍。** 夜里那些机关的触发现在是 v1 近似（入夜就动），声音跟着它们走；以后触发条件改了，声音会自动跟上。
4. **青铜地面在哪。** 现在按名字（Bronze、Copper、铜沿 `_Curb`、浑天仪）判断；有别的青铜地面，在设置页“按名字判定材质”里加一行。

## 文件

| 路径 | 是什么 |
|---|---|
| `Art/Audio/SFX/<编号_名字>/*.wav` | 265 个音效源文件（LFS） |
| `Art/Audio/manifest.json` | 文件清单：哪一条、循环与否、响度、原始的触发说明 |
| `Art/Audio/ir/IR_Temple_Rotunda.wav` | 石头圆殿的脉冲响应（要做卷积混响时用） |
| `Art/Audio/ue_import_sfx.py` | 一键导入 + 接入 |
| `Art/Audio/tools/sfx_events.py` | 事件表：事件名、中文说明、分类、2D/3D、衰减、默认音量和快慢 |
| `Art/Audio/tools/gen_sfx_table.py` | 由事件表和 manifest 生成 `DysisSfxDefaults.cpp`（换了 WAV 或加了事件就重跑） |
| `Art/Audio/tools/offline_check/` | 没有 UE 时的检查工具 |
| `Source/Dysis/Audio/DysisSfxSettings.*` | 设置页（每一项的滑块） |
| `Source/Dysis/Audio/DysisSfxSubsystem.*` | 播放：随机不重复、冷却、并发上限、循环、控制台命令、蓝图库 |
| `Source/Dysis/Audio/DysisSfxDirector.*` | 总调度：看玩法状态、环境混音、关卡标题、界面音 |
| `Source/Dysis/Audio/DysisFootstepComponent.*` | 脚步、起跳、落地、掉落、回档 |
| `Source/Dysis/Audio/DysisSfxDefaults.cpp` | 出厂表（生成的，不要手改） |
| `Source/Dysis/Audio/DysisSfxTests.cpp` | 自动化测试 |
