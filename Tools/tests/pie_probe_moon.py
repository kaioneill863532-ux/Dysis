# 查“夜里某一点朝月亮看过去被谁挡着”（UE 编辑器 Python，开 PIE 跑）。
# 要查什么写在 项目/Saved/probe_in.json：{"H": 142, "points": [[x, y, z], …], "wait": 1800}（wait = 入夜以后等多少帧再查，让屋顶的楼梯降到位）
# 结果写在 项目/Saved/pie_probe_moon.txt，最后一行 DONE。
import unreal, os, json, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_probe_moon.txt"
ASK = json.load(open(SAVED + "probe_in.json", encoding="utf-8"))
lines = []
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def wait(n):
    for _ in range(n): yield

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
    director.debug_set_night(True)
    yield from wait(int(ASK.get("wait", 1800)))
    H = ASK["H"]; tc.set_forced_time(H)
    yield from wait(10)
    moon = unreal.DysisSkyLibrary.dysis_moon_dir(H)
    ignore = list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor)) + [pawn]
    for p in ASK["points"]:
        s = unreal.Vector(p[0] + moon.x * 50, p[1] + moon.y * 50, p[2] + moon.z * 50)
        e = unreal.Vector(p[0] + moon.x * 40000, p[1] + moon.y * 40000, p[2] + moon.z * 40000)
        hit = unreal.SystemLibrary.line_trace_single(w, s, e, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, unreal.DrawDebugTrace.NONE, True)
        if not hit: say("%s：照得到" % p); continue
        t = hit.to_tuple(); a = t[9]; c = t[10]; q = t[4]
        say("%s：被挡在 %.0f cm 处，(%.0f, %.0f, %.0f)，%s / %s  [%s]" % (p, t[3] + 50, q.x, q.y, q.z, a.get_actor_label() if a else "?", c.get_name() if c else "?", a.get_folder_path() if a else ""))

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
_pm = R(); _pm.h = unreal.register_slate_post_tick_callback(_pm.tick)
