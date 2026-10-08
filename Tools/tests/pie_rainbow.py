# 虹那条支线的测试（UE 编辑器 Python，开 PIE 跑）：
#   · 伊莉丝浮雕：人站在“对的位置”周围 45 个地方，影子的头离人形多远、算不算对上——和灰盒比；
#   · 站到对的位置：一秒多以后虹醒过来，虹桥长出来、窗下的石沿伸出来（时间表和灰盒的公式比）；
#   · 从浮雕旁边走上虹桥，一路走到南窗下的石沿（高度和灰盒比，中途往边上走掉不下去）；
#   · 打开虹之龛：碎片、铜柱和棱镜升起来；
#   · 棱镜：8 格 × 5 个时刻，七色各落在哪——和灰盒比；一格一格转到靛色，塞勒涅醒过来，对话开始；
#   · 入夜：虹桥消失（放在最后）。
# 标准答案：Tools/greybox/golden/greybox_rainbow.json。结果写在 项目/Saved/pie_rainbow.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_rainbow.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_rainbow.json", encoding="utf-8"))
C = G["consts"]; BR = C["BRIDGE"]
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
def bridge_z(t): return C["F1"] + (C["SILL"]["y"] - C["F1"]) * t + BR["bump"] * math.sin(math.pi * t)

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
    def state(): return str(tc.describe_state())
    def zone(): return state().split("zone=")[1].split(" ")[0]
    def rb(): return json.loads(director.describe_rainbow())
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f" % (p[0], p[1], p[2] + dz))
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    d = rb()
    check(not d["done"] and not d["bridgeOn"] and d["sillK"] == 0 and d["ext"] == 0 and d["rails"] == len(G["rails"]), "开局：浮雕没解开，虹桥、石沿、棱镜都还没有；桥边护栏 %d 块（灰盒 %d 块）" % (d["rails"], len(G["rails"])))

    # ── A 浮雕：一圈位置 ──
    n = ok = 0; notes = []
    for r in G["align"]:
        go(r["foot"])
        yield from wait(14)
        director.debug_reset_relief()
        yield from wait(5)
        d = rb(); got_ok = d["align"] > 0; director.debug_reset_relief()
        miss_same = abs(d["miss"] - r["miss"]) < 3.0 or (d["miss"] > 300 and r["miss"] > 300)
        good = zone() == r["zone"] and miss_same and got_ok == r["ok"] and not d["done"]
        n += 1; ok += 1 if good else 0
        if not good: notes.append("横 %+.0f 纵 %+.0f：离人形 灰盒 %.1f UE %.1f cm，对上 灰盒 %s UE %s，区域 %s，窗光 %s" % (r["du"], r["dv"], r["miss"], d["miss"], r["ok"], got_ok, zone(), d["irisLight"]))
    check(ok == n, "浮雕：人站在对的位置周围 %d 个地方，影子的头离人形多远、算不算对上——%d 个和灰盒一致（其中 %d 个算对上）" % (n, ok, sum(1 for r in G["align"] if r["ok"])))
    for x in notes[:8]: say("     " + x)

    # ── B 站到差一点的地方（提示：七色淡淡地显出来），再站到对的位置 ──
    E = C["IRISREL"]["E"]; T = C["IRISREL"]["T"]
    go([E[0] + T[0] * 55, E[1] + T[1] * 55, C["F1"]]); look(79.0, 8.0)
    yield from wait(40)
    yield from shoot(w, "浮雕_差一点_七色淡淡显出来")
    go([E[0], E[1], C["F1"]])
    t0 = None; frames = 0
    for _ in range(400):
        yield
        d = rb()
        if t0 is None and d["align"] > 0: t0 = w.get_time_seconds() if hasattr(w, "get_time_seconds") else unreal.GameplayStatics.get_time_seconds(w)
        if d["done"]: break
    t1 = unreal.GameplayStatics.get_time_seconds(w)
    took = (t1 - t0) if t0 is not None else -1
    check(d["done"] and 1.1 < took < 2.0, "站到对的位置：%.2f 秒以后虹醒过来（灰盒 %.2f 秒）" % (took, G["flow"][0]["seconds"]))
    yield from shoot(w, "浮雕_对上了_整圈亮起来")
    # 虹桥长出来、石沿伸出来：和公式比
    look(-146.5, 18.0)
    worst = 0.0; on_ok = True; shot = False
    for _ in range(360):
        yield
        d = rb(); Tn = d["T"]
        worst = max(worst, abs(d["bridgeK"] - smooth(0.6, 3.8, Tn)), abs(d["sillK"] - smooth(2.4, 4.2, Tn)))
        if Tn > 3.9 and not d["bridgeOn"]: on_ok = False
        if Tn < 3.7 and d["bridgeOn"]: on_ok = False
        if Tn > 2.2 and not shot: shot = True; yield from shoot(w, "虹桥长到一半")
        if Tn > 5.0: break
    d = rb()
    check(worst < 0.02 and on_ok and d["bridgeOn"] and d["sillK"] == 1 and d["railsOn"] == len(G["rails"]), "虹桥 3.8 秒长到头、能踩了，石沿 4.2 秒伸到头（和灰盒的时间表差 %.3f）；护栏 %d 块都立起来" % (worst, d["railsOn"]))
    yield from shoot(w, "虹桥长到头")

    # ── C 走上虹桥，一路走到窗台 ──
    ax = BR["ax"]; A = BR["A"]
    go(G["flow"][2]["pts"][0]["foot"], 8.0); look(-146.5, -8.0)
    yield from wait(30)
    check(zone() == "rainbow" and abs(tc.get_editor_property("h") - C["RELIEF"]["H"]) < 1e-3, "站上虹桥：" + state()[:44])
    worst = 0.0; zones = set(); side_done = False; side_ok = True; shot = False; last = None; still = 0
    for _ in range(4000):
        f = foot(); z = zone(); zones.add(z)
        if z == "sill": break
        t = ((f[0] - A[0]) * ax[0] + (f[1] - A[1]) * ax[1]) / BR["len"]
        # 高度按“脚下那块桥面”量（时间组件记的脚底）：桥头很陡，胶囊的底会比脚下的桥面高出几厘米
        if z == "rainbow" and 0.03 < t < 0.97:
            fs = tc.get_editor_property("foot_cm")
            ts = ((fs.x - A[0]) * ax[0] + (fs.y - A[1]) * ax[1]) / BR["len"]
            worst = max(worst, abs(fs.z - bridge_z(ts)))
        still = still + 1 if last and math.dist(f, last) < 0.5 else 0
        last = f
        if still > 30: break
        if t > 0.45 and not side_done:
            side_done = True
            for _ in range(60):
                pawn.add_movement_input(unreal.Vector(-ax[1], ax[0], 0), 1.0, False); yield
            side_ok = zone() == "rainbow"
            yield from shoot(w, "走在虹桥上")
            continue
        pawn.add_movement_input(unreal.Vector(ax[0], ax[1], 0), 1.0, False); yield
    yield from wait(20)
    f = foot()
    check(zone() == "sill" and abs(f[2] - C["SILL"]["y"]) < 4 and zones <= {"rainbow", "sill"} and worst < 4 and side_ok and mv.get_editor_property("respawn_count") == 0,
          "沿虹桥走到南窗下的石沿：脚 (%.0f, %.0f, %.0f)，一路的区域 %s，高度和灰盒最多差 %.1f cm，中途往边上走 1 秒%s" % (f[0], f[1], f[2], sorted(zones), worst, "还在桥上" if side_ok else "掉下去了"))
    check(abs(tc.get_editor_property("h") - C["RELIEF"]["H"]) < 1e-3, "窗台上的时刻停在浮雕醒过来的那一刻：" + state()[:44])
    # 往窗洞里走两步：还算窗台
    out = C["SILL"]["out"]
    for _ in range(50):
        pawn.add_movement_input(unreal.Vector(out[0], out[1], 0), 1.0, False); yield
    f = foot()
    check(zone() == "sill" and math.hypot(f[0], f[1]) > 1560, "走进窗洞里：半径 %.0f，%s" % (math.hypot(f[0], f[1]), state()[:22]))

    # ── D 虹之龛 ──
    check(not director.debug_interact("prism"), "棱镜还没升起来：转不了")
    go(G["flow"][3]["pts"][2]["foot"]); look(165.0, 4.0)
    yield from wait(30)
    yield from shoot(w, "石沿上_虹之龛跟前")
    check(director.debug_interact("irisNiche"), "按 E 打开虹之龛")
    worst = 0.0; shot = False
    for _ in range(600):
        yield
        d = rb()
        worst = max(worst, abs(d["ext"] - smooth(2.8, 5.8, d["nicheT"])))
        if d["nicheT"] > 1.6 and not shot: shot = True; yield from shoot(w, "虹之龛打开_碎片飞出来")
        if d["nicheT"] > 6.2: break
    d = rb()
    check(d["nicheOpen"] and d["ext"] == 1 and worst < 0.02 and not director.debug_interact("irisNiche"), "虹之龛开了（只能开一次）；铜柱和棱镜 5.8 秒升到头（和灰盒的时间表差 %.3f）" % worst)

    # ── E 棱镜：每一格、几个时刻，七色各落在哪 ──
    n = ok = 0; notes = []; far = 0.0
    for r in G["prism"]:
        got = json.loads(director.debug_prism_hits(r["slot"], r["H"]))
        good = got["near"] == r["near"]; worst_d = 0.0
        for k in range(7):
            e = r["hits"][k]; g = got["hits"][k]
            ep = e["p"] if e else None
            if (ep is None) != (g is None): good = False; continue
            if ep is not None: worst_d = max(worst_d, math.dist(ep, g))
        if worst_d > 6.0: good = False
        far = max(far, worst_d)
        n += 1; ok += 1 if good else 0
        if not good: notes.append("时刻 %.2f 第 %d 格：落在眼睛上的 灰盒 %s UE %s；落点最多差 %.1f cm" % (r["H"], r["slot"], r["near"], got["near"], worst_d))
        if n % 8 == 0: yield
    check(ok == n, "棱镜 8 格 × 5 个时刻：七色的落点、哪一色落在塞勒涅眼睛上——%d/%d 和灰盒一致（落点最多差 %.1f cm）" % (ok, n, far))
    for x in notes[:8]: say("     " + x)

    # ── F 一格一格转到靛色 ──
    go(G["flow"][5]["seq"][0]["foot"]); look(-10.0, -24.0)
    yield from wait(40)
    d = rb()
    check(zone() == "sill" and d["irisLight"] and d["prismInLight"], "站在棱镜旁边：南窗的光照着棱镜（%s）" % state()[:22])
    seq = []
    for i in range(1, 7):
        check(director.debug_interact("prism"), "转一格") if i == 1 else director.debug_interact("prism")
        yield from wait(50)
        d = rb(); seq.append((d["slot"], d["color"]))
        if i == 3: yield from shoot(w, "棱镜_七色光")
    exp = [(s["slot"], s["color"]) for s in G["flow"][5]["seq"][1:7]]
    check(seq == exp, "转六格：每一格落在塞勒涅眼睛上的颜色 %s（灰盒 %s）" % (seq, exp))
    yield from wait(70)
    d = rb()
    check(d["seleneOn"], "靛色的光停了 0.8 秒：塞勒涅醒了")
    yield from wait(110)
    check(rb()["talking"], "一秒多以后对话开始（伊莉丝 × 塞勒涅 × 狄西斯）")
    yield from shoot(w, "塞勒涅醒了_对话")
    # 去水庭北墙看她
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector): pass
    go(C["SELENE"]["ip"]); look(-5.0, 6.0)
    tc.set_forced_time(C["RELIEF"]["H"])
    yield from wait(60)
    yield from shoot(w, "塞勒涅的浮雕_靛色落在眼睛上")
    tc.clear_forced_time()

    # ── G 入夜：虹桥消失 ──
    director.debug_set_night(True)
    yield from wait(30)
    d = rb()
    check(not d["bridgeOn"] and d["railsOn"] == 0, "入夜以后：虹桥消失，踩不了")
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
_rbw = R(); _rbw.h = unreal.register_slate_post_tick_callback(_rbw.tick)
