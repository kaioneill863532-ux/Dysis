# 试玩时“灰盒里不挡光、UE 里却挡了”的是谁（UE 编辑器 Python，开 PIE 跑）。
# 和 find_light_blockers.py 做的是同一件事，只是在试玩的世界里查——机关总管把部件挪好、藏好以后的样子
# （编辑器世界里夜里才出现的桥面之类都还摆着，会多报）。
# 做法：按灰盒标准答案里每束光的四个角和方向打“可见性”射线（不算光自己）；灰盒照得更远的，记下 UE 里先挡住它的部件。
# 结果写在 项目/Saved/pie_light_blockers.txt，最后一行 DONE。
import unreal, os, json, glob, collections, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_light_blockers.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_beams.json", encoding="utf-8"))
lines = []
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def wait(n):
    for _ in range(n): yield
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
    cmd(w, "Dysis.Mist 1 3150"); cmd(w, "Dysis.IsleGrow 1 1")
    yield from wait(3)
    ignore = list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor)) + [pawn]
    short = collections.Counter(); where = {}
    for r in G["sets"][0]["rows"]:
        for b in r["beams"]:
            if not b["valid"]: continue
            O, U, V, L = b["O"], b["U"], b["V"], b["L"]
            C = [O, [O[i] + U[i] for i in range(3)], [O[i] + V[i] for i in range(3)], [O[i] + U[i] + V[i] for i in range(3)]]
            for k in range(4):
                s = unreal.Vector(C[k][0] + L[0] * 5, C[k][1] + L[1] * 5, C[k][2] + L[2] * 5)
                e = unreal.Vector(s.x + L[0] * 12000, s.y + L[1] * 12000, s.z + L[2] * 12000)
                hit = unreal.SystemLibrary.line_trace_single(w, s, e, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, unreal.DrawDebugTrace.NONE, True)
                if not hit: continue
                t = hit.to_tuple(); dist = t[3] + 5; a = t[9]; c = t[10]
                if dist < b["c"][k] - 12:
                    key = (b["id"], "%s / %s  [%s]" % (a.get_actor_label() if a else "?", c.get_name() if c else "?", a.get_folder_path() if a else ""))
                    short[key] += 1
                    where.setdefault(key, "H=%.1f 第 %d 角：UE %.0f cm，灰盒 %.0f cm" % (r["H"], k, dist, b["c"][k]))
        yield
    say("== 沿光方向：UE 里比灰盒先被挡住，挡住的是（光 / 次数 / 部件 / 头一次出现） ==")
    for (bid, lab), n in sorted(short.items(), key=lambda x: -x[1])[:40]: say("  %-5s %4d  %s   （%s）" % (bid, n, lab, where[(bid, lab)]))
    if not short: say("  没有")
    cmd(w, "Dysis.WorldRelease")

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
_lb = R(); _lb.h = unreal.register_slate_post_tick_callback(_lb.tick)
