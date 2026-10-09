# 主界面延时摄影的截图（UE 编辑器 Python，开 PIE 跑）：不点“开始游戏”，每隔几秒拍一张，看一整个昼夜的样子。给人看画面用，不做判定。
# 结果写在 项目/Saved/pie_menu_look.txt（每张截图的文件名和当时的时刻），最后一行 DONE。
import unreal, os, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_menu_look.txt"
lines = []
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def shots(): return set(glob.glob(SAVED + "Screenshots/WindowsEditor/*.png"))
def shoot(w, tag):
    before = shots()
    unreal.SystemLibrary.execute_console_command(w, "Shot showui")
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)

STEP, TOTAL = 6.0, 140.0
def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisSkyActor)[0]
    t0 = unreal.GameplayStatics.get_real_time_seconds(w); nxt = 3.0
    while True:
        yield
        t = unreal.GameplayStatics.get_real_time_seconds(w) - t0
        if t >= nxt:
            nxt += STEP
            alt = sky.get_altitudes()
            yield from shoot(w, "%3.0f 秒 太阳高 %.0f° 月亮高 %.0f°" % (t, alt[0], alt[1]))
        if t > TOTAL: break

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
        say("DONE"); s.g = None; import gc; gc.collect()
if os.path.exists(OUT): os.remove(OUT)
_ml = R(); _ml.h = unreal.register_slate_post_tick_callback(_ml.tick)
