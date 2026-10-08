# 夜里第一段的测试（UE 编辑器 Python，开 PIE 跑）：天鹅、墙里的两段楼梯、东南墙的月亮浮雕。
#   · 天鹅：8 格 × 9 个时刻，月光照着她多少、天鹅头的影子离浮雕的空白多远——和灰盒比；
#   · 站到女神像旁边，转到对的那一格：0.7 秒后解开，浮雕和门沉下去，楼梯的窗一扇扇打开、下门的石板沉下去；
#   · 顺着墙里的楼梯（TS）从三层走到二层；
#   · 三相像转到夜里那一格：反射的月光落在东南墙的月亮浮雕上——九个取样点照没照到，13 个时刻和灰盒比；浮雕隐去；
#   · 顺着另一段楼梯（TR）从二层走到一层。
# 标准答案：Tools/greybox/golden/greybox_night1.json。结果写在 项目/Saved/pie_night1.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_night1.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_night1.json", encoding="utf-8"))
C = G["consts"]
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
def smooth(a, b, x):
    t = min(1.0, max(0.0, (x - a) / (b - a))); return t * t * (3 - 2 * t)
FORM = {"dark": 0, "sun": 1, "moon": 2}

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
    def az_r():
        f = foot(); return math.degrees(math.atan2(f[1], f[0])) % 360, math.hypot(f[0], f[1])
    def state(): return str(tc.describe_state())
    def zone(): return state().split("zone=")[1].split(" ")[0]
    def H(): return tc.get_editor_property("h")
    def night(): return json.loads(director.describe_night())
    def mirror(): return json.loads(director.describe_mirror())
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f night" % (p[0], p[1], p[2] + dz))   # 带 night：这条命令不带的话会把时间切回白天
    def walk_stairs(a_from, a_to, z_end, zone_end):
        """沿墙里的楼梯走：先从门口往外走进楼梯（半径 16.3 m），顺着方位角走到另一头，再往里走出下门。"""
        sgn = 1.0 if ((a_to - a_from + 540) % 360 - 180) > 0 else -1.0
        last = None; still = 0
        for _ in range(6000):
            a, r = az_r(); f = foot()
            if zone() == zone_end and abs(f[2] - z_end) < 8 and r < 1535: return True
            t = math.radians(a); left = ((a_to - a + 540) % 360 - 180) * sgn
            want_r = 1630.0 if left > 0.6 else 1420.0
            tx, ty = -math.sin(t) * sgn, math.cos(t) * sgn      # 方位角增大的方向 = (−sin, cos)
            if left <= 0.6: tx, ty = 0.0, 0.0
            elif r < 1600.0 and abs((a - a_from + 540) % 360 - 180) < 3.0:
                # 还在上门口：先正对着门往外走进楼梯，别沿着墙滑走
                off = ((a_from - a + 540) % 360 - 180) * sgn
                tx, ty = tx * max(-1.0, min(1.0, off)), ty * max(-1.0, min(1.0, off))
            corr = max(-1.0, min(1.0, (want_r - r) / 60.0))
            pawn.add_movement_input(unreal.Vector(tx + math.cos(t) * corr, ty + math.sin(t) * corr, 0), 1.0, False)
            still = still + 1 if last and math.dist(f, last) < 0.3 else 0
            last = f
            if still > 90: return False
            yield
        return False

    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    d = night()
    check(not d["swan"]["solved"] and d["swan"]["slot"] == C["SWAN"]["k0"] and d["stones"]["moonRelief"]["k"] == 0, "开局：天鹅没解开（底座在第 %d 格），月亮浮雕还在" % d["swan"]["slot"])
    director.debug_set_night(True)
    yield from wait(300)

    # ── A 天鹅：每一格、几个时刻 ──
    n = ok = 0; notes = []
    for r in G["swan"]:
        tc.set_forced_time(r["H"]); director.debug_set_swan(r["k"], 1.0)
        yield from wait(3)
        s = night()["swan"]; director.debug_set_swan(r["k"], 1.0)
        miss_same = abs(s["miss"] - r["miss"]) < 3.0 or (s["miss"] > 800 and r["miss"] > 800)
        good = abs(s["lit"] - r["lit"]) < 0.01 and miss_same and math.dist(s["head"], r["head"]) < 1.5
        n += 1; ok += 1 if good else 0
        if not good: notes.append("时刻 %.2f 第 %d 格：照着 灰盒 %.2f UE %.2f；影子差 灰盒 %.1f UE %.1f cm；头的位置差 %.1f cm" % (r["H"], r["k"], r["lit"], s["lit"], r["miss"], s["miss"], math.dist(s["head"], r["head"])))
    check(ok == n, "天鹅 8 格 × 9 个时刻：月光照着她多少、头的影子离浮雕的空白多远——%d/%d 和灰盒一致" % (ok, n))
    for x in notes[:8]: say("     " + x)
    tc.clear_forced_time(); director.debug_set_swan(C["SWAN"]["k0"], 0.0)

    # ── B 站到女神像旁边，转到对的那一格 ──
    fl = {f["step"]: f for f in G["flowSwan"]}
    go(fl["arrive"]["foot"]); look(250.0, 4.0)
    yield from wait(120)
    s = night()["swan"]
    check(zone() == "L3" and abs(H() - fl["arrive"]["H"]) < 0.02 and s["lit"] > 0.99 and s["form"] > 0.95, "夜里站到女神像旁边：%s，月光照满她，黑天鹅显出来了（%.2f）" % (state()[:40], s["form"]))
    yield from shoot(w, "女神像变成黑天鹅")
    check(director.debug_interact("swanRelief"), "解开以前可以看浮雕")
    presses = 0
    while night()["swan"]["slot"] != 0 and presses < 9:
        director.debug_interact("swan"); presses += 1
        t_press = unreal.GameplayStatics.get_time_seconds(w)
        while unreal.GameplayStatics.get_time_seconds(w) - t_press < 0.667: yield      # 和灰盒一样每 0.67 秒按一次
    t0 = unreal.GameplayStatics.get_time_seconds(w)
    for _ in range(300):
        yield
        if night()["swan"]["solved"]: break
    took = unreal.GameplayStatics.get_time_seconds(w) - t0
    s = night()["swan"]
    check(s["solved"] and presses == fl["solve"]["presses"] and abs(took - fl["solve"]["secondsAfterLastPress"]) < 0.25, "转 %d 次转到对的那一格，%.2f 秒后解开（灰盒 %d 次、%.2f 秒）；影子差 %.1f cm" % (presses, took, fl["solve"]["presses"], fl["solve"]["secondsAfterLastPress"], s["miss"]))
    worst = 0.0; shot = False
    for _ in range(400):
        yield
        d = night(); s = d["swan"]; st = d["stairs"][0]; k = smooth(0, 2.6, s["openT"])
        worst = max(worst, abs(s["doorZ"] + 260 * k), abs(s["reliefZ"] + 305 * k))
        for i, o in enumerate(st["win"]): worst = max(worst, 100 * abs(o - min(1.0, max(0.0, (st["t"] - 0.25 * i) / 0.8))))
        if s["openT"] > 1.3 and not shot: shot = True; yield from shoot(w, "浮雕和门沉下去")
        if s["openT"] > 2.9 and st["seal"] >= 1: break
    d = night(); s = d["swan"]; st = d["stairs"][0]
    check(worst < 3 and abs(s["doorZ"] + 260) < 1 and abs(s["reliefZ"] + 305) < 1 and st["win"] == [1, 1, 1] and st["seal"] == 1 and abs(st["sealZ"] + 260) < 1 and not director.debug_interact("swanRelief"),
          "浮雕沉下去 3.05 m、门沉下去 2.6 m；三扇窗依次打开；下门的石板沉下去（和灰盒的时间表最多差 %.1f）" % worst)

    # ── C 墙里的楼梯 TS：几个点的区域和时刻，再实走一遍 ──
    n = ok = 0; notes = []
    for p in fl["stairsTS"]["pts"]:
        go(p["foot"], 10.0)
        yield from wait(25)
        good = zone() == p["zone"] and abs(H() - p["H"]) < 0.05 and abs(foot()[2] - p["foot"][2]) < 26
        n += 1; ok += 1 if good else 0
        if not good: notes.append("方位 %.1f°：灰盒 %s H=%.2f 高 %.0f；UE %s 脚高 %.0f" % (p["az"], p["zone"], p["H"], p["foot"][2], state()[:30], foot()[2]))
    check(ok == n, "楼梯 TS 上 %d 个点：区域和时刻 %d 个和灰盒一致" % (n, ok))
    for x in notes: say("     " + x)
    go([C["SWAN"]["face"][0] + C["SWAN"]["n"][0] * 60, C["SWAN"]["face"][1] + C["SWAN"]["n"][1] * 60, C["F3"]]); look(-30.0, -10.0)
    yield from wait(30)
    done = yield from walk_stairs(C["TS"]["topDoor"]["az"], C["TS"]["botDoor"]["az"], C["F2"], "L2")
    a, r = az_r()
    check(done and mv.get_editor_property("respawn_count") == 0, "从三层的门走进墙里的楼梯，一路走到二层：方位 %.1f° 半径 %.0f，%s" % (a, r, state()[:40]))
    yield from shoot(w, "从楼梯下到二层")

    # ── D 三相像转到夜里那一格，月亮浮雕 ──
    fm = {f["step"]: f for f in G["flowMrel"]}
    go(fm["arrive"]["foot"]); look(120.0, 0.0)
    yield from wait(120)
    m = mirror()
    check(zone() == "L2" and abs(H() - fm["arrive"]["H"]) < 0.02 and m["slot"] == fm["arrive"]["slot"] and m["form"] == FORM[fm["arrive"]["form"]], "夜里站到三相像旁边：%s，镜子还在白天那一格，已经是月相" % state()[:40])
    seq = []
    for e in fm["turn"]["seq"]:
        director.debug_interact("mirrorCrank")
        yield from wait(75)
        m = mirror(); st = night()["stones"]["moonRelief"]
        seq.append((m["slot"], m["form"], round(st["lit"], 2)))
    exp = [(e["slot"], FORM[e["form"]], round(e["relLit"], 2)) for e in fm["turn"]["seq"]]
    check(seq == exp, "转五格到夜里那一格：每一格是哪一相、月亮浮雕照到多少 %s（灰盒 %s）" % (seq, exp))
    yield from shoot(w, "反射的月光照向月亮浮雕")
    n = ok = 0; notes = []
    for r in G["mrel"]:
        tc.set_forced_time(r["H"])
        yield from wait(50)
        got = str(director.debug_moonstone_lit("moonRelief")).split(",")
        good = got == r["each"]
        n += 1; ok += 1 if good else 0
        if not good: notes.append("时刻 %.2f：灰盒 %s；UE %s" % (r["H"], ",".join(r["each"]), ",".join(got)))
    check(ok == n, "月亮浮雕九个取样点、%d 个时刻：照没照到 %d 个和灰盒一致" % (n, ok))
    for x in notes[:6]: say("     " + x)
    tc.clear_forced_time()
    yield from wait(240)
    d = night(); st = d["stones"]["moonRelief"]; tr = d["stairs"][1]
    check(st["perm"] and st["k"] == 1 and tr["seal"] == 1 and tr["win"] == [1, 1], "月亮浮雕照够了就永远隐去；它后面那段楼梯的窗和下门也开了")

    # ── E 墙里的楼梯 TR ──
    n = ok = 0; notes = []
    for p in fm["stairsTR"]["pts"]:
        go(p["foot"], 10.0)
        yield from wait(25)
        good = zone() == p["zone"] and abs(H() - p["H"]) < 0.05 and abs(foot()[2] - p["foot"][2]) < 26
        n += 1; ok += 1 if good else 0
        if not good: notes.append("方位 %.1f°：灰盒 %s H=%.2f 高 %.0f；UE %s 脚高 %.0f" % (p["az"], p["zone"], p["H"], p["foot"][2], state()[:30], foot()[2]))
    check(ok == n, "楼梯 TR 上 %d 个点：区域和时刻 %d 个和灰盒一致" % (n, ok))
    for x in notes: say("     " + x)
    a0 = math.radians(C["TR"]["topDoor"]["az"])
    go([1480 * math.cos(a0), 1480 * math.sin(a0), C["F2"]]); look(180.0, -10.0)
    yield from wait(30)
    done = yield from walk_stairs(C["TR"]["topDoor"]["az"], C["TR"]["botDoor"]["az"], C["F1"], "L1")
    a, r = az_r()
    check(done and mv.get_editor_property("respawn_count") == 0, "从月亮浮雕后面的门走进楼梯，一路走到一层：方位 %.1f° 半径 %.0f，%s" % (a, r, state()[:40]))
    yield from shoot(w, "从楼梯下到一层")
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
_n1 = R(); _n1.h = unreal.register_slate_post_tick_callback(_n1.tick)
