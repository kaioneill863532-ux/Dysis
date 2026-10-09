# 主角模型和动作的测试（UE 编辑器 Python，开 PIE 跑）：用的是不是正式模型、站 / 走 / 跑 / 起跳 / 下落 / 落地 / 上台阶 / 互动伸手
# 各放的是不是对的那一段动作、第一人称时人藏起来；顺带拍几张看样子。结果写在 项目/Saved/pie_hero.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_hero.txt"
lines = []; bad = [0]
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def check(ok, what):
    if not ok: bad[0] += 1
    say(("✔ " if ok else "✘ ") + what)
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def shots(): return set(glob.glob(SAVED + "Screenshots/WindowsEditor/*.png"))
def wait(n):
    for _ in range(n): yield
def shoot(w, tag):
    before = shots()
    unreal.SystemLibrary.execute_console_command(w, "Shot")
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    pc.get_hud().start_game(True)
    yield from wait(120)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def clip(): return str(pawn.get_hero_clip_name())
    def speed():
        v = pawn.get_velocity(); return math.hypot(v.x, v.y)
    def go(p, dz=6.0): unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.2f %.2f %.2f" % (p[0], p[1], p[2] + dz))
    def view(tag, back, side, up, look_up=95.0, fov=45.0, frames=24):
        # 相对人的朝向摆一台镜头：back = 在人前面多远（正数 = 从正面看）、side = 往右多远、up = 多高
        f = foot(); yaw = math.radians(pawn.get_actor_rotation().yaw)
        fx, fy = math.cos(yaw), math.sin(yaw); rx, ry = -fy, fx
        eye = [f[0] + fx * back + rx * side, f[1] + fy * back + ry * side, f[2] + up]
        director.debug_camera(unreal.Vector(*eye), unreal.Vector(f[0], f[1], f[2] + look_up), fov)
        yield from wait(frames)
        yield from shoot(w, tag)
        director.debug_camera_off()

    # 1 模型
    sk = mesh.get_skeletal_mesh_asset() if mesh else None
    check(bool(pawn.has_hero_mesh()) and sk is not None and sk.get_name() == "SK_DysisHero", "用的是正式的主角模型：%s" % (sk.get_name() if sk else None))
    rl = mesh.get_editor_property("relative_location"); rr = mesh.get_editor_property("relative_rotation")
    check(abs(rl.z + hh) < 0.5 and abs(rr.yaw + 90.0) < 0.5, "模型的脚在碰撞体底下、脸朝前：相对位置 z = %.0f，转了 %.0f°" % (rl.z, rr.yaw))
    yield from wait(30)
    check(clip() == "A_Idle", "站着不动：放的是 %s（应为 A_Idle）" % clip())
    yield from view("主角_正面", 330.0, 0.0, 120.0)
    yield from view("主角_侧面", 60.0, 320.0, 110.0)
    yield from view("主角_背面（玩的时候的角度）", -380.0, 40.0, 190.0, fov=60.0)
    yield from view("主角_脸", 150.0, 30.0, 158.0, look_up=152.0, fov=30.0)

    # 2 走、跑（岛上横着走；正前方是开场那束光，先不往上撞）
    yaw = math.radians(pc.get_control_rotation().yaw); fwd = unreal.Vector(-math.sin(yaw), math.cos(yaw), 0)
    seen = []; sp = 0.0
    for i in range(46):
        pawn.add_movement_input(fwd, 1.0, False); yield
        if i > 12 and speed() > 300: seen.append(clip()); sp = max(sp, speed())
        if i == 28: director.debug_camera(unreal.Vector(foot()[0] + math.cos(yaw) * 380, foot()[1] + math.sin(yaw) * 380, foot()[2] + 110), unreal.Vector(foot()[0], foot()[1], foot()[2] + 95), 45.0)
        if i == 40: unreal.SystemLibrary.execute_console_command(w, "Shot")
    director.debug_camera_off()
    check(len(seen) > 8 and set(seen) == {"A_Run"} and abs(sp - 310) < 8, "走（%.0f cm/s）：放的是 %s（走路动作的自然速度只有 1.7 m/s，所以用慢一点的 A_Run）" % (sp, sorted(set(seen))))
    yield from wait(25)
    check(clip() == "A_Idle", "停下来回到 %s" % clip())
    # 慢慢走（手柄轻推）：应该是走路的动作
    seen = set(); sp = 0.0
    for i in range(60):
        pawn.add_movement_input(fwd, -0.45, False); yield
        if i > 25: seen.add(clip()); sp = max(sp, speed())
    check(seen == {"A_Walk"} and sp < 200, "慢慢走（%.0f cm/s）：放的是 %s" % (sp, sorted(seen)))
    yield from wait(25)

    # 3 跳：起跳 → 下落 → 落地 → 站着
    order = []
    pawn.jump()
    for i in range(140):
        yield
        c = clip()
        if not order or order[-1] != c: order.append(c)
        if i == 14: yield from view("主角_跳起来", 60.0, 380.0, 150.0, look_up=130.0, frames=1)
    check(order[:3] == ["A_JumpStart", "A_FallLoop", "A_Land"] and order[-1] == "A_Idle", "跳：动作的先后 %s" % " → ".join(order))

    # 4 上台阶：水庭去水闸的那几级石阶（方位 100°–112°），从下往上走
    go(polar(96.0, 1330.0, 0.0), 8.0)
    yield from wait(50)
    seen = set(); z0 = foot()[2]
    for i in range(160):
        f = foot(); az = math.atan2(f[1], f[0]) + math.radians(1.0)
        tgt = polar(math.degrees(az) + 6.0, 1330.0, 0.0)
        d = unreal.Vector(tgt[0] - f[0], tgt[1] - f[1], 0.0)
        pawn.add_movement_input(d, 1.0, False); yield
        seen.add(clip())
        if i == 50: yield from view("主角_上台阶", 30.0, -380.0, 150.0, frames=1)
    say("   上台阶这一段出现过的动作：%s，升高了 %.0f cm" % (sorted(seen), foot()[2] - z0))
    check("A_RunUpstairs" in seen and foot()[2] - z0 > 60, "踩着台阶往上走时放 A_RunUpstairs")
    yield from wait(30)

    # 5 互动：站到水闸旁边按 E（和玩的时候一样走人身上的那个按键处理）→ 水闸开了，人伸一下手
    go(polar(121.0, 1395.0, 150.0), 8.0)
    yield from wait(50)
    prompt = str(director.debug_prompt_at(unreal.Vector(*foot())))
    if hasattr(pawn, "interact_pressed"):
        pawn.interact_pressed()
        yield from wait(12)
        check(prompt != "" and clip() == "A_TouchForward", "在“%s”旁边按 E：人伸手（%s）" % (prompt, clip()))
        yield from view("主角_互动时伸手", 40.0, 330.0, 140.0, frames=40)
        # 一走动就停
        for i in range(12): pawn.add_movement_input(unreal.Vector(-foot()[0], -foot()[1], 0.0), 1.0, False); yield
        check(clip() != "A_TouchForward", "伸手的时候一走动就停（%s）" % clip())
    else:
        say("   （这一版还没有 interact_pressed，互动伸手没测；脚边能互动的是“%s”）" % prompt)
    yield from wait(20)

    # 6 第一人称：人藏起来
    pawn.set_view_mode(unreal.DysisViewMode.FIRST_PERSON)
    yield from wait(10)
    hidden_fp = mesh.get_editor_property("hidden_in_game")
    pawn.set_view_mode(unreal.DysisViewMode.THIRD_PERSON)
    yield from wait(20)
    check(hidden_fp and not mesh.get_editor_property("hidden_in_game"), "第一人称时人藏起来，切回第三人称又出来")

    # 7 在殿里的样子（镜头的位置写死，不跟人的朝向）
    go(polar(285.0, 1250.0, 0.0), 8.0)      # 两根柱子之间（方位 300° 那里正好是柱子）
    yield from wait(60)
    f = foot()
    director.debug_camera(unreal.Vector(*polar(287.0, 900.0, 150.0)), unreal.Vector(f[0], f[1], f[2] + 95.0), 50.0)
    yield from wait(30)
    yield from shoot(w, "主角_在水庭")
    director.debug_camera_off()
    say("全部通过" if bad[0] == 0 else "有 %d 条没过" % bad[0])

class R:
    def __init__(s): s.g = main(); s.h = None
    def tick(s, dt):
        try: next(s.g)
        except StopIteration: s.fin()
        except Exception: say("出错: " + traceback.format_exc()); s.fin()
    def fin(s):
        if s.h is not None: unreal.unregister_slate_post_tick_callback(s.h); s.h = None
        try: unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
        except Exception: pass
        for f in glob.glob(SAVED + "SaveGames/Dysis*.sav") + glob.glob(SAVED + "SaveGames/Dysis*.bak"):
            try: os.remove(f)
            except Exception: pass
        say("DONE"); s.g = None; import gc; gc.collect()
if os.path.exists(OUT): os.remove(OUT)
_hero = R(); _hero.h = unreal.register_slate_post_tick_callback(_hero.tick)
