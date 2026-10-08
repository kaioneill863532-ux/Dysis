# 窗光的核对（UE 编辑器 Python，开 PIE 跑）：把时刻摆到一串 H，让每束窗光重算，
# 和灰盒导出的标准答案（Tools/greybox/golden/greybox_beams.json）逐项比：有没有光、照亮比例、能不能踩、
# 四个角照多远、能踩的面从哪到哪。结果写在 项目/Saved/pie_beams.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_beams.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_beams.json", encoding="utf-8"))
TOL_LEN = 12.0     # 照射距离、踩踏面起止：厘米
TOL_POS = 1.5      # 窗上的角点：厘米
lines = []; stat = {"n": 0, "bad": 0}
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def wait(n):
    for _ in range(n): yield
def cmd(w, s): unreal.SystemLibrary.execute_console_command(w, s)

def compare(H, exp, got, label):
    """返回这一束在这个时刻哪里对不上（空 = 全对）。"""
    d = []
    if exp["valid"] != got["valid"]: return ["有没有光 灰盒 %s UE %s" % (exp["valid"], got["valid"])]
    if abs(exp["lit"] - got["lit"]) > 0.0251: d.append("照亮比例 灰盒 %.3f UE %.3f" % (exp["lit"], got["lit"]))
    if not exp["valid"]: return d
    for k in ("O", "b0", "b1"):
        if max(abs(a - b) for a, b in zip(exp[k], got[k])) > TOL_POS and abs(exp["lit"] - got["lit"]) < 1e-6: d.append("%s 差 %.1f cm" % (k, max(abs(a - b) for a, b in zip(exp[k], got[k]))))
    if max(abs(a - b) for a, b in zip(exp["L"], got["L"])) > 2e-4: d.append("方向差")
    same_rect = abs(exp["lit"] - got["lit"]) < 1e-6 and max(abs(a - b) for a, b in zip(exp["O"], got["O"])) <= TOL_POS
    if same_rect:
        dc = max(abs(a - b) for a, b in zip(exp["c"], got["c"]))
        if dc > TOL_LEN: d.append("四角照射距离差 %.0f cm（灰盒 %s，UE %s）" % (dc, [round(x) for x in exp["c"]], [round(x) for x in got["c"]]))
        de = max(abs(a - b) for a, b in zip(exp["edge"], got["edge"]))
        if de > TOL_LEN: d.append("下沿取样差 %.0f cm" % de)
        if abs(exp["s0"] - got["s0"]) > TOL_LEN or abs(min(exp["s1"], 1e7) - min(got["s1"], 1e7)) > TOL_LEN: d.append("踩踏面 灰盒 %.0f–%.0f UE %.0f–%.0f" % (exp["s0"], exp["s1"], got["s0"], got["s1"]))
    if exp["clean"] != got["clean"]: d.append("够不够长 灰盒 %s UE %s" % (exp["clean"], got["clean"]))
    if exp["walkable"] != got["walkable"]: d.append("能不能踩 灰盒 %s UE %s" % (exp["walkable"], got["walkable"]))
    return d

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    pc.get_hud().start_game(True)
    yield from wait(90)
    beams = {}
    for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor):
        gid = str(b.get_editor_property("greybox_id"))
        if gid and gid != "None": beams[gid] = b
    say("窗光 %d 束：%s" % (len(beams), sorted(beams)))

    def run_set(rows, label):
        per = {}
        for r in rows:
            for exp in r["beams"]:
                b = beams.get(exp["id"])
                if b is None: continue
                got = json.loads(b.debug_solve(r["H"]))
                diff = compare(r["H"], exp, got, label)
                stat["n"] += 1
                p = per.setdefault(exp["id"], [0, 0, []]); p[0] += 1
                if diff:
                    stat["bad"] += 1; p[1] += 1
                    if len(p[2]) < 6: p[2].append("H=%.2f：%s" % (r["H"], "；".join(diff)))
            yield
        say("── " + label)
        for gid in sorted(per):
            n, bad, ex = per[gid]
            say("%s %s：%d/%d 个时刻一致" % ("✔" if bad == 0 else "✘", gid, n - bad, n))
            for e in ex: say("     " + e)

    # 第一组：雾满、开场的光伸到头
    cmd(w, "Dysis.Mist 1 3150"); cmd(w, "Dysis.IsleGrow 1 1")
    yield from wait(3)
    yield from run_set(G["sets"][0]["rows"], G["sets"][0]["label"])
    # 第二组：没有雾
    cmd(w, "Dysis.Mist 0 -100")
    yield from wait(3)
    yield from run_set(G["sets"][1]["rows"], G["sets"][1]["label"])
    # 开场的光伸到一半
    say("── 开场的光伸出来")
    H_I = G["consts"]["H_I"]
    for g in G["grow"]:
        cmd(w, "Dysis.IsleGrow %d %s" % (1 if g["k"] > 0 else 0, g["k"]))
        yield from wait(2)
        got = json.loads(beams["isle"].debug_solve(H_I)); exp = g["isle"]
        ok = exp["valid"] == got["valid"] and exp["walkable"] == got["walkable"] and (not exp["valid"] or max(abs(a - b) for a, b in zip(exp["c"], got["c"])) <= TOL_LEN)
        stat["n"] += 1; stat["bad"] += 0 if ok else 1
        say("%s 进度 %.2f：灰盒照到 %s，UE %s，能踩 %s / %s" % ("✔" if ok else "✘", g["k"], [round(x) for x in exp.get("c", [])], [round(x) for x in got.get("c", [])], exp["walkable"], got["walkable"]))
    cmd(w, "Dysis.WorldRelease")
    say("合计 %d 项，%d 项不一致" % (stat["n"], stat["bad"]))

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
_bt = R(); _bt.h = unreal.register_slate_post_tick_callback(_bt.tick)
