# 三相像和日之龛的测试（UE 编辑器 Python，开 PIE 跑）：
#   · 人在三层回廊不同地方（时刻不同）时，三相像是暗相还是日相、镜光有没有、能不能踩——和灰盒比；
#   · 镜子停在日相时，只变太阳的时刻，镜光和灰盒比；白天把六格转一遍；
#   · 夜里：月相、反射的月光，和灰盒比；
#   · 沿镜光越过中庭走到东边的石台，打开日之龛，得到太阳碎片。
# 标准答案：Tools/greybox/golden/greybox_mirror.json。结果写在 项目/Saved/pie_mirror.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_mirror.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_mirror.json", encoding="utf-8"))
FORM = {"dark": 0, "sun": 1, "moon": 2}
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
def beam_same(exp, got, walk=True):
    if exp["valid"] != got["valid"]: return "有没有光 灰盒 %s UE %s" % (exp["valid"], got["valid"])
    if abs(exp["lit"] - got["lit"]) > 0.043: return "照亮比例 灰盒 %.3f UE %.3f" % (exp["lit"], got["lit"])
    if not exp["valid"]: return ""
    if exp["clean"] != got["clean"] or (walk and exp["walkable"] != got["walkable"]): return "够不够长/能不能踩 灰盒 %s/%s UE %s/%s" % (exp["clean"], exp["walkable"], got["clean"], got["walkable"])
    if abs(exp["lit"] - got["lit"]) < 1e-6:
        if max(abs(a - b) for a, b in zip(exp["O"], got["O"])) > 2.0: return "起点差 %.1f cm" % max(abs(a - b) for a, b in zip(exp["O"], got["O"]))
        if max(abs(a - b) for a, b in zip(exp["L"], got["L"])) > 3e-4: return "方向差"
        if max(abs(a - b) for a, b in zip(exp["c"], got["c"])) > 15: return "照射距离差 %.0f cm（灰盒 %s UE %s）" % (max(abs(a - b) for a, b in zip(exp["c"], got["c"])), [round(x) for x in exp["c"]], [round(x) for x in got["c"]])
    return ""
def mirror_same(exp, got):
    if FORM[exp["form"]] != got["form"]: return "哪一相 灰盒 %s UE %d" % (exp["form"], got["form"])
    if abs(((exp["yaw"] - got["yaw"]) + 540) % 360 - 180) > 0.05 or abs(exp["tilt"] - got["tilt"]) > 0.05: return "朝向/仰角 灰盒 %.2f/%.2f UE %.2f/%.2f" % (exp["yaw"], exp["tilt"], got["yaw"], got["tilt"])
    if max(abs(a - b) for a, b in zip(exp["center"], got["center"])) > 1.0: return "镜心差 %.1f cm" % max(abs(a - b) for a, b in zip(exp["center"], got["center"]))
    if max(abs(a - b) for a, b in zip(exp["n"], got["n"])) > 2e-3: return "法线差"
    return ""

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
    yield from wait(90)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    beams = {str(b.get_editor_property("greybox_id")): b for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor)}
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def state(): return str(tc.describe_state())
    def mirror(): return json.loads(director.describe_mirror())
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    check("mirror" in beams and "mmoon" in beams, "镜光和月光反射两束都在（%s）" % sorted(beams))

    # ── A 白天：人站在不同地方（时刻不同）──
    n = ok = 0; notes = []
    for r in G["day"]:
        tc.set_forced_time(r["H"])
        yield from wait(150)
        e1 = mirror_same(r["mirror"], mirror()); e2 = beam_same(r["beam"], json.loads(beams["mirror"].debug_solve(r["H"])))
        n += 1; ok += 1 if not (e1 or e2) else 0
        if e1 or e2: notes.append("时刻 %.2f：%s %s" % (r["H"], e1, e2))
    check(ok == n, "白天 %d/%d 个时刻：三相像是哪一相、镜光的样子和灰盒一致（刚路过时是暗的，时刻过了 31 以后亮起来）" % (ok, n))
    for x in notes: say("     " + x)

    # ── B 镜子在日相：只变太阳的时刻 ──
    tc.set_forced_time(32.98)
    yield from wait(150)
    n = ok = 0; notes = []
    for r in G["sweep"]:
        e = beam_same(r["beam"], json.loads(beams["mirror"].debug_solve(r["H"])))
        n += 1; ok += 1 if not e else 0
        if e: notes.append("时刻 %.2f：%s" % (r["H"], e))
        yield
    check(ok == n, "镜子在日相、太阳在 %d 个不同时刻：镜光 %d 个和灰盒一致" % (n, ok))
    for x in notes[:6]: say("     " + x)

    # ── C 白天把六格转一遍 ──
    n = ok = 0; notes = []
    for r in G["daySlots"]:
        director.debug_set_mirror_slot(r["slot"])
        yield from wait(260)
        e1 = mirror_same(r["mirror"], mirror()); e2 = beam_same(r["beam"], json.loads(beams["mirror"].debug_solve(32.98)))
        n += 1; ok += 1 if not (e1 or e2) else 0
        if e1 or e2: notes.append("第 %d 格：%s %s" % (r["slot"], e1, e2))
    check(ok == n, "白天六格转一遍：%d/%d 格和灰盒一致" % (ok, n))
    for x in notes: say("     " + x)
    # 站到三层西南边的回廊上，看镜光从三相像那里越过中庭照到东边的石台
    cmd(w, "Dysis.Go -836 -996 1480")
    yield from wait(30)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=6.0, yaw=51.5))
    tc.set_forced_time(32.98)
    yield from wait(60)
    yield from shoot(w, "白天_镜光越过中庭")

    tc.set_forced_time(33.5)
    yield from wait(300)

    # ── E 沿镜光走到日之龛（在入夜之前）──
    info = json.loads(beams["mirror"].debug_solve(33.5))
    check(info["valid"] and info["walkable"], "时刻 33.5（镜光上的时刻）：镜光能踩")
    mid = [(info["b0"][i] + info["b1"][i]) / 2 for i in range(3)]; L = info["L"]
    cmd(w, "Dysis.Go %.1f %.1f %.1f" % (mid[0] + L[0] * 400, mid[1] + L[1] * 400, mid[2] + L[2] * 400 + 20))
    yield from wait(40)
    tc.clear_forced_time()
    yield from wait(40)
    check("beam:mirror" in state(), "站在镜光上：" + state()[:60])
    yield from shoot(w, "站在镜光上")
    # 中途往边上走 1 秒：两边拦着，掉不下去
    for _ in range(600):
        f = foot()
        if (f[0] - mid[0]) * L[0] + (f[1] - mid[1]) * L[1] > 1200: break
        pawn.add_movement_input(unreal.Vector(L[0], L[1], 0), 1.0, False); yield
    z0 = foot()[2]
    for _ in range(70):
        pawn.add_movement_input(unreal.Vector(-L[1], L[0], 0), 1.0, False); yield
    check("beam:mirror" in state() and abs(foot()[2] - z0) < 40, "走到中段往边上走 1 秒：还在光上（两边拦着）")
    # 贴着边一直往前走：能踩的面铺到石台上方为止，走到头自然落在石台上（落差几十厘米）。
    # 最后几米光贴着四层楼板的底面过去，人在镜光上时不被那块楼板碰头。
    last = None; still = 0
    for _ in range(3000):
        if "zone=ledge" in state(): break
        f = foot()
        still = still + 1 if last and math.dist(f, last) < 0.5 else 0
        last = f
        if still > 20: break
        pawn.add_movement_input(unreal.Vector(L[0], L[1], 0), 1.0, False); yield
    yield from wait(30)
    f = foot()
    check("zone=ledge" in state() and abs(f[2] - 2000) < 6 and mv.get_editor_property("respawn_count") == 0,
          "顺着镜光一直走，落在东边的石台上：脚 (%.0f, %.0f, %.0f)（石台面高 2000），半径 %.0f，%s" % (f[0], f[1], f[2], math.hypot(f[0], f[1]), state()[:22]))
    # 走到日之龛跟前，打开
    niche = G["consts"]["sunNiche"]
    for _ in range(600):
        f = foot(); dx, dy = niche[0] - f[0], niche[1] - f[1]; d = math.hypot(dx, dy)
        if d < 130: break
        pawn.add_movement_input(unreal.Vector(dx / d, dy / d, 0), 1.0, False); yield
    yield from wait(40)
    yield from shoot(w, "日之龛跟前_提示")
    # 把时刻拨到三相像还暗着的时候（人就站在镜光里，光会把画面罩住），从侧后方看清盖子往哪边掀、碎片升起来
    f = foot()
    tc.set_forced_time(29.9)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-24.0, yaw=math.degrees(math.atan2(niche[1] - f[1], niche[0] - f[0])) + 42.0))
    yield from wait(150)
    yield from shoot(w, "日之龛_关着")
    check(director.debug_interact("sunNiche"), "按 E 打开日之龛")
    yield from wait(100)
    yield from shoot(w, "日之龛打开")
    yield from wait(120)
    check(director.get_editor_property("sun_niche_open") and not director.debug_interact("sunNiche"), "日之龛开了（只能开一次）")
    yield from shoot(w, "得到太阳碎片")
    # ── D 夜里：月相、反射的月光（放在最后：入夜以后屋顶另外半圈降成楼梯并一直留着，最低一级就在日之龛石台上方；
    #    要是测完夜里再切回白天，它会挡在镜光的尽头。真实游戏里入夜以后不会再有这束日光。）──
    director.debug_set_night(True)
    tc.set_forced_time(125.42837)
    n = ok = 0; notes = []
    for r in G["night"]:
        director.debug_set_mirror_slot(r["slot"])
        yield from wait(260)
        e1 = mirror_same(r["mirror"], mirror()); e2 = beam_same(r["beam"], json.loads(beams["mmoon"].debug_solve(r["H"])), walk=False)
        n += 1; ok += 1 if not (e1 or e2) else 0
        if e1 or e2: notes.append("第 %d 格：%s %s" % (r["slot"], e1, e2))
    check(ok == n, "夜里转四格：%d/%d 格的月相和反射的月光和灰盒一致" % (ok, n))
    for x in notes: say("     " + x)
    director.debug_set_mirror_slot(1)
    yield from wait(260)
    n = ok = 0; notes = []
    for r in G["nightSweep"]:
        e = beam_same(r["beam"], json.loads(beams["mmoon"].debug_solve(r["H"])), walk=False)
        n += 1; ok += 1 if not e else 0
        if e: notes.append("时刻 %.1f：%s" % (r["H"], e))
        yield
    check(ok == n, "夜里那一格、月亮在 %d 个不同时刻：反射的月光 %d 个和灰盒一致" % (n, ok))
    for x in notes[:6]: say("     " + x)
    cmd(w, "Dysis.Go -836 -996 1480")
    yield from wait(30)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=2.0, yaw=-14.0))
    tc.set_forced_time(125.42837)
    yield from wait(60)
    yield from shoot(w, "夜里_月相和反射的月光")
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
_mi = R(); _mi.h = unreal.register_slate_post_tick_callback(_mi.tick)
