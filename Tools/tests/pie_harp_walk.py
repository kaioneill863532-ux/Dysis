# 实走“光阶”（UE 编辑器 Python，开 PIE 跑）：二层东边踩上第一束光，往上走，跳到第二束、第三束，
# 第三束越过四层的栏杆，下到四层（灰盒 L3）。结果写在 项目/Saved/pie_harp_walk.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_harp_walk.txt"
H_H0 = 32.5; F2 = 1450.0; F3 = 2300.0
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
def sub(a, b): return [a[i] - b[i] for i in range(3)]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def strip(info, p):
    """点 p 在这束光能踩的面上的坐标：a 横向 0–1，s 沿光多少厘米（解 p − b0 = a·E + s·L）。"""
    E = sub(info["b1"], info["b0"]); L = info["L"]; D = sub(p, info["b0"])
    ee, el, ll, ed, ld = dot(E, E), dot(E, L), dot(L, L), dot(E, D), dot(L, D); det = ee * ll - el * el
    return (ed * ll - ld * el) / det, (ld * ee - ed * el) / det, math.sqrt(ee)
def at(info, a, s): return [info["b0"][i] + (info["b1"][i] - info["b0"][i]) * a + info["L"][i] * s for i in range(3)]

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
    beams = {str(b.get_editor_property("greybox_id")): b for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor)}
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    def foot():
        l = pawn.get_actor_location(); return [l.x, l.y, l.z - hh]
    def state(): return str(tc.describe_state())
    def zone(): return state().split("zone=")[1].split(" ")[0]
    def H(): return tc.get_editor_property("h")
    def solve(bid): return json.loads(beams[bid].debug_solve(H()))
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    # 第一束光的光脚在二层东边：先把时刻摆到 32.5，看它落在哪，站过去
    tc.set_forced_time(H_H0)
    yield from wait(60)
    i1 = json.loads(beams["h1"].debug_solve(H_H0))
    check(i1["valid"] and i1["walkable"], "时刻 32.5：第一束光有、能踩（照亮 %.2f，能踩的一段 %.0f–%.0f cm）" % (i1["lit"], i1.get("s0", 0), i1.get("s1", 0)))
    if not i1["valid"]: return
    p = at(i1, 0.5, i1["s1"] - 120)
    cmd(w, "Dysis.Go %.1f %.1f %.1f" % (p[0], p[1], p[2] + 12))
    yield from wait(30)
    tc.clear_forced_time()
    yield from wait(40)
    check(zone() == "beam:h1", "踩上第一束光的下端：%s，脚 (%.0f, %.0f, %.0f)" % (state()[:40], *foot()))
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=25.0, yaw=math.degrees(math.atan2(-i1["L"][1], -i1["L"][0]))))
    yield from shoot(w, "二层_踩上第一束光")

    order = ["h1", "h2", "h3"]; cur = 0; zones = []; trail = []; jumps = []; last = None; still = 0; done = False; air = 0; shot = 0
    for i in range(9000):
        f = foot(); z = zone()
        if not zones or zones[-1] != z: zones.append(z)
        if z == "L3" and abs(f[2] - F3) < 8: done = True; break
        if z in ("L0", "L1", "L2", "out") and i > 60: break          # 掉下去了
        info = solve(order[cur]); L = info["L"] if info["valid"] else None
        if i % 60 == 0: trail.append("第 %d 束 %s：脚高 %.0f，时刻 %.2f，%s" % (cur + 1, z, f[2], H(), ("沿光 %.0f / %.0f–%.0f，横向 %.2f" % (strip(info, f)[1], info["s0"], info["s1"], strip(info, f)[0])) if L else "这束光现在没有"))
        grounded = "ground=1" in state() or mv.is_moving_on_ground()
        if not grounded:
            air += 1
            if jumps and jumps[-1].get("dir"): pawn.add_movement_input(unreal.Vector(jumps[-1]["dir"][0], jumps[-1]["dir"][1], 0), 1.0, False)
            yield; continue
        air = 0
        if z.startswith("beam:h") and z[5:] in order: cur = order.index(z[5:])
        if not L: yield; continue
        a, s, width = strip(info, f)
        hl = math.hypot(L[0], L[1]); up = [-L[0] / hl, -L[1] / hl]
        # 能不能跳到下一束：下一束在这个高度上有能踩的面、离得够近
        if cur < 2:
            nx = solve(order[cur + 1])
            if nx["valid"] and nx["walkable"]:
                na, ns, nw = strip(nx, f)
                tgt = at(nx, 0.5, ns)
                d = math.dist(f[:2], tgt[:2])
                if nx["s0"] + 80 < ns < nx["s1"] - 80 and abs(tgt[2] - f[2]) < 90 and f[2] > F2 + 250:
                    v = [(tgt[0] - f[0]) / d, (tgt[1] - f[1]) / d]
                    # 先走到靠那一边的边上，再跳
                    edge = a < 0.12 or a > 0.88
                    pawn.add_movement_input(unreal.Vector(v[0], v[1], 0), 1.0, False)
                    if edge or still > 10:
                        pawn.jump(); jumps.append({"from": order[cur], "at": [round(x) for x in f], "gap": round(d), "dir": v})
                        if shot < 2: shot += 1; yield from shoot(w, "从第 %d 束跳向第 %d 束" % (cur + 1, cur + 2))
                    still = still + 1 if last and math.dist(f, last) < 0.3 else 0
                    last = f; yield; continue
        # 第三束：到了四层楼面上方就往下走到楼面上
        if cur == 2 and f[2] > F3 + 20:
            r = math.hypot(f[0], f[1])
            if 1250 < r < 1500:
                out = [f[0] / r, f[1] / r]
                side = [-up[1], up[0]]
                for sgn in (1, -1):
                    pawn.add_movement_input(unreal.Vector(side[0] * sgn * (1 if a > 0.5 else -1), side[1] * sgn * (1 if a > 0.5 else -1), 0), 1.0, False); break
                last = f; yield; continue
        pawn.add_movement_input(unreal.Vector(up[0], up[1], 0), 1.0, False)
        still = still + 1 if last and math.dist(f, last) < 0.3 else 0
        last = f
        if still > 240: break
        yield
    yield from wait(30)
    f = foot()
    for t in trail[:24]: say("     " + t)
    for j in jumps[:6]: say("     跳：从 %s，脚 %s，离下一束中线 %d cm" % (j["from"], j["at"], j["gap"]))
    check(done and mv.get_editor_property("respawn_count") == 0, "沿光阶从二层上到四层：脚 (%.0f, %.0f, %.0f)，一路的区域 %s，回落脚点 %d 次，%s" % (f[0], f[1], f[2], zones, mv.get_editor_property("respawn_count"), state()[:30]))
    yield from shoot(w, "光阶走完_到四层")
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
_hw = R(); _hw.h = unreal.register_slate_post_tick_callback(_hw.tick)
