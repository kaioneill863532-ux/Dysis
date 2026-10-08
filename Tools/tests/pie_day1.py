# 日间机关第一段的测试（UE 编辑器 Python，开 PIE 跑）：水闸和雾、合并后的 L1 机关和两块推拉石板、沿日1 的光走上二层。
# 标准答案：Tools/greybox/golden/greybox_beams.json（开局状态）、greybox_beams_slider1.json（机关拉下以后）。
# 结果写在 项目/Saved/pie_day1.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_day1.txt"
G0 = json.load(open(PROJ + "Tools/greybox/golden/greybox_beams.json", encoding="utf-8"))
G1 = json.load(open(PROJ + "Tools/greybox/golden/greybox_beams_slider1.json", encoding="utf-8"))
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
def same(exp, got):
    if exp["valid"] != got["valid"] or exp["clean"] != got["clean"] or exp["walkable"] != got["walkable"]: return False
    if abs(exp["lit"] - got["lit"]) > 0.0251: return False
    if exp["valid"] and abs(exp["lit"] - got["lit"]) < 1e-6:
        if max(abs(a - b) for a, b in zip(exp["c"], got["c"])) > 12: return False
        if abs(exp["s0"] - got["s0"]) > 12 or abs(min(exp["s1"], 1e7) - min(got["s1"], 1e7)) > 12: return False
    return True

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
    def piece(tag):
        for a in unreal.GameplayStatics.get_all_actors_with_tag(w, tag): return a
        return None
    def hidden(a):
        try: return bool(a.get_editor_property("hidden"))
        except Exception: return not a.static_mesh_component.is_visible()
    def az_of(a):
        l = a.get_actor_location(); return math.degrees(math.atan2(l.y, l.x)) % 360

    # ── A 开局：西窗关、南窗开 ──
    cmd(w, "Dysis.Mist 1 3150"); cmd(w, "Dysis.IsleGrow 1 1")
    yield from wait(3)
    n = ok = 0; notes = []
    for r in G0["sets"][0]["rows"]:
        for exp in r["beams"]:
            if exp["id"] not in ("b2", "iris"): continue
            got = json.loads(beams[exp["id"]].debug_solve(r["H"]))
            # iris 这束不是用来踩的：只比“有没有光、照亮多少”（它照到浮雕上，浮雕的模型比灰盒的厚一点，落点会差几厘米）
            good = same(exp, got) if exp["id"] == "b2" else (exp["valid"] == got["valid"] and abs(exp["lit"] - got["lit"]) <= 0.0251)
            n += 1; ok += 1 if good else 0
            if not good and len(notes) < 5: notes.append("%s H=%.1f 灰盒 valid=%s lit=%.3f / UE valid=%s lit=%.3f" % (exp["id"], r["H"], exp["valid"], exp["lit"], got["valid"], got["lit"]))
        yield
    check(ok == n, "开局（西窗关、南窗开）：b2、iris 两束光 %d/%d 个时刻和灰盒一致" % (ok, n))
    for x in notes: say("     " + x)

    # ── B 拉机关：1.6 秒后西窗开、南窗关 ──
    check(director.debug_interact("lever"), "拉动机关")
    yield from wait(20)
    yield from shoot(w, "机关提示_石板滑动")
    for _ in range(600):
        if director.get_editor_property("slider_pos") >= 0.999: break
        yield
    a_b2, a_iris = az_of(piece("SM_Mech_Slider_Panel_b2")), az_of(piece("SM_Mech_Slider_Panel_iris"))
    check(abs(a_b2 - 254.27) < 0.05 and abs(a_iris - 171.43) < 0.05, "石板滑到位：西边那块到方位 %.2f°（应 254.27），南边那块到 %.2f°（应 171.43）" % (a_b2, a_iris))
    yield from wait(20)
    n = ok = 0
    for r in G1["rows"]:
        for exp in r["beams"]:
            n += 1; ok += 1 if same(exp, json.loads(beams[exp["id"]].debug_solve(r["H"]))) else 0
        yield
    check(ok == n, "机关拉下以后：b2、iris 两束光 %d/%d 个时刻和灰盒一致" % (ok, n))
    # 再拉一次：复原
    director.debug_interact("lever")
    for _ in range(600):
        if director.get_editor_property("slider_pos") <= 0.001: break
        yield
    yield from wait(20)
    got = json.loads(beams["iris"].debug_solve(31.0))
    check(got["valid"] and director.get_editor_property("lever_pulls") == 2, "再拉一次复原：南窗又开了（iris 有光）")

    # ── C 水闸：开了以后雾升起来，日1 的光才能踩 ──
    cmd(w, "Dysis.Mist 0 -100"); yield from wait(3); cmd(w, "Dysis.WorldRelease")
    yield from wait(5)
    fall = piece("SM_Mech_Water_Waterfall")
    got = json.loads(beams["b1"].debug_solve(24.0))
    check(got["valid"] and not got["walkable"] and hidden(fall), "水闸没开：日1 的光有，但不能踩；看不见瀑布")
    check(director.debug_interact("sluice"), "打开水闸")
    yield from wait(30)
    yield from shoot(w, "水闸提示")
    t0 = unreal.GameplayStatics.get_time_seconds(w); t_ok = None
    for _ in range(3000):
        yield
        if json.loads(beams["b1"].debug_solve(24.0))["walkable"]:
            t_ok = unreal.GameplayStatics.get_time_seconds(w) - t0; break
    check(t_ok is not None and 7.0 < t_ok < 11.0 and not hidden(fall), "开闸后 %.1f 秒日1 的光能踩了（灰盒：雾浓到 0.85 要 8.5 秒）；瀑布出现了" % (t_ok or -1))

    # ── D 走到机关旁边：界面上出现“E　拉动机关” ──
    cmd(w, "Dysis.Go %.1f %.1f 600" % (1430 * math.cos(math.radians(328.5)), 1430 * math.sin(math.radians(328.5))))
    yield from wait(60)
    yield from shoot(w, "走到机关旁边")

    # ── E 沿日1 的光从水庭走上二层 ──
    cmd(w, "Dysis.Go %.1f %.1f 0" % (1151 * math.cos(math.radians(92.0)), 1151 * math.sin(math.radians(92.0))))
    yield from wait(60)
    deck = piece("SM_Mech_MoonBridge_Deck")
    check(("zone=L0" in state() or "zone=beam:b1" in state()) and "main=sun" in state() and (deck is None or hidden(deck)), "站在水庭东边的光脚旁：白天这里没有月桥，时间是下午（%s）" % state()[:44])
    info = json.loads(beams["b1"].debug_solve(24.0))
    mid = [(info["b0"][i] + info["b1"][i]) / 2 for i in range(3)]
    for _ in range(1500):
        f = foot()
        if f[2] > 760: break
        dx, dy = mid[0] - f[0], mid[1] - f[1]; d = math.hypot(dx, dy)
        pawn.add_movement_input(unreal.Vector(dx / d, dy / d, 0), 1.0, False)
        yield
    f = foot()
    check(f[2] > 760 and "beam:b1" in state(), "沿光往上走到二层上方：脚高 %.0f cm，%s" % (f[2], state()[:44]))
    yield from shoot(w, "走在日1的光上")
    # 往旁边一步，落到二层回廊上
    L = info["L"]; side = unreal.Vector(-L[1], L[0], 0)
    for _ in range(240):
        if "zone=L1" in state(): break
        pawn.add_movement_input(side, 1.0, False); yield
    yield from wait(40)
    f = foot()
    check("zone=L1" in state() and abs(f[2] - 600) < 5 and mv.get_editor_property("respawn_count") == 0, "从光上走下来落在二层：脚高 %.0f cm，%s" % (f[2], state()[:40]))
    yield from shoot(w, "到了二层")
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
_d1 = R(); _d1.h = unreal.register_slate_post_tick_callback(_d1.tick)
