# 夜里第三段的测试（UE 编辑器 Python，开 PIE 跑）：瀑布后面的女神、半桥、影桥、放苹果和结局。
#   · 在月桥上一步步往下走：每一步月光照着她怀里的月亮多少——和灰盒比；站在对的那一步 1 秒，她亮起来，半桥伸出来；
#   · 走上半桥：屋顶细桥的月影接上，变成影桥；从池沿一路走到水亭；
#   · 细桥月影的位置、女神身上十个取样点照没照到——几个时刻和灰盒比；
#   · 把金苹果放上月托：4 秒后结局的对话开始。
# 标准答案：Tools/greybox/golden/greybox_night3.json（月桥的点在 greybox_night2.json 里）。结果写在 项目/Saved/pie_night3.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_night3.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_night3.json", encoding="utf-8"))
PTS = json.load(open(PROJ + "Tools/greybox/golden/greybox_night2.json", encoding="utf-8"))["consts"]["bridge"]["pts"]
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
    yield from wait(90)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def state(): return str(tc.describe_state())
    def zone(): return state().split("zone=")[1].split(" ")[0]
    def H(): return tc.get_editor_property("h")
    def fin(): return json.loads(director.describe_finale())
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def hold(sec):
        t = now()
        while now() - t < sec: yield
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f night" % (p[0], p[1], p[2] + dz))

    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    d = fin()
    check(not d["goddess"] and d["halfExt"] == 0 and not d["halfOn"] and not d["placed"], "开局：女神没亮，半桥还没有")
    director.debug_set_night(True); director.debug_set_mirror_slot(1); director.debug_show_moon_bridge()
    go(PTS[40], 10.0)
    yield from wait(360)

    # ── A 月桥上一步步往下走 ──
    fl = {f["step"]: f for f in G["flow"]}
    n = ok = 0; notes = []
    for s in fl["scanBridge"]["scan"]:
        go(PTS[s["i"]], 10.0)
        yield from hold(0.22)
        # 月光的边刚扫到某个取样点的那一步，UE 和灰盒可能差一个点（五个点里的一个）；别的都要一样
        d = fin(); good = abs(d["lit"] - s["lit"]) < (0.21 if 0 < max(d["lit"], s["lit"]) < 0.5 else 0.01) and abs(H() - s["H"]) < 0.05 and zone() == "moonbr"
        n += 1; ok += 1 if good else 0
        if not good: notes.append("第 %d 点：灰盒 H=%.2f 照着 %.1f；UE %s 照着 %.1f" % (s["i"], s["H"], s["lit"], state()[:30], d["lit"]))
    d = fin()
    check(ok == n and d["glint"] and not d["goddess"], "月桥上 %d 步：每一步月光照着她怀里的月亮多少——%d 步和灰盒一致；只是走过去她不会亮（只亮一下）" % (n, ok))
    for x in notes[:8]: say("     " + x)
    st = fl["stand"]
    go(PTS[st["i"]], 10.0); look(100.0, -6.0)
    t0 = now()
    for _ in range(600):
        yield
        if fin()["goddess"]: break
    took = now() - t0
    check(fin()["goddess"] and abs(took - st["seconds"]) < 0.4 and zone() == "moonbr" and abs(H() - st["H"]) < 0.05, "在对的那一步站住：%.2f 秒后女神亮起来（灰盒 %.2f 秒），%s" % (took, st["seconds"], state()[:40]))
    yield from shoot(w, "月桥上_月光落进女神怀里")
    t0 = now()
    for _ in range(600):
        yield
        if fin()["halfExt"] >= 1: break
    d = fin()
    check(d["halfExt"] == 1 and d["halfOn"] and abs(now() - t0 - 2.0) < 0.4, "半桥 %.1f 秒伸到头（灰盒 2.0 秒），能踩了" % (now() - t0))

    # ── B 水庭：池沿、半桥、影桥、水亭 ──
    p = fl["poolEdge"]
    go(p["foot"]); look(C["GOD"]["halfAz"] + 180.0, -8.0)
    yield from wait(40)
    check(zone() == "L0" and abs(H() - p["H"]) < 0.05 and fin()["shadowOn"] < 0.01, "走到水池东北边的池沿：%s，影子还没接上" % state()[:40])
    yield from shoot(w, "池沿_半桥和细桥的月影")
    p = fl["onHalfBridge"]
    go(p["foot"], 10.0)
    t0 = now()
    for _ in range(600):
        yield
        if fin()["shadowOn"] > 0.99: break
    d = fin()
    # 影桥亮起来的时候，UE 里人有身宽，站在这个位置会被影桥的面托上去（区域变成 shadowbr）；灰盒里人是一个点，还算在半桥上
    check(zone() in ("gbridge", "shadowbr") and abs(H() - p["H"]) < 0.02 and d["shadowOn"] > 0.99 and d["shadowFloor"] and abs(now() - t0 - p["seconds"]) < 0.5, "站上半桥：%s，%.2f 秒后月影接上，亮成影桥（灰盒 %.2f 秒）" % (state()[:40], now() - t0, p["seconds"]))
    yield from shoot(w, "半桥上_影桥亮起来")
    n = ok = 0; notes = []
    for key in ("onShadow", "pavilion"):
        p = fl[key]
        go(p["foot"], 10.0)
        yield from wait(30)
        good = zone() == p["zone"] and abs(H() - p["H"]) < 0.02 and abs(foot()[2] - p["foot"][2]) < 5
        n += 1; ok += 1 if good else 0
        if not good: notes.append("%s：灰盒 %s 高 %.0f；UE %s 脚高 %.0f" % (key, p["zone"], p["foot"][2], state()[:30], foot()[2]))
    check(ok == n, "影桥上、水亭里：区域、时刻、高度和灰盒一致")
    for x in notes: say("     " + x)
    # 从池沿一路走到水亭
    go(fl["poolEdge"]["foot"])
    yield from wait(30)
    zones = set(); last = None; still = 0; done = False; a = math.radians(C["GOD"]["halfAz"])
    for _ in range(4000):
        f = foot(); z = zone(); zones.add(z)
        if z == "pav" and math.hypot(f[0], f[1]) < 200: done = True; break
        tgt = (math.hypot(f[0], f[1]) - 150) ; tx, ty = tgt * math.cos(a) - f[0], tgt * math.sin(a) - f[1]; dd = math.hypot(tx, ty) or 1.0
        pawn.add_movement_input(unreal.Vector(tx / dd, ty / dd, 0), 1.0, False)
        still = still + 1 if last and math.dist(f, last) < 0.3 else 0
        last = f
        if still > 90: break
        yield
    f = foot()
    check(done and zones <= {"L0", "gbridge", "shadowbr", "pav"} and mv.get_editor_property("respawn_count") == 0, "从池沿走上半桥、走过影桥、走进水亭：脚 (%.0f, %.0f, %.0f)，一路的区域 %s（走到了=%s，卡住=%s，回落脚点 %d 次，%s）" % (f[0], f[1], f[2], sorted(zones), done, still > 90, mv.get_editor_property("respawn_count"), state()[:28]))
    look(C["GOD"]["halfAz"], 4.0)
    yield from wait(10)
    yield from shoot(w, "水亭里_回头看影桥")

    # ── C 细桥的月影、女神身上的取样点 ──
    n = ok = 0; worst = 0.0
    for r in G["shadow"]:
        got = json.loads(str(director.debug_bridge_shadow(r["H"])))
        good = (got is None) == (r["quad"] is None)
        if good and got: dmax = max(math.dist(a2, b2) for a2, b2 in zip(got, r["quad"])); worst = max(worst, dmax); good = dmax < 1.5
        n += 1; ok += 1 if good else 0
    check(ok == n, "细桥的月影 %d 个时刻：四个角的位置 %d 个和灰盒一致（最多差 %.1f cm）" % (n, ok, worst))
    n = ok = 0; notes = []
    for r in G["god"]:
        tc.set_forced_time(r["H"])
        yield from wait(50)
        got = str(director.debug_goddess_lit()); exp = ",".join(r["samples"]) + "|" + ",".join(r["sweep"])
        n += 1; ok += 1 if got == exp else 0
        if got != exp: notes.append("时刻 %.2f：灰盒 %s；UE %s" % (r["H"], exp, got))
    check(ok == n, "女神身上十个取样点、%d 个时刻：照没照到 %d 个和灰盒一致" % (n, ok))
    for x in notes[:6]: say("     " + x)
    tc.clear_forced_time()

    # ── D 放苹果、结局 ──
    go(fl["pavilion"]["foot"], 10.0); look(C["GOD"]["halfAz"] + 180.0, 10.0)
    yield from wait(40)
    check(director.debug_interact("placeApple") and fin()["placed"] and not director.debug_interact("placeApple"), "在水亭里按 E：把金苹果放上浑天仪的月托")
    yield from hold(2.0)
    yield from shoot(w, "金苹果放上月托")
    yield from hold(2.6)
    d = fin()
    check(d["ending"] and d["talking"] and d["endingLines"] == 2, "4 秒后结局的对话开始（没集齐三片碎片的那一段，%d 句）" % d["endingLines"])
    yield from shoot(w, "结局的对话")
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
_n3 = R(); _n3.h = unreal.register_slate_post_tick_callback(_n3.tick)
