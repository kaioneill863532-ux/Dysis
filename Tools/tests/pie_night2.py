# 夜里第二段的测试（UE 编辑器 Python，开 PIE 跑）：月之龛、双子、月桥。
#   · 月之龛、厚墙两块月石：镜子在对应的那一格，几个时刻，九个取样点照没照到——和灰盒比；
#   · 像站在回廊不同地方被月亮直射照着多少——和灰盒比；
#   · 走到厚墙前（墙透开）→ 把波吕丢刻斯拉出来 → 顺时针一路推回卡斯托耳身边 → 并肩，月桥出现，铜链收起来；
#   · 月桥上几个点的区域和时刻；从桥头一路走到水庭。
# 标准答案：Tools/greybox/golden/greybox_night2.json。结果写在 项目/Saved/pie_night2.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_night2.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_night2.json", encoding="utf-8"))
C = G["consts"]; TW = C["TWINS"]
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
    pc.get_hud().start_game(True)
    if os.path.exists(SAVED + "sfx_debug.flag"): unreal.SystemLibrary.execute_console_command(w, "Dysis.Sfx.Debug 1")   # 想看这次试玩放了哪些音效：先建一个空文件 Saved/sfx_debug.flag
    yield from wait(90)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
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
    def tw(): return json.loads(director.describe_twins())
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f night" % (p[0], p[1], p[2] + dz))
    def lit_samples(rows, stone):
        n = ok = 0; notes = []
        for r in rows:
            tc.set_forced_time(r["H"])
            yield from wait(50)
            got = str(director.debug_moonstone_lit(stone)).split(",")
            n += 1; ok += 1 if got == r["each"] else 0
            if got != r["each"]: notes.append("时刻 %.2f：灰盒 %s；UE %s" % (r["H"], ",".join(r["each"]), ",".join(got)))
        return n, ok, notes

    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    d = tw()
    check(d["state"] == 0 and d["dormant"] and d["rails"] == len(C["bridge"]["rails"]) and d["chainOn"] and d["chainShown"], "开局：波吕丢刻斯藏在墙里，月桥还没有（护栏 %d 块，灰盒 %d 块），桥头的铜链拦着" % (d["rails"], len(C["bridge"]["rails"])))
    director.debug_set_night(True)
    yield from wait(300)

    # ── A 月之龛：镜子在第 0 格 ──
    director.debug_set_mirror_slot(C["MSHRINE"]["slot"])
    tc.set_forced_time(C["H_3X"] - 3.0)
    yield from wait(150)
    n, ok, notes = yield from lit_samples(G["shrine"], "moonShrine")
    check(ok == n, "月之龛九个取样点、%d 个时刻（镜子在第 0 格）：照没照到 %d 个和灰盒一致" % (n, ok))
    for x in notes[:6]: say("     " + x)
    yield from wait(60)
    d = tw()
    check(d["shrinePerm"] and d["shrineK"] == 1, "月之龛照够了：墙透开，不再合上")
    check(director.debug_interact("moonShrine") and tw()["moonShard"] and not director.debug_interact("moonShrine"), "打开月之龛：得到月亮碎片（只能拿一次）")
    go(polar(C["MSHRINE"]["az"] + 4.0, 1420.0, 1450.0)); look(C["MSHRINE"]["az"] - 12.0, 8.0)
    yield from wait(40)
    yield from shoot(w, "月之龛透开了")

    # ── B 厚墙：镜子在夜里那一格 ──
    director.debug_set_mirror_slot(1)
    tc.set_forced_time(C["Hw"] - 6.0)
    yield from wait(150)
    n, ok, notes = yield from lit_samples(G["wall"], "twinWall")
    check(ok == n, "厚墙九个取样点、%d 个时刻（镜子在夜里那一格）：照没照到 %d 个和灰盒一致" % (n, ok))
    for x in notes[:6]: say("     " + x)

    # ── C 像被月亮直射照着多少 ──
    n = ok = 0; notes = []; lastH = None
    for r in G["statueLit"]:
        if r["H"] != lastH:
            lastH = r["H"]; tc.set_forced_time(r["H"])
            yield from wait(4)
        got = director.debug_statue_moon_lit(unreal.Vector(r["pos"][0], r["pos"][1], r["pos"][2]))
        n += 1; ok += 1 if abs(got - r["lit"]) < 0.01 else 0
        if abs(got - r["lit"]) >= 0.01: notes.append("时刻 %.2f 方位 %.1f°：灰盒 %.2f UE %.2f" % (r["H"], r["az"], r["lit"], got))
    check(ok == n, "像站在回廊上 %d 处 × 时刻：被月亮照着多少 %d 个和灰盒一致" % (n, ok))
    for x in notes[:8]: say("     " + x)
    tc.clear_forced_time()

    # ── D 走到厚墙前，拉出来，推回去 ──
    fl = {f["step"]: f for f in G["flow"]}
    go(fl["approach"]["foot"]); look(140.0, 0.0)
    yield from wait(90)
    d = tw()
    check(zone() == "L1" and abs(H() - fl["approach"]["H"]) < 0.02 and d["wallK"] == 1 and d["state"] == 1, "夜里从西边走近厚墙：%s，反射的月光扫到墙上，墙透开了，龛里站着波吕丢刻斯" % state()[:40])
    yield from shoot(w, "厚墙透开_龛里的波吕丢刻斯")
    go(fl["atWall"]["foot"])
    yield from wait(30)
    check(tw()["wallK"] > 0.6 and director.debug_interact("pullPollux"), "站在龛前（人在门洞里墙不合上）：按 E 把他拉出来")
    t0 = now()
    for _ in range(400):
        yield
        if tw()["state"] == 3: break
    d = tw()
    check(d["state"] == 3 and abs(now() - t0 - 1.6) < 0.2 and math.dist(d["pollux"], C["path"]["out"]) < 1.0, "%.2f 秒拉出来（灰盒 1.60 秒），站在回廊上" % (now() - t0))
    go(polar(TW["wallAz"] - 4.0, TW["rOut"], C["F1"])); look(TW["wallAz"] + 60.0, -10.0)
    yield from wait(20)
    t0 = now(); shot = False; behind = True
    for _ in range(4000):
        d = tw()
        if d["state"] != 3 or now() - t0 > 25: break
        a, r = az_r(); t = math.radians(a); corr = max(-0.8, min(0.8, (TW["rOut"] - r) / 60.0))
        pawn.add_movement_input(unreal.Vector(-math.sin(t) + math.cos(t) * corr, math.cos(t) + math.sin(t) * corr, 0), 1.0, False)
        if ((a - d["polluxAz"] + 540) % 360 - 180) > -0.4: behind = False
        if d["polluxAz"] > 200 and not shot: shot = True; look(a + 60.0, -10.0); yield from shoot(w, "推着波吕丢刻斯往回走")
        yield
    took = now() - t0
    yield from wait(60)
    d = tw(); exp = fl["push"]
    check(d["joined"] and abs(d["polluxAz"] - TW["endAz"]) < 0.01 and d["lit"] > 0.99 and d["litC"] > 0.99 and abs(took - exp["seconds"]) < 1.0 and behind,
          "顺时针一路推回卡斯托耳身边：用了 %.1f 秒（灰盒 %.1f 秒），停在方位 %.1f°，并肩=%s，月光照着 波吕丢刻斯 %.2f / 卡斯托耳 %.2f，人一直在他身后=%s；%s" % (took, exp["seconds"], d["polluxAz"], d["joined"], d["lit"], d["litC"], behind, state()[:30]))
    check(d["bridgeK"] == 1 and not d["dormant"] and d["railsOn"] == d["rails"] and not d["chainOn"] and not d["chainShown"], "月桥出现（护栏 %d 块立起来），桥头的铜链收起来了" % d["railsOn"])
    look(30.0, -12.0)
    yield from wait(10)
    yield from shoot(w, "双子并肩_月桥出现")

    # ── E 月桥 ──
    n = ok = 0; notes = []
    for p in fl["bridge"]["pts"]:
        if p["zone"] != "moonbr": continue
        go(p["foot"], 10.0)
        yield from wait(25)
        good = zone() == "moonbr" and abs(H() - p["H"]) < 0.25 and abs(foot()[2] - p["foot"][2]) < 8
        # 桥落地前最后半米紧挨着水闸石台的第一级台阶：UE 里人有 30 cm 的身宽，会先蹭上那一级（脚下算水庭了），灰盒里人是一个点、还在桥上
        if p["i"] >= len(C["bridge"]["pts"]) - 2 and zone() == "L0" and abs(foot()[2] - p["foot"][2]) < 12: good = True
        n += 1; ok += 1 if good else 0
        if not good:
            fa = [x for x in mv.get_editor_property("current_floor").hit_result.to_tuple() if isinstance(x, unreal.Actor)]
            notes.append("第 %d 点：灰盒 H=%.2f 高 %.0f；UE %s 脚高 %.0f，脚下是 %s" % (p["i"], p["H"], p["foot"][2], state()[:30], foot()[2], fa[0].get_actor_label() if fa else "?"))
    check(ok == n, "月桥上 %d 个点：区域、时刻、高度 %d 个和灰盒一致" % (n, ok))
    for x in notes: say("     " + x)
    pts = C["bridge"]["pts"]
    go(polar(TW["castorAz"] - 2.0, 1330.0, C["F1"]))
    yield from wait(30)
    zones = set(); last = None; still = 0; done = False; shot = False
    for _ in range(6000):
        f = foot(); z = zone(); zones.add(z)
        i = min(range(len(pts)), key=lambda k: (pts[k][0] - f[0]) ** 2 + (pts[k][1] - f[1]) ** 2)
        if z == "L0" and i >= len(pts) - 4: done = True; break
        tgt = pts[min(len(pts) - 1, i + 3)] if i < len(pts) - 2 else [pts[-1][0] * 1.06, pts[-1][1] * 1.06, 0]
        dx, dy = tgt[0] - f[0], tgt[1] - f[1]; dd = math.hypot(dx, dy) or 1.0
        pawn.add_movement_input(unreal.Vector(dx / dd, dy / dd, 0), 1.0, False)
        if i == 36 and not shot: shot = True; look(math.degrees(math.atan2(dy, dx)), -14.0); yield from shoot(w, "走在月桥上")
        still = still + 1 if last and math.dist(f, last) < 0.3 else 0
        last = f
        if still > 90: break
        yield
    yield from wait(20)
    f = foot()
    check(done and zones <= {"L1", "moonbr", "L0"} and mv.get_editor_property("respawn_count") == 0, "从双子脚下走上月桥，一路走到水庭东边：脚 (%.0f, %.0f, %.0f)，一路的区域 %s" % (f[0], f[1], f[2], sorted(zones)))
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
_n2 = R(); _n2.h = unreal.register_slate_post_tick_callback(_n2.tick)
