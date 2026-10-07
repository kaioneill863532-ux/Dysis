# 狄西斯的日落回廊 · 音效事件表（唯一来源）
#
# 每一项 = 游戏里的一个“事件”（例如 Foot.Stone.Walk = 石头上走一步），对应 manifest.json 里的一组 WAV。
# gen_sfx_table.py 用它生成 Source/Dysis/Audio/DysisSfxDefaults.cpp（C++ 默认表）；
# 运行时每一项的音量、快慢（音高）等都能在 UE 的 Project Settings → Game → Dysis 音效 里拖滑块调。
#
# 字段：
#   key      事件名（代码里用它播）
#   label    中文说明（设置页里显示）
#   files    manifest 里的文件名（正则，按 manifest 顺序收集；带编号的“第几级/第几格”按这个顺序取）
#   cat      分类：Footsteps 脚步 / Player 玩家动作 / Mechanism 机关 / Story 剧情与奇观 / Ambience 环境 / UI 界面
#   d2       True = 2D（不分方位，玩家自己的声音、界面、标题）；False = 3D（放在物体上，随距离衰减）
#   inner    3D：满音量半径（厘米）；falloff：从 inner 往外多少厘米衰减到无声
#   vol / pitch           默认音量 / 快慢（音高，1 = 原速）
#   vj / pj               每次随机的音量 / 音高幅度（±）
#   norep                 随机时不和上一次重复
#   cd                    同一项两次之间最短间隔（秒）
#   maxn                  同时最多几个在响
#   loop                  循环（由导入脚本把 SoundWave 设成 Looping）

M = 100.0  # 1 米 = 100 厘米

EVENTS = [
    # ── 脚步（2D：玩家自己的脚）──
    dict(key="Foot.Stone.Walk", label="脚步·石头·走", files=r"SFX_Footstep_Stone_Walk_\d+", cat="Footsteps", d2=True, pj=0.04, vj=0.05, maxn=3),
    dict(key="Foot.Stone.Run", label="脚步·石头·快走", files=r"SFX_Footstep_Stone_Run_\d+", cat="Footsteps", d2=True, pj=0.04, vj=0.05, maxn=3),
    dict(key="Foot.Stone.Scuff", label="脚步·石头·急停蹭地", files=r"SFX_Footstep_Stone_Scuff_\d+", cat="Footsteps", d2=True, vol=0.8, pj=0.03, cd=0.8, maxn=1),
    dict(key="Foot.Light.Walk", label="脚步·光路·走", files=r"SFX_Footstep_Light_Walk_\d+", cat="Footsteps", d2=True, vj=0.05, maxn=3),
    dict(key="Foot.Light.Run", label="脚步·光路·快走", files=r"SFX_Footstep_Light_Run_\d+", cat="Footsteps", d2=True, vj=0.05, maxn=3),
    dict(key="Foot.Moon.Walk", label="脚步·月石/月桥/夜光石·走", files=r"SFX_Footstep_Moonstone_Walk_\d+", cat="Footsteps", d2=True, vj=0.05, maxn=3),
    dict(key="Foot.Moon.Run", label="脚步·月石/月桥/夜光石·快走", files=r"SFX_Footstep_Moonstone_Run_\d+", cat="Footsteps", d2=True, vj=0.05, maxn=3),
    dict(key="Foot.Shadow.Walk", label="脚步·影桥/月光踏片/月光大道·走", files=r"SFX_Footstep_ShadowBridge_Walk_\d+", cat="Footsteps", d2=True, vj=0.05, maxn=3),
    dict(key="Foot.Shadow.Run", label="脚步·影桥/月光踏片/月光大道·快走", files=r"SFX_Footstep_ShadowBridge_Run_\d+", cat="Footsteps", d2=True, vj=0.05, maxn=3),
    dict(key="Foot.Bronze.Walk", label="脚步·青铜·走", files=r"SFX_Footstep_Bronze_Walk_\d+", cat="Footsteps", d2=True, pj=0.03, vj=0.05, maxn=3),
    dict(key="Foot.Bronze.Run", label="脚步·青铜·快走", files=r"SFX_Footstep_Bronze_Run_\d+", cat="Footsteps", d2=True, pj=0.03, vj=0.05, maxn=3),
    dict(key="Foot.Water.Walk", label="脚步·浅水·走", files=r"SFX_Footstep_Water_Walk_\d+", cat="Footsteps", d2=True, pj=0.04, vj=0.05, maxn=3),
    dict(key="Foot.Water.Run", label="脚步·浅水·快走", files=r"SFX_Footstep_Water_Run_\d+", cat="Footsteps", d2=True, pj=0.04, vj=0.05, maxn=3),
    dict(key="Foot.WetStone.Walk", label="脚步·湿石（走和快走共用）", files=r"SFX_Footstep_WetStone_Walk_\d+", cat="Footsteps", d2=True, pj=0.04, vj=0.05, maxn=3),

    # ── 玩家动作（2D）──
    dict(key="Player.Jump", label="起跳", files=r"SFX_Jump_\d+", cat="Player", d2=True, pj=0.03, maxn=1, cd=0.15),
    dict(key="Player.Land.Stone", label="落地·石头", files=r"SFX_Land_Stone_\d+", cat="Player", d2=True, pj=0.03, maxn=1, cd=0.15),
    dict(key="Player.Land.Light", label="落地·光路/月石", files=r"SFX_Land_Light_\d+", cat="Player", d2=True, maxn=1, cd=0.15),
    dict(key="Player.Land.Bronze", label="落地·青铜", files=r"SFX_Land_Bronze_\d+", cat="Player", d2=True, maxn=1, cd=0.15),
    dict(key="Player.Fall.Start", label="掉落开始", files=r"SFX_Fall_Start", cat="Player", d2=True, maxn=1, cd=1.0),
    dict(key="Player.Fall.Loop", label="掉落中（循环）", files=r"SFX_Fall_Loop", cat="Player", d2=True, loop=True, maxn=1),
    dict(key="Player.Respawn.Day", label="回到落脚点·白天", files=r"SFX_Respawn_Day", cat="Player", d2=True, maxn=1, cd=0.5),
    dict(key="Player.Respawn.Night", label="回到落脚点·夜里", files=r"SFX_Respawn_Night", cat="Player", d2=True, maxn=1, cd=0.5),

    # ── 光与月 ──
    dict(key="Light.Reveal", label="光路显形", files=r"SFX_LightPath_Reveal_\d+", cat="Story", inner=4*M, falloff=40*M, maxn=1, cd=1.0),
    dict(key="Light.RevealOpening", label="开场光路显形（标题音）", files=r"SFX_LightPath_Reveal_Opening", cat="Story", d2=True, maxn=1),
    dict(key="Moon.Stone.Appear", label="夜光石/月石亮起", files=r"SFX_Moonstone_Appear_\d+", cat="Story", inner=2*M, falloff=18*M, maxn=3, cd=0.12),
    dict(key="Moon.Stone.Vanish", label="夜光石/月石暗下去", files=r"SFX_Moonstone_Vanish_\d+", cat="Story", inner=2*M, falloff=18*M, maxn=3, cd=0.12),
    dict(key="Moon.Wall.Vanish", label="大块月石隐去（月亮浮雕、月之龛、双子墙）", files=r"SFX_Moonstone_Wall_Vanish", cat="Story", inner=4*M, falloff=35*M, maxn=2),

    # ── 环境循环 ──
    dict(key="Amb.Waterfall.Near", label="瀑布·近（循环）", files=r"SFX_Waterfall_Near_Loop", cat="Ambience", inner=6*M, falloff=19*M, loop=True, maxn=1),
    dict(key="Amb.Waterfall.Far", label="瀑布·远（循环）", files=r"SFX_Waterfall_Far_Loop", cat="Ambience", inner=15*M, falloff=45*M, loop=True, maxn=1),
    dict(key="Amb.Sea.Close", label="海浪·近（循环，殿外低处）", files=r"SFX_Sea_Waves_Close_Loop", cat="Ambience", d2=True, loop=True, maxn=1),
    dict(key="Amb.Sea.Far", label="海浪·远（循环，一直在）", files=r"SFX_Sea_Waves_Far_Loop", cat="Ambience", d2=True, loop=True, maxn=1),
    dict(key="Amb.RoofWind", label="海风（循环，越高越响）", files=r"SFX_Roof_Wind_Loop", cat="Ambience", d2=True, loop=True, maxn=1),
    dict(key="Amb.RoomTone", label="殿内空间底噪（循环）", files=r"SFX_Interior_RoomTone_Loop", cat="Ambience", d2=True, loop=True, maxn=1),
    dict(key="Amb.Pool", label="水池水面（循环）", files=r"SFX_Pool_Water_Loop", cat="Ambience", inner=8*M, falloff=12*M, loop=True, maxn=1),
    dict(key="Amb.DayBirds", label="白天鸟鸣（循环）", files=r"SFX_Day_Birds_Loop", cat="Ambience", d2=True, loop=True, maxn=1),
    dict(key="Amb.Mist", label="水雾（循环，开闸以后）", files=r"SFX_Mist_Loop", cat="Ambience", inner=10*M, falloff=15*M, loop=True, maxn=1),

    # ── 机关 ──
    dict(key="Mech.Sluice.Wheel", label="水闸轮转动", files=r"SFX_Sluice_Wheel_Turn", cat="Mechanism", inner=3*M, falloff=30*M, maxn=1),
    dict(key="Mech.Waterfall.Start", label="瀑布开始流", files=r"SFX_Waterfall_Start", cat="Mechanism", inner=8*M, falloff=45*M, maxn=1),
    dict(key="Mech.Waterfall.Stop", label="瀑布停", files=r"SFX_Waterfall_Stop", cat="Mechanism", inner=8*M, falloff=45*M, maxn=1),
    dict(key="Mech.Steps.Rise", label="屋顶踏步升起（第 1–16 级，各一个音）", files=r"SFX_Steps_Rise_\d+", cat="Mechanism", inner=3*M, falloff=25*M, norep=False, maxn=4),
    dict(key="Mech.Steps.Settle", label="下行梯落平（第 1–16 级）", files=r"SFX_Steps_Settle_\d+", cat="Mechanism", inner=3*M, falloff=25*M, norep=False, maxn=4),
    dict(key="Mech.Steps.MoveLoop", label="踏步移动中（循环，跟速度走）", files=r"SFX_Steps_Move_Loop", cat="Mechanism", inner=4*M, falloff=25*M, loop=True, maxn=1),
    dict(key="Mech.Lever.Pull", label="拉杆拉下", files=r"SFX_Lever_Pull", cat="Mechanism", inner=2*M, falloff=25*M, maxn=1),
    dict(key="Mech.Lever.Push", label="拉杆推回", files=r"SFX_Lever_Push", cat="Mechanism", inner=2*M, falloff=25*M, maxn=1),
    dict(key="Mech.Slab.Slide", label="石板滑动（远处传来）", files=r"SFX_Slab_Slide_\d+", cat="Mechanism", inner=5*M, falloff=45*M, maxn=2),
    dict(key="Mech.Iris.Open", label="光圈叶片打开", files=r"SFX_Iris_Open", cat="Mechanism", inner=6*M, falloff=35*M, maxn=1),
    dict(key="Mech.Iris.Close", label="光圈叶片合上", files=r"SFX_Iris_Close", cat="Mechanism", inner=6*M, falloff=35*M, maxn=1),
    dict(key="Mech.Iris.MoveLoop", label="光圈叶片慢慢动（循环）", files=r"SFX_Iris_Move_Loop", cat="Mechanism", inner=6*M, falloff=35*M, loop=True, maxn=1),
    dict(key="Mech.Gate.Open", label="屋顶桥门打开", files=r"SFX_Gate_Open", cat="Mechanism", inner=3*M, falloff=30*M, maxn=1),
    dict(key="Mech.Gate.Close", label="屋顶桥门关上", files=r"SFX_Gate_Close", cat="Mechanism", inner=3*M, falloff=30*M, maxn=1),
    dict(key="Mech.Statue.Turn", label="转动雕像底座（三相像、天鹅女神像）", files=r"SFX_Statue_Turn_\d+", cat="Mechanism", inner=2*M, falloff=25*M, pj=0.03, maxn=2, cd=0.2),
    dict(key="Mech.Statue.PullOut", label="拉出雕像", files=r"SFX_Statue_PullOut", cat="Mechanism", inner=2*M, falloff=25*M, maxn=1),
    dict(key="Mech.Statue.PushStart", label="推雕像·起步", files=r"SFX_Statue_Push_Start", cat="Mechanism", inner=2*M, falloff=25*M, maxn=1),
    dict(key="Mech.Statue.PushLoop", label="推雕像·推动中（循环）", files=r"SFX_Statue_Push_Loop", cat="Mechanism", inner=2*M, falloff=25*M, loop=True, maxn=1),
    dict(key="Mech.Statue.PushStop", label="推雕像·停下", files=r"SFX_Statue_Push_Stop", cat="Mechanism", inner=2*M, falloff=25*M, maxn=1),
    dict(key="Mech.MoonBridge.Extend", label="月桥伸出", files=r"SFX_MoonBridge_Extend", cat="Mechanism", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Mech.SwanRelief.Sink", label="天鹅浮雕下沉", files=r"SFX_SwanRelief_Sink", cat="Mechanism", inner=4*M, falloff=40*M, maxn=1),
    dict(key="Mech.Stairs.Lower", label="下行梯开始降下", files=r"SFX_Stairs_Lower", cat="Mechanism", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Mech.WallStairs.RevealTS", label="墙里楼梯显现·TS（天鹅解开）", files=r"SFX_WallStairs_Reveal_TS", cat="Mechanism", inner=4*M, falloff=40*M, maxn=1),
    dict(key="Mech.WallStairs.RevealTR", label="墙里楼梯显现·TR（月亮浮雕隐去）", files=r"SFX_WallStairs_Reveal_TR", cat="Mechanism", inner=4*M, falloff=40*M, maxn=1),
    dict(key="Mech.WallStairs.Window", label="墙里楼梯的窗（单块，默认不用）", files=r"SFX_WallStairs_Window_\d+", cat="Mechanism", inner=3*M, falloff=30*M, maxn=3),
    dict(key="Mech.HalfBridge.Extend", label="半桥伸出", files=r"SFX_HalfBridge_Extend", cat="Mechanism", inner=4*M, falloff=35*M, maxn=1),
    dict(key="Mech.Prism.Turn", label="棱镜转台转一格（红→紫 7 个音）", files=r"SFX_Prism_Turn_\d_\w+", cat="Mechanism", inner=2*M, falloff=25*M, norep=False, maxn=2),
    dict(key="Mech.Sill.Extend", label="窗下石沿伸出", files=r"SFX_Sill_Ledge_Extend", cat="Mechanism", inner=3*M, falloff=30*M, maxn=1),
    dict(key="Mech.Niche.DoorsOpen", label="虹之龛铜门打开", files=r"SFX_Niche_Doors_Open", cat="Mechanism", inner=2*M, falloff=20*M, maxn=1),
    dict(key="Mech.Prism.ColumnRise", label="棱镜铜柱升起", files=r"SFX_Prism_Column_Rise", cat="Mechanism", inner=3*M, falloff=30*M, maxn=1),
    dict(key="Mech.SunNiche.Open", label="日之龛开盖", files=r"SFX_SunNiche_Open", cat="Mechanism", inner=2*M, falloff=25*M, maxn=1),

    # ── 剧情与奇观 ──
    dict(key="Story.Selene.DroneLow", label="塞勒涅怀里的月亮·低音（循环）", files=r"SFX_Selene_MoonDrone_Low_Loop", cat="Story", inner=6*M, falloff=30*M, loop=True, maxn=1),
    dict(key="Story.Selene.DroneHigh", label="塞勒涅怀里的月亮·高音（循环）", files=r"SFX_Selene_MoonDrone_High_Loop", cat="Story", inner=6*M, falloff=30*M, loop=True, maxn=1),
    dict(key="Story.Selene.Lock", label="月光对准月亮（Lock）", files=r"SFX_Selene_MoonDrone_Lock", cat="Story", inner=6*M, falloff=40*M, maxn=1),
    dict(key="Story.Selene.Awaken", label="塞勒涅神像亮起", files=r"SFX_Selene_Awaken", cat="Story", inner=6*M, falloff=45*M, maxn=1),
    dict(key="Story.Twins.Light", label="双子亮起", files=r"SFX_Twins_Light", cat="Story", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Story.ShadowBridge.Join", label="影桥接上（月光大道出现）", files=r"SFX_ShadowBridge_Join", cat="Story", inner=6*M, falloff=35*M, maxn=1, cd=5.0),
    dict(key="Story.Apple.Place", label="放上金苹果", files=r"SFX_Apple_Place", cat="Story", inner=4*M, falloff=40*M, maxn=1),
    dict(key="Story.Apple.Take", label="取下金苹果（接住最后一缕光）", files=r"SFX_Apple_Take", cat="Story", d2=True, maxn=1),
    dict(key="Story.Apple.HoldLoop", label="捧着金苹果（循环）", files=r"SFX_Apple_Hold_Loop", cat="Story", d2=True, loop=True, maxn=1),
    dict(key="Story.Shard.Sun", label="拾取日之碎片", files=r"SFX_Shard_Pickup_Sun", cat="Story", d2=True, maxn=1),
    dict(key="Story.Shard.Moon", label="拾取月之碎片", files=r"SFX_Shard_Pickup_Moon", cat="Story", d2=True, maxn=1),
    dict(key="Story.Shard.Rainbow", label="拾取虹之碎片", files=r"SFX_Shard_Pickup_Rainbow", cat="Story", d2=True, maxn=1),
    dict(key="Story.Swan.Transform", label="女神像变天鹅", files=r"SFX_Swan_Transform", cat="Story", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Story.Swan.Revert", label="天鹅变回女神像", files=r"SFX_Swan_Revert", cat="Story", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Story.Waterfall.HoleOpen", label="瀑布水帘透开（还没接到玩法）", files=r"SFX_Waterfall_Hole_Open", cat="Story", inner=4*M, falloff=30*M, maxn=1),
    dict(key="Story.Waterfall.HoleClose", label="瀑布水帘合上（还没接到玩法）", files=r"SFX_Waterfall_Hole_Close", cat="Story", inner=4*M, falloff=30*M, maxn=1),
    dict(key="Story.IrisRelief.Awaken", label="伊莉丝浮雕醒来", files=r"SFX_IrisRelief_Awaken", cat="Story", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Story.RainbowBridge.Appear", label="彩虹桥出现", files=r"SFX_RainbowBridge_Appear", cat="Story", inner=6*M, falloff=45*M, maxn=1),
    dict(key="Story.Selene.Eyes", label="塞勒涅眼睛点亮", files=r"SFX_Selene_Eyes_Light", cat="Story", inner=5*M, falloff=40*M, maxn=1),
    dict(key="Story.Mirror.Awaken", label="三相像镜子醒来（镜面被阳光照到）", files=r"SFX_Mirror_Awaken", cat="Story", inner=4*M, falloff=35*M, maxn=1, cd=30.0),
    dict(key="Story.Dodecahedron", label="正十二面体与星座亮起（集齐三片）", files=r"SFX_Dodecahedron_Stars", cat="Story", d2=True, maxn=1),
    dict(key="Story.RainbowGate", label="虹门彩虹", files=r"SFX_RainbowGate_Rainbow", cat="Story", inner=3*M, falloff=25*M, maxn=1, cd=6.0),
    dict(key="Story.Selene.SleepTalk", label="塞勒涅梦话", files=r"SFX_Selene_SleepTalk_\d+", cat="Story", inner=3*M, falloff=12*M, maxn=1),

    # ── 界面（2D）──
    dict(key="UI.Hover", label="按钮悬停", files=r"SFX_UI_Hover_\d+", cat="UI", d2=True, maxn=2, cd=0.04),
    dict(key="UI.Confirm", label="确认", files=r"SFX_UI_Confirm", cat="UI", d2=True, maxn=2),
    dict(key="UI.Back", label="返回", files=r"SFX_UI_Back", cat="UI", d2=True, maxn=2),
    dict(key="UI.StartGame", label="开始游戏", files=r"SFX_UI_StartGame", cat="UI", d2=True, maxn=1),
    dict(key="UI.Title", label="关卡标题（序、日1–5、日落之后、月1–5）", files=r"SFX_Title_\w+", cat="UI", d2=True, norep=False, maxn=1),
    dict(key="UI.Prompt", label="互动提示出现", files=r"SFX_UI_Prompt_Appear_\d+", cat="UI", d2=True, maxn=1, cd=1.0),
    dict(key="UI.Text", label="提示文字出现", files=r"SFX_UI_Text_Appear_\d+", cat="UI", d2=True, maxn=1, cd=1.0),
    dict(key="UI.DialogueNext", label="对话推进（下一句）", files=r"SFX_UI_Dialogue_Next_\d+", cat="UI", d2=True, maxn=1, cd=0.1),
    dict(key="UI.ShardHover.Sun", label="界面·碰到日之碎片", files=r"SFX_UI_Shard_Hover_Sun", cat="UI", d2=True, maxn=1, cd=0.3),
    dict(key="UI.ShardHover.Moon", label="界面·碰到月之碎片", files=r"SFX_UI_Shard_Hover_Moon", cat="UI", d2=True, maxn=1, cd=0.3),
    dict(key="UI.ShardHover.Rainbow", label="界面·碰到虹之碎片", files=r"SFX_UI_Shard_Hover_Rainbow", cat="UI", d2=True, maxn=1, cd=0.3),
]

DEFAULTS = dict(cat="Mechanism", d2=False, inner=3*M, falloff=30*M, vol=1.0, pitch=1.0, vj=0.0, pj=0.0, norep=True, cd=0.0, maxn=4, loop=False)
