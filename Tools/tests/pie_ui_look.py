# 界面细节的截图（UE 编辑器 Python，开 PIE 跑）：对话框里字的大小和位置、三片碎片的摆动、设置图标的明暗。给人看画面用，不做判定。
# 结果写在 项目/Saved/pie_ui_look.txt（每张截图的文件名），最后一行 DONE。
import unreal, os, glob, math, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_ui_look.txt"
lines = []
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
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
def polar(az, r, z): return (r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z)

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    hud = pc.get_hud()
    hud.start_game(True)
    # 开场的对话（不删开场导演）：第 4 句就是参考图里的那一句，第 6 句最长
    t0 = unreal.GameplayStatics.get_real_time_seconds(w)
    while unreal.GameplayStatics.get_real_time_seconds(w) - t0 < 15.0 and not pawn.in_dialogue(): yield
    say("开场对话开始：%s" % pawn.in_dialogue())
    for k in range(3): pawn.advance_pressed(); yield from wait(6)
    yield from wait(40)
    yield from shoot(w, "对话框_参考图那一句")
    for k in range(2): pawn.advance_pressed(); yield from wait(6)
    yield from wait(40)
    yield from shoot(w, "对话框_最长的一句")
    for k in range(6): pawn.advance_pressed(); yield from wait(4)
    yield from wait(30)
    unreal.SystemLibrary.execute_console_command(w, "Dysis.Sluice 1"); unreal.SystemLibrary.execute_console_command(w, "Dysis.Mist 1 3150")
    p = polar(250.0, 1400.0, 1450.0)
    unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f" % (p[0], p[1], p[2] + 8))
    pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=18.0, yaw=250.0 + 180.0))
    for i in range(3): hud.set_shard(i, True)
    yield from wait(200)   # 等关卡名淡掉
    # 三片碎片：隔一会儿拍一张，看摆到哪了
    t0 = unreal.GameplayStatics.get_real_time_seconds(w)
    for k in range(4):
        while unreal.GameplayStatics.get_real_time_seconds(w) - t0 < k * 1.3: yield
        yield from shoot(w, "碎片和设置图标_%d" % k)

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
_ui = R(); _ui.h = unreal.register_slate_post_tick_callback(_ui.tick)
