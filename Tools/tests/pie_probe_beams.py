# 查某几束光在某几个时刻的算法结果（UE 编辑器 Python，开 PIE 跑）。
# 要查什么写在 项目/Saved/probe_in.json：{"ask": [["c", 66], ["c", 68]], "cmds": ["Dysis.Mist 1 3150"]}
# 结果写在 项目/Saved/pie_probe_beams.txt（每行一条 JSON），最后一行 DONE。
import unreal, os, json, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_probe_beams.txt"
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
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    pc.get_hud().start_game(True)
    yield from wait(90)
    for c in ASK.get("cmds", ["Dysis.Mist 1 3150", "Dysis.IsleGrow 1 1"]): unreal.SystemLibrary.execute_console_command(w, c)
    yield from wait(5)
    beams = {str(b.get_editor_property("greybox_id")): b for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor)}
    for bid, H in ASK["ask"]:
        say("%s H=%s %s" % (bid, H, beams[bid].debug_solve(H) if bid in beams else "没有这束光"))
        yield
    unreal.SystemLibrary.execute_console_command(w, "Dysis.WorldRelease")

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
_pb = R(); _pb.h = unreal.register_slate_post_tick_callback(_pb.tick)
