# 2026-10-08 第二轮反馈的测试和截图（UE 编辑器 Python，开 PIE 跑）：
#   · 主界面：屋顶的踏步是升起来的样子，点了开始游戏就落平；
#   · 关卡名是罗马数字；
#   · 过剧情时人定在原地，“单击鼠标”翻页；入夜以后狄西斯那一句用剧情对话框；
#   · “查看××”：女神像、托镜天使像、卡斯托耳、藏着波吕丢刻斯的墙、月门、两处浑天仪、开过的水闸；
#   · 黄昏：走到殿顶那一段的画面（截图）。
# 结果写在 项目/Saved/pie_round2.txt，最后一行 DONE。想只跑其中几段：把段名写在 Saved/round2_only.txt（逗号隔开：menu,dusk,views,talk）。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_round2.txt"
ONLY = [x.strip() for x in open(SAVED + "round2_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "round2_only.txt") else []
TUNE = json.load(open(SAVED + "dusk_tune.json", encoding="utf-8")) if os.path.exists(SAVED + "dusk_tune.json") else {}
def on(name): return not ONLY or name in ONLY
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
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisSkyActor)[0]
    for k, v in TUNE.items(): sky.set_editor_property(k, unreal.LinearColor(*v) if isinstance(v, list) else v)
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    night = [False]
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def look_at(p):
        f = foot(); d = (p[0] - f[0], p[1] - f[1], p[2] - (f[2] + 150.0))
        look(math.degrees(math.atan2(d[1], d[0])), math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))))
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

    # ── 主界面 ──
    if on("menu"):
        yield from seconds(2.5)
        r = roof()
        yield from shoot(w, "主界面_台阶升起来")
        check(abs(r["up"] - 1.0) < 1e-3, "主界面开着：屋顶的踏步升到头（up = %.2f）" % r["up"])
    pc.get_hud().start_game(True)
    yield from wait(90)
    if on("menu"):
        r = roof()
        check(abs(r["up"]) < 1e-3, "开始游戏以后：踏步落平（up = %.2f）" % r["up"])
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")

    # ── 黄昏：殿顶一路的画面 ──
    if on("dusk"):
        for H in (45.0, 58.0, 66.0, 72.0, 75.5):
            go(polar(20.0, 1420.0, 3035.0)); tc.set_forced_time(H); look(250.0, 4.0)
            yield from wait(110)
            yield from shoot(w, "黄昏_殿顶朝落日_H%.0f（黄昏的浓度 %.2f）" % (H, sky.get_dusk_look()))
        go(polar(20.0, 1420.0, 3035.0)); tc.set_forced_time(72.0); look(70.0, 2.0)
        yield from wait(110)
        yield from shoot(w, "黄昏_殿顶背着落日_H72")
        go(polar(250.0, 1400.0, 2300.0)); tc.set_forced_time(70.0); look(250.0 + 180.0 + 20.0, 4.0)
        yield from wait(110)
        yield from shoot(w, "黄昏_四层殿内_H70")
        go(polar(250.0, 1400.0, 600.0)); tc.set_forced_time(30.0); look(250.0 + 180.0 - 25.0, 8.0)
        yield from wait(110)
        check(sky.get_dusk_look() < 0.01, "下午（H = 30）没有黄昏的调子（%.2f）" % sky.get_dusk_look())
        yield from shoot(w, "下午_二层殿内_H30（对照）")
        tc.clear_forced_time()

    # ── “查看××”和关卡名 ──
    if on("views"):
        go(polar(147.0, 1400.0, 0.0), 10.0); look_at([-1016.0, 992.0, 200.0])
        yield from wait(45)
        yield from shoot(w, "查看_女神像（屏幕中间是罗马数字的关卡名 I）")
        day = {i: director.debug_interact(i) for i in ("viewGoddess", "viewTriple", "viewCastor", "viewPolluxWall", "viewMoonGate", "viewArmillaryPav", "viewSluice", "viewArmillaryTop")}
        check(all(day[i] for i in ("viewGoddess", "viewTriple", "viewCastor", "viewPolluxWall", "viewMoonGate", "viewArmillaryPav", "viewSluice")) and not day["viewArmillaryTop"],
              "白天：女神像、天使像、卡斯托耳、两面石墙、水亭浑天仪、开过的水闸都能查看；殿顶浑天仪这时是“取下金苹果” %s" % {k: v for k, v in day.items() if not v})
        go([-371.4 + 110.0, -1386.1 + 60.0, 600.0]); look_at([-371.4, -1386.1, 700.0])
        yield from wait(90)
        yield from shoot(w, "查看_卡斯托耳")
        go(polar(146.5, 1480.0, 600.0)); look(146.5, 0.0)
        yield from wait(90)
        yield from shoot(w, "查看_藏着波吕丢刻斯的墙")

    # ── 入夜：狄西斯那一句用剧情对话框；过剧情时人定住 ──
    if on("talk"):
        go(polar(250.0, 1400.0, 2300.0)); look(250.0 + 180.0 + 20.0, 4.0)
        yield from wait(40)
        free = yield from walk_try()
        cmd(w, "Dysis.WorldRelease"); cmd(w, "Dysis.Sluice 1")
        director.debug_set_night(True); night[0] = True
        for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
        t0 = now(); talking = False
        while now() - t0 < 9.0 and not talking:
            yield
            talking = pawn.in_dialogue()
        took = now() - t0
        yield from wait(30)
        yield from shoot(w, "入夜_狄西斯那一句（剧情对话框）")
        locked = yield from walk_try()
        check(talking and 4.5 < took < 6.5 and free > 100.0 and locked < 2.0, "入夜 %.1f 秒后狄西斯那一句用剧情对话框放；平时这样能走 %.0f cm，过剧情时走了 %.0f cm（定住了）" % (took, free, locked))
        pawn.advance_pressed()
        yield from wait(10)
        after = pawn.in_dialogue()
        moved = yield from walk_try()
        check(not after and moved > 100.0, "单击鼠标：这一句翻过去，对话结束，人又能走了（走了 %.0f cm）" % moved)
        yield from seconds(3.0)
        go(polar(250.0, 1400.0, 600.0)); look(250.0 + 180.0 - 25.0, 8.0)
        yield from wait(45)
        ok = director.debug_interact("viewArmillaryTop")
        check(ok, "入夜以后殿顶的浑天仪可以查看（金苹果已经取下来了）")
        yield from shoot(w, "夜_二层（关卡名应是 II）")
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
_r2 = R(); _r2.h = unreal.register_slate_post_tick_callback(_r2.tick)
