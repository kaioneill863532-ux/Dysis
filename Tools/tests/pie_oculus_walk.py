# 实走“圆眼光柱”（UE 编辑器 Python，开 PIE 跑）：从三层回廊踩上从屋顶光圈斜着落下来的光柱，一路走上去，
# 穿过光圈，上到屋顶的细桥。结果写在 项目/Saved/pie_oculus_walk.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_oculus_walk.txt"
C = json.load(open(PROJ + "Tools/greybox/golden/greybox_roof.json", encoding="utf-8"))["consts"]
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
    def zone(): return state().split("zone=")[1].split(" ")[0]
    def H(): return tc.get_editor_property("h")
    def roof(): return json.loads(director.describe_roof())
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
    # 站到三层回廊上、光柱落脚的那个方位
    a = math.radians(C["AZ_OC"])
    cmd(w, "Dysis.Go %.1f %.1f %.1f" % (1380 * math.cos(a), 1380 * math.sin(a), C["F3"] + 8))
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=20.0, yaw=C["AZ_OC"] + 180.0))
    yield from wait(240)
    info = json.loads(beams["oculus"].debug_solve(H()))
    check(zone() in ("L3", "beam:oculus") and abs(H() - C["H_OC"]) < 0.3 and info["valid"] and info["walkable"], "三层、圆眼的方位上：%s，光柱有、能踩（光圈开了 %.0f cm）" % (state()[:40], roof()["iris"]))
    yield from shoot(w, "三层_圆眼光柱落下来")
    if not (info["valid"] and info["walkable"]):
        say("     光柱：" + json.dumps({k: info[k] for k in info if k in ("valid", "lit", "clean", "walkable", "s0", "s1")})); return
    mid = [(info["b0"][i] + info["b1"][i]) / 2 for i in range(3)]; L = info["L"]; s1 = info["s1"]
    foot_pt = [mid[i] + L[i] * (s1 - 140) for i in range(3)]
    cmd(w, "Dysis.Go %.1f %.1f %.1f" % (foot_pt[0], foot_pt[1], foot_pt[2] + 12))
    yield from wait(40)
    check(zone() == "beam:oculus", "踩上光柱的下端：" + state()[:44])
    hl = math.hypot(L[0], L[1]); up = (-L[0] / hl, -L[1] / hl)
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=35.0, yaw=math.degrees(math.atan2(up[1], up[0]))))
    last = None; still = 0; top = False; zones = []; trail = []; shot = False
    for i in range(5000):
        f = foot(); z = zone()
        if not zones or zones[-1] != z: zones.append(z)
        if i % 45 == 0: trail.append("高 %.0f 区域 %s 时刻 %.2f 光圈 %.0f" % (f[2], z, H(), roof()["iris"]))
        if z in ("rbridge", "crown") and f[2] > C["RING_Z"] - 5: top = True; break
        if f[2] > 2700 and not shot: shot = True; yield from shoot(w, "光柱上_快到光圈")
        still = still + 1 if last and math.dist(f, last) < 0.3 else 0
        last = f
        if still > 120: break
        pawn.add_movement_input(unreal.Vector(up[0], up[1], 0), 1.0, False); yield
    yield from wait(30)
    f = foot()
    for t in trail[:14]: say("     " + t)
    check(top and mv.get_editor_property("respawn_count") == 0, "顺着光柱一路走上去，穿过光圈，上到屋顶：脚 (%.0f, %.0f, %.0f)，一路的区域 %s，%s" % (f[0], f[1], f[2], zones, state()[:30]))
    yield from shoot(w, "穿过光圈_上到屋顶细桥")
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
_ow = R(); _ow.h = unreal.register_slate_post_tick_callback(_ow.tick)
