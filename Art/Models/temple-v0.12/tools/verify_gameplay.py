# 验证 Dysis_Temple 里玩法 Actor 的摆放与接线（只读，不保存）
# 无头：UnrealEditor-Cmd Dysis.uproject -run=pythonscript -script=本文件
import unreal

MAP = "/Game/Dysis/Maps/Dysis_Temple"
TAG = "DysisGameplay"

def say(msg):
    unreal.log_warning("[Verify] " + str(msg))   # Warning 级才能到 stdout

def getp(obj, prop, default=None):
    try:
        return obj.get_editor_property(prop)
    except Exception:
        # 布尔属性剥 b 前缀的规则
        if prop.startswith("b_"):
            try:
                return obj.get_editor_property(prop[2:])
            except Exception:
                return default
        return default

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.EditorLevelLibrary.get_editor_world()
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()

# 1) WorldSettings GameMode
ws = world.get_world_settings()
gm = getp(ws, "game_mode_override", getp(ws, "default_game_mode"))
say("GameModeOverride = %s" % (gm.get_name() if gm else "None"))

# 2) 每类玩法 Actor 计数
from collections import Counter
count = Counter()
mine = []
for a in actors:
    try:
        cn = a.get_class().get_name()
    except Exception:
        continue
    if cn.startswith("Dysis"):
        count[cn] += 1
    if TAG in list(a.tags):
        mine.append(a)
say("Dysis 类 Actor 计数: %s" % dict(sorted(count.items())))
say("带 %s 标签的 Actor: %d 个" % (TAG, len(mine)))

def by_label(label):
    for a in actors:
        try:
            if a.get_actor_label() == label:
                return a
        except Exception:
            pass
    return None

# 3) 光柱接线抽查
for lbl in ("Beam_b1", "Beam_Mirror", "Beam_Oculus", "Beam_Prologue"):
    b = by_label(lbl)
    if not b:
        say("%s 缺失!" % lbl)
        continue
    zone = getp(b, "beam_zone")
    w = getp(b, "window_cm")
    say("%s zone=%s window=(%.0f,%.0f,%.0f) from_mirror=%s mirror_beam=%s oculus=%s prologue=%s" % (
        lbl, zone, w.x, w.y, w.z,
        getp(b, "b_from_mirror"), getp(b, "b_mirror_beam"),
        getp(b, "b_oculus_beam"), getp(b, "b_prologue_beam")))
msrc = by_label("DysisMirrorSource")
bm = by_label("Beam_Mirror")
say("Beam_Mirror.mirror_source = %s (镜源=%s)" % (
    getp(bm, "mirror_source").get_actor_label() if bm and getp(bm, "mirror_source") else "None",
    msrc.get_actor_label() if msrc else "缺"))

# 4) 拉杆臂挂接
la = by_label("DysisLeverA")
arm = by_label("SM_Mech_LeverA_Arm")
if la and arm:
    p = arm.get_attach_parent_actor()
    say("LeverA 臂挂接 → %s" % (p.get_actor_label() if p else "无(没挂上)"))
say("LeverSluice 水闸: sluice=%s water=%s" % (
    getp(by_label("DysisLeverSluice"), "sluice_name"),
    getp(by_label("DysisLeverSluice"), "water_state_on_change")))

# 5) 石板/驱动抽查
s = by_label("DysisSlider_b2")
say("Slider_b2: az %.2f→%.2f r=%.2f y=%.2f go_end_when_pulled=%s target=%s lever=%s" % (
    getp(s, "az_start"), getp(s, "az_end"), getp(s, "radius_m"), getp(s, "height_m"),
    getp(s, "b_go_to_end_when_pulled"),
    getp(s, "target_mesh").get_actor_label() if getp(s, "target_mesh") else "None",
    getp(s, "lever").get_actor_label() if getp(s, "lever") else "None"))
d = by_label("Mech_MoonBridge")
say("Mech_MoonBridge: motion=%s reveal_to_visible=%s reveal_collision=%s trigger_on_night=%s target=%s" % (
    getp(d, "motion"), getp(d, "b_reveal_to_visible"), getp(d, "b_reveal_collision"),
    getp(d, "b_trigger_on_night"),
    getp(d, "target_mesh").get_actor_label() if getp(d, "target_mesh") else "None"))
d2 = by_label("Mech_StairSeal_TR")
say("Mech_StairSeal_TR: trigger_mirror=%s target=%s from=(%.0f,%.0f,%.0f)" % (
    getp(d2, "trigger_mirror").get_actor_label() if getp(d2, "trigger_mirror") else "None",
    getp(d2, "target_mesh").get_actor_label() if getp(d2, "target_mesh") else "None",
    getp(d2, "slide_from_cm").x, getp(d2, "slide_from_cm").y, getp(d2, "slide_from_cm").z))

# 6) 水面/月路/接光/苹果
w = by_label("DysisWater")
mp = by_label("DysisMoonPath")
say("MoonPath: water=%s z=%.0f zone=%s" % (
    getp(mp, "target_water").get_actor_label() if getp(mp, "target_water") else "None",
    getp(mp, "surface_z_cm"), getp(mp, "zone_name")))
c = by_label("DysisCatchLight")
say("CatchLight: platform=(%.0f,%.0f,%.0f) apple=%s door=%s" % (
    getp(c, "platform_cm").x, getp(c, "platform_cm").y, getp(c, "platform_cm").z,
    getp(c, "apple").get_actor_label() if getp(c, "apple") else "None",
    getp(c, "bridge_door_name")))

# 7) BGM 资产路径排查
for name in ("M_Day_Sicilienne_Long", "M_Night_ClairDeLune_Full"):
    path = "/Game/Dysis/Audio/" + name
    a1 = unreal.load_asset(path)
    a2 = None
    try:
        a2 = unreal.EditorAssetLibrary.load_asset(path)
    except Exception:
        pass
    exists = unreal.EditorAssetLibrary.does_asset_exist(path)
    say("BGM %s: load_asset=%s EditorAssetLibrary=%s exists=%s" % (name, a1, a2, exists))
mus = by_label("DysisMusic")
say("Music: day=%s night=%s" % (getp(mus, "day_music"), getp(mus, "night_music")))

# 8) 拉杆 A/B 位置（对照机关清单）
for lbl in ("DysisLeverA", "DysisLeverB"):
    a = by_label(lbl)
    loc = a.get_actor_location()
    say("%s 位置 (%.0f, %.0f, %.0f)" % (lbl, loc.x, loc.y, loc.z))

say("验证完毕")
