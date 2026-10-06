# 狄西斯的日落回廊 v0.12 · 把玩法 Actor 摆进 Dysis_Temple 关卡
#
# ue_import_temple.py 摆的是建筑与机关的"网格"；本脚本摆的是玩法侧的 C++ Actor：
# 光柱（beam:b1/b2/h1-h3/镜光/圆眼/开场）、月光踏片与月光大道、屋顶升降踏步驱动、
# 推拉石板+拉杆、月石/月桥/半桥/封门石板/外窗石块驱动、三相像镜源、棱镜、虹桥对齐、
# 三个时辰龛、金苹果、接光、接缝空气墙、BGM 管理器、开场导演。SkyActor 关卡里已有则跳过。
#
# 用法 A（Windows/Mac 编辑器里）：
#   打开关卡 Dysis_Temple → 工具 → 执行 Python 脚本…，选这个文件。
# 用法 B（无头命令行，Mac 上生成 umap 用）：
#   UnrealEditor-Cmd Dysis.uproject -run=pythonscript -script="本文件绝对路径"
#   （脚本自己加载并保存 Dysis_Temple；需要 Dysis 模块已编译，命令行会自己触发 UBT。）
#
# 幂等：重跑安全。旧的本脚本产物（大纲 Dysis_Gameplay 文件夹 / 带 Tag "DysisGameplay"）
#   先全部删掉再摆；WorldSettings 的 GameMode 每次重设。已有 DysisSky 不动。
# 数值来源：机关清单.md（轴心/怎么动）、施工图 v0.12（主光窗表、光路表、总平面）。
#   施工图 az/r/y → UE：X=100·r·cos(az)，Y=100·r·sin(az)，Z=100·y（厘米）。
# 注：夜侧机关的触发是 v1 近似（bTriggerOnNight / 镜源任意格），见各条目旁注释，后续在编辑器里微调。
import unreal, math

MAP = "/Game/Dysis/Maps/Dysis_Temple"
FOLDER = "Dysis_Gameplay"
TAG = "DysisGameplay"
AUDIO_DIR = "/Game/Dysis/Audio"

# ───────────────────────── 基础设施 ─────────────────────────

def log(msg):
    unreal.log("[DysisGameplay] " + str(msg))

def warn(msg):
    unreal.log_warning("[DysisGameplay] " + str(msg))

def az_ry(az_deg, r_m, y_m):
    """施工图 az/r/y（度/米/米）→ UE 世界坐标（厘米）。"""
    a = math.radians(az_deg)
    return unreal.Vector(100.0 * r_m * math.cos(a), 100.0 * r_m * math.sin(a), 100.0 * y_m)

def az_dir(az_deg):
    """方位角 → 水平单位向量（UE 世界系）。"""
    a = math.radians(az_deg)
    return unreal.Vector(math.cos(a), math.sin(a), 0.0)

actor_sub = None
try:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
except Exception:
    pass

def all_actors():
    if actor_sub:
        return actor_sub.get_all_level_actors()
    return unreal.EditorLevelLibrary.get_all_level_actors()

def load_class(name):
    """原生类优先 unreal.<Name>，退回 /Script/Dysis.<Name>。"""
    cls = getattr(unreal, name, None)
    if cls is None:
        cls = unreal.load_object(None, "/Script/Dysis." + name)
    if cls is None:
        raise RuntimeError("找不到 C++ 类 Dysis.%s（模块编译了吗？）" % name)
    return cls

def find_by_label(label):
    """按大纲标签精确找 Actor（ue_import_temple.py 摆的网格都按机关清单 UE 名字命名）。"""
    for a in all_actors():
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            continue
    return None

def find_by_class(cls_name):
    out = []
    for a in all_actors():
        try:
            if a.get_class().get_name() == cls_name:
                out.append(a)
        except Exception:
            continue
    return out

def make_movable(actor):
    """驱动器要动的网格必须是 Movable（导入脚本已设，这里兜底再设一次）。
    AStaticMeshActor 没有 get_root_component()，走 static_mesh_component。"""
    try:
        comp = actor.get_editor_property("static_mesh_component")
        if comp is None:
            comp = actor.get_editor_property("root_component")
        if comp and comp.get_editor_property("mobility") != unreal.ComponentMobility.MOVABLE:
            comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    except Exception as e:
        warn("%s 设 Movable 失败：%s" % (actor.get_actor_label(), e))

spawned = []

def spawn(class_name, label, loc=None, rot_yaw=0.0):
    cls = load_class(class_name)
    loc = loc or unreal.Vector(0, 0, 0)
    rot = unreal.Rotator(0.0, 0.0, rot_yaw)
    if actor_sub:
        ac = actor_sub.spawn_actor_from_class(cls, loc, rot)
    else:
        ac = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    ac.set_actor_label(label)
    try:
        ac.set_folder_path(unreal.Name(FOLDER))
    except Exception:
        pass
    try:
        ac.tags = list(ac.tags) + [TAG]
    except Exception:
        pass
    spawned.append(ac)
    return ac

def setp(actor, prop, value):
    """设一个编辑器属性；失败只警告。
    布尔属性 UE Python 会剥掉 C++ 的 b 前缀（bHidden→hidden），所以 b_xxx 失败自动试 xxx。"""
    try:
        actor.set_editor_property(prop, value)
    except Exception:
        if prop.startswith("b_"):
            alt = prop[2:]
            try:
                actor.set_editor_property(alt, value)
                return
            except Exception:
                pass
        warn("%s.%s 设置失败" % (actor.get_actor_label(), prop))

def attach_child(parent, child):
    """把已有网格 Actor 挂到玩法 Actor 下（保持世界位置）——父动子随。"""
    try:
        child.attach_to_actor(parent, unreal.Name("None"),
                              unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD,
                              unreal.AttachmentRule.KEEP_WORLD, False)
    except Exception as e:
        warn("挂接 %s → %s 失败：%s" % (child.get_actor_label(), parent.get_actor_label(), e))

def cleanup_previous():
    removed = 0
    for a in list(all_actors()):
        try:
            in_folder = FOLDER in str(a.get_folder_path() or "")
            has_tag = TAG in list(a.tags)
        except Exception:
            continue
        if in_folder or has_tag:
            a.destroy_actor()
            removed += 1
    if removed:
        log("清理旧玩法 Actor %d 个" % removed)

# ───────────────────────── 开始 ─────────────────────────

def run():
    # 无头命令行进来没有开地图：先加载 Dysis_Temple（编辑器里已开同名图则不重载）
    world = None
    need_load = True
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world and str(world.get_path_name()).startswith(MAP + "."):
            need_load = False
    except Exception:
        world = None
    if need_load:
        unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("拿不到 World（地图加载失败？）")

    cleanup_previous()

    # 0) WorldSettings：GameMode = ADysisGameMode（HUD/DefaultPawn 全在这条链上）
    ws = world.get_world_settings()
    if ws is None:
        raise RuntimeError("拿不到 WorldSettings")
    gm = load_class("DysisGameMode")
    ok = False
    for prop in ("game_mode_override", "default_game_mode"):
        try:
            ws.set_editor_property(prop, gm)
            ok = True
            break
        except Exception:
            continue
    log("GameModeOverride = DysisGameMode " + ("OK" if ok else "失败(两个属性名都没写上)"))

    # 1) SkyActor：关卡里已有（搭天时摆的）就跳过
    skies = find_by_class("DysisSkyActor")
    if skies:
        log("DysisSkyActor 已有 %d 个，跳过" % len(skies))
    else:
        spawn("DysisSkyActor", "DysisSky")
        warn("关卡里原来没有 DysisSkyActor，补了一个（太阳/月亮/月盘自建）")

    # 2) BGM 管理器（日曲/夜曲）。.wav 只是磁盘上的原始文件——没在完整编辑器里打开过
    #    就不是资产包，load_asset 返回 None；这里用 AssetImportTask 正式导入（wav 的
    #    包格式就是 .wav 本身，导入后原地成资产）。
    def load_or_import_sound(name):
        path = AUDIO_DIR + "/" + name
        asset = unreal.load_asset(path)
        if asset:
            return asset
        src = unreal.Paths.project_content_dir() + "Dysis/Audio/" + name + ".wav"
        task = unreal.AssetImportTask()
        task.filename = src
        task.destination_path = AUDIO_DIR
        task.destination_name = name
        task.automated = True
        task.save = True
        task.replace_existing = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        return unreal.load_asset(path)

    music = spawn("DysisMusicManager", "DysisMusic")
    day_cue = load_or_import_sound("M_Day_Sicilienne_Long")
    night_cue = load_or_import_sound("M_Night_ClairDeLune_Full")
    if day_cue:
        setp(music, "day_music", day_cue)
    else:
        warn("M_Day_Sicilienne_Long 导入/加载都失败")
    if night_cue:
        setp(music, "night_music", night_cue)
    else:
        warn("M_Night_ClairDeLune_Full 导入/加载都失败")

    # 3) 开场导演（延迟 2s 自动播开场对话、钉 H_I）
    spawn("DysisPrologueDirector", "DysisPrologue")

    # ── 光柱（施工图·光路表：起点=方位/r/高、宽；窗照亮/坡度/雾阈值用类默认=灰盒值）──
    # (标签, 窗心az/r/y, 区域, 截面半宽cm, 兜底地面y_m, 额外设置dict)
    beams = [
        ("Beam_Prologue", (23.2, 31.8, -0.8),  "beam:isle",   87.0, -12.0, {"b_prologue_beam": True}),
        ("Beam_b1",       (164.0, 15.4, 9.4),  "beam:b1",     120.0,  0.0, {}),
        ("Beam_b2",       (250.0, 15.5, 18.3), "beam:b2",      90.0,  6.0, {}),
        ("Beam_h1",       (180.0, 15.5, 26.4), "beam:h1",     100.0, 14.5, {}),
        ("Beam_h2",       (193.0, 15.5, 26.4), "beam:h2",     100.0, 14.5, {}),
        ("Beam_h3",       (206.0, 15.5, 26.4), "beam:h3",     100.0, 14.5, {}),
        ("Beam_Mirror",   (282.4, 13.3, 15.7), "beam:mirror",  91.0, 14.5,
            {"b_from_mirror": True, "b_mirror_beam": True}),   # MirrorSource 下面统一回填
        ("Beam_Oculus",   (218.1, 1.8, 30.4),  "beam:oculus",  95.0, 23.0, {"b_oculus_beam": True}),
    ]
    beam_actors = {}
    for label, (az, r, y), zone, half_w, floor_y, extra in beams:
        loc = az_ry(az, r, y)
        b = spawn("DysisBeamActor", label, loc)
        setp(b, "window_cm", loc)                 # 光路锚点属性（光柱数学用它，不是 Actor 变换）
        setp(b, "beam_zone", unreal.Name(zone))
        setp(b, "box_extent_cm", unreal.Vector(50.0, half_w, 120.0))
        setp(b, "fallback_floor_y", floor_y)
        for k, v in extra.items():
            setp(b, k, v)
        beam_actors[label] = b

    # ── 水面三态 + 月光大道（水庭水面 y=-0.45、池半径 10.2m；初始态=Calm 是类默认）──
    water = spawn("DysisMoonSurface", "DysisWater")
    setp(water, "box_min_cm", unreal.Vector(-1020, -1020, -100))
    setp(water, "box_max_cm", unreal.Vector(1020, 1020, 100))

    moon_path = spawn("DysisMoonPath", "DysisMoonPath")
    setp(moon_path, "target_water", water)
    setp(moon_path, "surface_z_cm", -45.0)
    setp(moon_path, "zone_name", unreal.Name("gbridge"))

    # ── 屋顶升降踏步（自己按名字找 SM_Mech_RoofSteps_*，默认参数=机关清单）──
    spawn("DysisRoofSteps", "DysisRoofSteps", unreal.Vector(0, 0, 3030))

    # ── 接缝空气墙（116°–140°，BeginPlay 自算盒子）──
    spawn("DysisSeamWall", "DysisSeamWall", az_ry(128.0, 16.3, 15.0))

    # ── 拉杆（日2 两根 + 水闸一根）：C++ Actor 是转轴逻辑件，网格臂挂到它下面跟着转 ──
    lever_a = spawn("DysisLeverActor", "DysisLeverA", unreal.Vector(-1045, -1083, 645), -44.0)
    lever_b = spawn("DysisLeverActor", "DysisLeverB", unreal.Vector(490, 1423, 645), 161.0)
    # 水闸（总平面：机关在 113.8°/11.6m/0.9m；拉下 → 水面切 FlowOut 起雾，光柱显形）
    lever_sluice = spawn("DysisLeverActor", "DysisLeverSluice", az_ry(113.8, 11.6, 0.9), 66.2)
    setp(lever_sluice, "sluice_name", unreal.Name("sluice"))
    setp(lever_sluice, "water_state_on_change", unreal.Name("FlowOut"))
    for arm_label, lever in (("SM_Mech_LeverA_Arm", lever_a), ("SM_Mech_LeverB_Arm", lever_b)):
        arm = find_by_label(arm_label)
        if arm:
            make_movable(arm)
            attach_child(lever, arm)
        else:
            warn("找不到 %s（拉杆臂网格不挂也能玩，只是扳杆不动）" % arm_label)

    # ── 推拉石板（日2）：TargetMesh=已有面板网格；机关清单 az 一对 ──
    for label, mesh_label, az0, az1, r_m, y_m, go_end in (
        ("DysisSlider_b2",   "SM_Mech_Slider_Panel_b2",   245.72, 254.27, 17.20, 19.85, True),
        ("DysisSlider_iris", "SM_Mech_Slider_Panel_iris", 171.43, 179.78, 17.20, 19.05, False),  # 拉杆A拉下→滑来挡窗
    ):
        s = spawn("DysisSliderActor", label, az_ry((az0 + az1) / 2.0, r_m, y_m))
        panel = find_by_label(mesh_label)
        if panel:
            make_movable(panel)
            setp(s, "target_mesh", panel)
        else:
            warn("找不到 %s，%s 没有目标网格" % (mesh_label, label))
        setp(s, "lever", lever_a)
        setp(s, "az_start", az0)
        setp(s, "az_end", az1)
        setp(s, "radius_m", r_m)
        setp(s, "height_m", y_m)
        setp(s, "b_go_to_end_when_pulled", go_end)

    # ── 三相像镜源（日3/月3 镜光）：摆镜心（机关清单 SM_Mech_Mirror_Mirror 轴心）──
    mirror = spawn("DysisMirrorSource", "DysisMirrorSource", unreal.Vector(323, -1273, 1635))
    setp(beam_actors["Beam_Mirror"], "mirror_source", mirror)

    # ── 棱镜转台（虹）：摆棱镜玻璃处（164.78°/15.2/18.08）──
    spawn("DysisPrismActor", "DysisPrism", unreal.Vector(-1466, 399, 1808))

    # ── 虹桥对齐（日2 伊莉丝浮雕：站浮雕前 0.7m、影头入人形）──
    rainbow = spawn("DysisRainbowAlign", "DysisRainbowAlign")
    setp(rainbow, "stand_point_cm", az_ry(79.0, 15.44 - 0.7, 6.0))     # 离墙 0.7m
    setp(rainbow, "relief_head_cm", az_ry(79.0, 15.44, 8.3))           # 人形头部（浮雕板 2.7m 高，头部 ~2.3m 处）
    setp(rainbow, "plane_normal", unreal.Vector(-math.cos(math.radians(79.0)), -math.sin(math.radians(79.0)), 0.0))

    # ── 三个时辰龛（机关清单坐标）──
    def niche_enum(kind):
        return getattr(unreal.DysisNiche, kind.upper(), None) or getattr(unreal.DysisNiche, kind, None)
    for label, az, r, y, kind in (
        ("Niche_Sun",     96.00, 14.90, 20.45, "Sun"),
        ("Niche_Rainbow", 161.07, 15.28, 18.70, "Rainbow"),
        ("Niche_Moon",     6.58, 15.75, 16.40, "Moon"),
    ):
        n = spawn("DysisNicheActor", label, az_ry(az, r, y))
        val = niche_enum(kind)
        if val is not None:
            setp(n, "niche", val)
        else:
            warn("DysisNiche 枚举值 %s 取不到" % kind)

    # ── 金苹果 + 接光（屋顶浑天仪 27.83°/13.2/39.05；苹果在架上）──
    apple = spawn("DysisGoldenApple", "DysisGoldenApple", unreal.Vector(1167, 616, 4010))
    catch = spawn("DysisCatchLight", "DysisCatchLight", unreal.Vector(1167, 616, 3905))
    setp(catch, "platform_cm", unreal.Vector(1167, 616, 3905))
    setp(catch, "apple", apple)

    # ── 通用机关驱动器（机关清单"怎么动"→ MechDriver 三类运动）──
    # 触发近似说明：TS 上门=天鹅解开（夜线 v1 挂 bTriggerOnNight）；TR 上门/月石=月光扫到
    # （v1 挂镜源任意格）；月桥/双子/半桥=夜线。都在编辑器里可改。
    def motion_enum(name):
        return getattr(unreal.DysisMechMotion, name)

    def driver(label, mesh_label, motion, params, trigger):
        d = spawn("DysisMechDriver", label, unreal.Vector(0, 0, 0))
        mesh = find_by_label(mesh_label)
        if mesh:
            make_movable(mesh)
            setp(d, "target_mesh", mesh)
            d.set_actor_location(mesh.get_actor_location(), False, False)
        else:
            warn("找不到 %s，%s 没有目标网格" % (mesh_label, label))
            return d
        setp(d, "motion", motion)
        for k, v in params.items():
            setp(d, k, v)
        for k, v in trigger.items():
            setp(d, k, v)
        return d

    SLIDE = motion_enum("SLIDE_WORLD")
    ROT = motion_enum("ROTATE_LOCAL")
    REV = motion_enum("REVEAL")
    NIGHT = {"b_trigger_on_night": True}
    MOON = {"trigger_mirror": mirror}          # 任意镜格（slot=-1 默认）
    REV_HIDE = {"b_reveal_to_visible": False, "b_reveal_collision": False}
    REV_SHOW_WALK = {"b_reveal_to_visible": True, "b_reveal_collision": True}

    # 月2 天鹅组：女神像一格、石门沉 2.6、浮雕沉 3.05、TS 下门石板沉 2.6
    driver("Mech_SwanGoddess", "SM_Mech_Swan_Goddess", ROT,
           {"rotate_axis": unreal.Vector(0, 0, 1), "rotate_from_deg": 0.0, "rotate_to_deg": -45.0, "speed": 30.0}, {})
    driver("Mech_SwanDoor", "SM_Mech_Swan_Door", SLIDE,
           {"slide_from_cm": unreal.Vector(-279, -1534, 2300), "slide_to_cm": unreal.Vector(-279, -1534, 2040), "speed": 30.0}, NIGHT)
    driver("Mech_SwanRelief", "SM_Mech_Swan_Relief", SLIDE,
           {"slide_from_cm": unreal.Vector(-275, -1519, 2450), "slide_to_cm": unreal.Vector(-275, -1519, 2145), "speed": 30.0}, NIGHT)
    driver("Mech_StairSeal_TS", "SM_Mech_StairSeal_TS", SLIDE,
           {"slide_from_cm": unreal.Vector(1223, -965, 1450), "slide_to_cm": unreal.Vector(1223, -965, 1190), "speed": 30.0}, NIGHT)

    # 月3 月亮浮雕隐去 + TR 下门石板沉 2.6（镜月光触发，v1=任意格）
    driver("Mech_MoonRelief", "SM_Mech_MoonRelief_Block", REV, dict(REV_HIDE), MOON)
    driver("Mech_StairSeal_TR", "SM_Mech_StairSeal_TR", SLIDE,
           {"slide_from_cm": unreal.Vector(-584, 1445, 600), "slide_to_cm": unreal.Vector(-584, 1445, 340), "speed": 30.0}, MOON)

    # 月4 双子：波吕丢刻斯推回卡斯托耳身边（250.6°/14.2）→ 铜链撤、月桥显形可踩
    driver("Mech_Pollux", "SM_Mech_Twins_Pollux", SLIDE,
           {"slide_from_cm": unreal.Vector(-1305, 864, 600), "slide_to_cm": az_ry(250.6, 14.2, 6.0), "speed": 100.0}, NIGHT)
    driver("Mech_TwinsChain", "SM_Mech_Twins_Chain", REV, dict(REV_HIDE), NIGHT)
    driver("Mech_TwinsWall", "SM_Mech_Twins_WallBlock", REV, dict(REV_HIDE), MOON)
    driver("Mech_MoonBridge", "SM_Mech_MoonBridge_Deck", REV, dict(REV_SHOW_WALK), NIGHT)

    # 月5 半桥：从池沿伸向水亭（v1 用整体平移近似"缩放伸出"）
    driver("Mech_HalfBridge", "SM_Mech_HalfBridge_Deck", SLIDE,
           {"slide_from_cm": az_ry(47.21, 10.55, 0.0), "slide_to_cm": az_ry(47.21, 6.20, 0.0), "speed": 50.0}, NIGHT)

    # 月之龛月石透开（三相像转对格；v1=任意格）
    driver("Mech_MoonShrine", "SM_Mech_MoonShrine_Block", REV, dict(REV_HIDE), MOON)

    # 日3 日之龛：盖子翻开 -1.9rad（镜槽位 [68.9,122.9,153.19,206.19,...]，206.19=下标 3）
    driver("Mech_SunNicheLid", "SM_Mech_SunNiche_Lid", ROT,
           {"rotate_axis": unreal.Vector(1, 0, 0), "rotate_from_deg": 0.0, "rotate_to_deg": -108.86, "speed": 60.0},
           {"trigger_mirror": mirror, "trigger_mirror_slot": 3})

    # 虹线：窗下石沿伸出来（缩进=沿半径往外退 0.9m；From=缩、To=现位=伸出）、棱镜柱升起 0.78m
    ledge_now = unreal.Vector(-1446, 360, 1750)
    out = az_dir(166.0)
    driver("Mech_SillLedge", "SM_Mech_Sill_Ledge", SLIDE,
           {"slide_from_cm": ledge_now + unreal.Vector(out.x * 90, out.y * 90, 0), "slide_to_cm": ledge_now, "speed": 20.0}, {})
    driver("Mech_PrismColumn", "SM_Mech_Prism_Column", SLIDE,
           {"slide_from_cm": unreal.Vector(-1466, 399, 1750 - 78), "slide_to_cm": unreal.Vector(-1466, 399, 1750), "speed": 20.0}, {})

    # 水闸 → 瀑布水帘泻下（NoCollision 网格，显形即可见）
    driver("Mech_Waterfall", "SM_Mech_Water_Waterfall", REV,
           dict(REV_SHOW_WALK), {"trigger_lever": lever_sluice})

    # 外窗石块（上门开后逐块消失）：TS 三块挂夜线、TR 两块挂镜线，v1 一起动
    for label, mesh_label, trig in (
        ("Mech_Win_TS1", "SM_Mech_StairWindows_TS_1", NIGHT),
        ("Mech_Win_TS2", "SM_Mech_StairWindows_TS_2", NIGHT),
        ("Mech_Win_TS3", "SM_Mech_StairWindows_TS_3", NIGHT),
        ("Mech_Win_TR1", "SM_Mech_StairWindows_TR_1", MOON),
        ("Mech_Win_TR2", "SM_Mech_StairWindows_TR_2", MOON),
    ):
        driver(label, mesh_label, REV, dict(REV_HIDE), trig)

    # ── 保存 ──
    saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("保存关卡：%s（%s）" % (MAP, "成功" if saved else "失败"))
    log("共摆放玩法 Actor %d 个；失效条目见上方警告" % len(spawned))
    return len(spawned)


run()
