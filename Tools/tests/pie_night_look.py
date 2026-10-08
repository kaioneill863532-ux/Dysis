# 夜晚观感的截图（UE 编辑器 Python，开 PIE 跑）：入夜以后站到几处，各拍一张，给人看画面用，不做判定。
# 结果写在 项目/Saved/pie_night_look.txt（每张截图的文件名），最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_night_look.txt"
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
# 名字、站的地方（方位、半径、高度）、镜头朝向（相对“朝殿心”转多少度）、俯仰
SPOTS = [("屋顶_刚入夜", 300.0, 1420.0, 3035.0, 0.0, -10.0),
         ("四层_天鹅旁", 250.0, 1400.0, 2300.0, 20.0, 4.0),
         ("三层_三相像旁", 300.0, 1400.0, 1450.0, -30.0, 6.0),
         ("二层_厚墙前", 152.0, 1400.0, 600.0, 60.0, 4.0),
         ("水庭_看水亭", 60.0, 1250.0, 0.0, 0.0, 2.0),
         ("殿外_岛上回望", None, None, None, 0.0, 0.0)]

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
    start = pawn.get_actor_location()
    unreal.SystemLibrary.execute_console_command(w, "Dysis.Sluice 1"); unreal.SystemLibrary.execute_console_command(w, "Dysis.Mist 1 3150")
    yield from shoot(w, "白天_岛上（对照）")
    director.debug_set_night(True)
    yield from wait(400)
    # 想对比几档夜里的曝光：把数写在 项目/Saved/night_bias.txt（一行，用逗号隔开），只拍“四层 / 二层 / 水庭”三处
    bias_file = SAVED + "night_bias.txt"
    if os.path.exists(bias_file):
        for b in [float(x) for x in open(bias_file).read().split(",") if x.strip()]:
            pawn.set_editor_property("night_exposure_bias", b)
            for name, az, r, z, turn, pitch in SPOTS[1:5:1]:
                if "三层" in name: continue
                p = polar(az, r, z)
                unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f night" % (p[0], p[1], p[2] + 8))
                pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=az + 180.0 + turn))
                yield from wait(120)
                yield from shoot(w, "曝光 %.1f %s" % (b, name))
        return
    for name, az, r, z, turn, pitch in SPOTS:
        if az is None:
            unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f night" % (start.x, start.y, start.z - 80))
            pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=4.0, yaw=math.degrees(math.atan2(-start.y, -start.x))))
        else:
            p = polar(az, r, z)
            unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f night" % (p[0], p[1], p[2] + 8))
            pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=az + 180.0 + turn))
        yield from wait(150)
        say("%s：%s" % (name, str(tc.describe_state())[:60]))
        yield from shoot(w, name)

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
_nl = R(); _nl.h = unreal.register_slate_post_tick_callback(_nl.tick)
