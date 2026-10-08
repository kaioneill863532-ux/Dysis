# 玩家这一步的自动测试（UE 编辑器 Python，开 PIE 跑）：出生点、镜头、走 / 跳的数值、回落脚点的几条规则。
# 标准答案是灰盒 v0.12 的数（见 Source/Dysis/DysisGreybox.h 和 Tools/greybox/golden/greybox_player.json）。
# 结果写在 项目/Saved/pie_player.txt，最后一行 DONE；每条前面 ✔ / ✘。
import unreal, os, glob, math, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_player.txt"
SPAWN = (4978.79, 2189.09, -1200.0)
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
    unreal.SystemLibrary.execute_console_command(w, "Shot showui")
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    hud = pc.get_hud()
    hud.start_game(True)
    yield from wait(120)
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def go(x, y, z):
        unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f" % (x, y, z))
    cam = pc.player_camera_manager

    # 1 出生点和朝向
    f = foot(); rot = pc.get_control_rotation()
    check(math.dist(f, SPAWN) < 6, "出生在岛的外沿：脚 (%.0f, %.0f, %.0f)，应为 (%.0f, %.0f, %.0f)" % (f + SPAWN))
    check(abs(((rot.yaw - 203.7343) + 540) % 360 - 180) < 0.5 and abs(rot.pitch - (-12.6)) < 0.5, "面朝神殿、微微俯视：yaw %.1f pitch %.1f" % (rot.yaw % 360, rot.pitch))
    t = pawn.get_component_by_class(unreal.DysisTimeComponent)
    say("   时间组件：" + str(t.describe_state()) if hasattr(t, "describe_state") else "   （时间组件没有 describe_state）")

    # 2 第三人称镜头
    c = cam.get_camera_location(); yaw = math.radians(rot.yaw)
    tgt = (f[0] - math.sin(yaw) * 55, f[1] + math.cos(yaw) * 55, f[2] + 150)
    d = math.dist((c.x, c.y, c.z), tgt)
    check(abs(d - 460) < 8, "第三人称：镜头离瞄点 %.0f cm（应 460）" % d)
    check(pawn.get_editor_property("walk_speed") == 310 and pawn.get_editor_property("run_speed") == 520, "走 310 / 跑 520 cm/s")
    yield from shoot(w, "第三人称_出生点")

    # 3 走：在岛上横着走 1 秒，量速度（正前方是开场那束光，先不往上撞）
    fwd = unreal.Vector(-math.sin(yaw), math.cos(yaw), 0)
    sp = 0
    for i in range(50):
        pawn.add_movement_input(fwd, 1.0, False); yield
        if i > 20: sp = max(sp, pawn.get_velocity().length_2d() if hasattr(pawn.get_velocity(), "length_2d") else math.hypot(pawn.get_velocity().x, pawn.get_velocity().y))
    check(abs(sp - 310) < 6, "走的速度 %.0f cm/s（应 310）" % sp)
    yield from wait(20)

    # 4 跳：最高点应为 v²/2g = 760²/(2×2200) = 131 cm；在岛上跳不应该被送回落脚点
    z0 = foot()[2]; n0 = mv.get_editor_property("respawn_count"); top = z0
    pawn.jump()
    for _ in range(120):
        yield; top = max(top, foot()[2])
    check(abs((top - z0) - 131.3) < 5, "跳的高度 %.0f cm（应 131）" % (top - z0))
    check(abs(foot()[2] - z0) < 3 and mv.get_editor_property("respawn_count") == n0, "岛上起跳后落回原地、没有被送回落脚点")

    # 5 第一人称
    pawn.set_view_mode(unreal.DysisViewMode.FIRST_PERSON)
    yield from wait(8)
    c = cam.get_camera_location(); f = foot()
    check(abs(c.z - (f[2] + 162)) < 3 and math.hypot(c.x - f[0], c.y - f[1]) < 3, "第一人称：眼高 %.0f cm（应 162）" % (c.z - f[2]))
    yield from shoot(w, "第一人称")
    pawn.set_view_mode(unreal.DysisViewMode.THIRD_PERSON)
    yield from wait(30)

    # 6 回落脚点：掉进海里
    safe = foot(); n0 = mv.get_editor_property("respawn_count")
    go(5913, 2599, -1150)
    for _ in range(600):
        yield
        if mv.get_editor_property("respawn_count") > n0: break
    yield from wait(20)
    f = foot()
    check(mv.get_editor_property("respawn_count") == n0 + 1 and math.dist(f, safe) < 80, "掉进海里 → 回到落脚点（离原处 %.0f cm）" % math.dist(f, safe))

    # 7 掉 8 m（> 6.5 m）落地 → 回落脚点；掉 5 m 不回
    yield from wait(40); n0 = mv.get_editor_property("respawn_count")
    go(safe[0], safe[1], safe[2] + 800)
    for _ in range(400):
        yield
        if mv.get_editor_property("respawn_count") > n0: break
    check(mv.get_editor_property("respawn_count") == n0 + 1, "从 8 m 高处落地 → 回到落脚点")
    yield from wait(60); n0 = mv.get_editor_property("respawn_count")
    go(safe[0], safe[1], safe[2] + 500)
    yield from wait(200)
    check(mv.get_editor_property("respawn_count") == n0 and abs(foot()[2] - safe[2]) < 3, "从 5 m 高处落地 → 不回落脚点")

    # 8 R 键
    n0 = mv.get_editor_property("respawn_count")
    mv.respawn_now()
    yield from wait(60)
    check(mv.get_editor_property("respawn_count") == n0 + 1 and not mv.is_respawning(), "R 键回落脚点")

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
        say("DONE"); s.g = None; import gc; gc.collect()   # 放掉对 PIE 里那些对象的引用
if os.path.exists(OUT): os.remove(OUT)
_pt = R(); _pt.h = unreal.register_slate_post_tick_callback(_pt.tick)
