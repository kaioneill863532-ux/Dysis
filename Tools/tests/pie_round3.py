# 2026-10-08 第三轮小改的测试和截图（UE 编辑器 Python，开 PIE 跑）：
#   · 主界面：踏步升着、叶片合着（跟着踏步一级级抬高），和编辑器里不试玩时看到的一样；开始游戏后落平；
#   · 过剧情时人不能走、镜头不能转；
#   · 结局：塞勒涅开口以后屋顶的细桥退走，圆眼里是一整个圆；结局的剧情里什么都不能互动；
#   · 卡斯托耳的描述（截图）；关卡名是罗马数字的图（截图）。
# 结果写在 项目/Saved/pie_round3.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_round3.txt"
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
def cmd(w, s): unreal.SystemLibrary.execute_console_command(w, s)
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
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    night = [False]
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f%s" % (p[0], p[1], p[2] + dz, " night" if night[0] else ""))
    def seconds(t):
        t0 = now()
        while now() - t0 < t: yield
    def roof(): return json.loads(director.describe_roof())
    def walk_try(n=40):
        f0 = foot()
        for _ in range(n):
            pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.0, False); yield
        f1 = foot(); return math.dist(f0[:2], f1[:2])
    def turn_try(n=20):
        y0 = pc.get_control_rotation().yaw
        for _ in range(n):
            pawn.add_controller_yaw_input(2.0); yield
        d = (pc.get_control_rotation().yaw - y0 + 540.0) % 360.0 - 180.0
        return abs(d)
    def piece(tag):
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
            if tag in [str(t) for t in a.tags]: return a
        return None

    # ── 主界面 ──
    yield from seconds(2.5)
    r = roof()
    yield from shoot(w, "主界面_台阶升着、叶片合着")
    check(abs(r["up"] - 1.0) < 1e-3 and abs(r["iris"] - 40.0) < 0.5, "主界面开着：踏步升到头（up = %.2f）、叶片合着（光圈 %.0f cm）" % (r["up"], r["iris"]))
    pc.get_hud().start_game(True)
    yield from wait(90)
    r = roof()
    check(abs(r["up"]) < 1e-3 and abs(r["iris"] - 40.0) < 0.5, "开始游戏以后：踏步落平（up = %.2f），叶片还是合着的（%.0f cm）" % (r["up"], r["iris"]))
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")

    # ── 关卡名：罗马数字的图；岛上（序）不出 ──
    yield from wait(40)
    yield from shoot(w, "关卡名_岛上什么也不出")
    go(polar(60.0, 1250.0, 0.0), 8.0); look(240.0, 2.0)
    yield from wait(45)
    yield from shoot(w, "关卡名_一层 I")
    go(polar(250.0, 1400.0, 1450.0)); look(250.0 + 180.0, 4.0)
    yield from wait(45)
    yield from shoot(w, "关卡名_三层 III")

    # ── 卡斯托耳 ──
    go([-371.4 + 150.0, -1386.1 + 110.0, 600.0]); look(math.degrees(math.atan2(-110.0, -150.0)), -4.0)
    yield from wait(60)
    check(director.debug_interact("viewCastor"), "查看卡斯托耳")
    yield from wait(30)
    yield from shoot(w, "查看_卡斯托耳_带名字")

    # ── 过剧情：人和镜头都定住 ──
    go(polar(250.0, 1400.0, 2300.0)); look(250.0 + 180.0 + 20.0, 4.0)
    yield from wait(40)
    free_walk = yield from walk_try(); free_turn = yield from turn_try()
    cmd(w, "Dysis.WorldRelease"); cmd(w, "Dysis.Sluice 1")
    director.debug_set_night(True); night[0] = True
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    t0 = now()
    while now() - t0 < 9.0 and not pawn.in_dialogue(): yield
    yield from wait(20)
    lock_walk = yield from walk_try(); lock_turn = yield from turn_try()
    yield from shoot(w, "过剧情_对话框（右下角没有“继续”的提示）")
    check(pawn.in_dialogue() and free_walk > 100.0 and free_turn > 10.0 and lock_walk < 2.0 and lock_turn < 0.5,
          "过剧情时：走了 %.0f cm（平时 %.0f）、镜头转了 %.1f°（平时 %.0f°）——都定住了" % (lock_walk, free_walk, lock_turn, free_turn))
    pawn.advance_pressed()
    yield from wait(10)
    after_turn = yield from turn_try()
    check(not pawn.in_dialogue() and after_turn > 10.0, "翻过这一句、对话结束：镜头又能转了（%.0f°）" % after_turn)

    # ── 结局：细桥退走 ──
    cmd(w, "Dysis.Iris 1100")
    go([90.0, 60.0, 20.0], 8.0); look(200.0, 0.0)
    yield from wait(120)
    bridge = piece("SM_RoofBridge"); door = piece("SM_Mech_RoofBridgeDoor_Leaf")
    b0 = bridge.get_actor_location(); d0 = door.get_actor_location()
    here = unreal.Vector(90.0, 60.0, 20.0)
    can_before = str(director.debug_prompt_at(here))
    director.debug_play_ending(True)
    t0 = now(); shot = [(1.2, "细桥开始退"), (3.0, "退到一半"), (6.2, "圆眼里是整圆"), (11.5, "星座")]
    talk_t = None; moved_mid = 0.0
    while shot and now() - t0 < 60.0:
        yield
        f = json.loads(director.describe_finale())
        if f["talking"] and talk_t is None: talk_t = now()
        if talk_t is not None and now() - talk_t >= shot[0][0]:
            if shot[0][0] == 3.0: moved_mid = math.dist((b0.x, b0.y), (bridge.get_actor_location().x, bridge.get_actor_location().y))
            yield from shoot(w, "结局_%s" % shot[0][1]); shot.pop(0)
    b1 = bridge.get_actor_location(); d1 = door.get_actor_location()
    moved = math.dist((b0.x, b0.y), (b1.x, b1.y)); r0 = math.hypot(b0.x, b0.y); r1 = math.hypot(b1.x, b1.y)
    check(100.0 < moved_mid < 700.0 and abs(moved - 780.0) < 2.0 and r1 > r0 + 700.0 and bridge.get_editor_property("hidden") and door.get_editor_property("hidden") and abs(b1.z - b0.z) < 0.1,
          "塞勒涅开口以后细桥顺着自己的方向退走：3 秒时退了 %.0f cm，最后退了 %.0f cm（离殿心从 %.0f 到 %.0f cm），收起来了；桥门也一起" % (moved_mid, moved, r0, r1))
    can_after = str(director.debug_prompt_at(here))
    check(can_before == "placeApple" and can_after == "", "站在水亭的浑天仪旁边：结局以前旁边是“%s”，结局的剧情开始以后什么互动都没有了（“%s”）" % (can_before, can_after))
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
_r3 = R(); _r3.h = unreal.register_slate_post_tick_callback(_r3.tick)
