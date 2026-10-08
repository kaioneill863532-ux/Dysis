# 调夜晚亮度用（UE 编辑器 Python，开 PIE 跑）：入夜以后按几组参数各拍一轮，拿去和灰盒同样位置的画面比。
# 参数写在 项目/Saved/night_tune.json：[{"bias": -1.5, "fill": 0.59, "sky": 3.0, "star": 3.0, "apple": 1.0}, …]（缺的用现在的值）。
# 结果写在 项目/Saved/pie_night_tune.txt（每张截图的文件名），最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_night_tune.txt"
CFG = json.load(open(SAVED + "night_tune.json", encoding="utf-8")) if os.path.exists(SAVED + "night_tune.json") else [{}]
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
SPOTS = [("四层_天鹅旁", 250.0, 1400.0, 2300.0, 20.0, 4.0), ("三层_三相像旁", 300.0, 1400.0, 1450.0, -30.0, 6.0), ("水庭_看水亭", 60.0, 1250.0, 0.0, 0.0, 2.0)]

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
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisSkyActor)[0]
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    unreal.SystemLibrary.execute_console_command(w, "Dysis.Sluice 1"); unreal.SystemLibrary.execute_console_command(w, "Dysis.Mist 1 3150")
    director.debug_set_night(True)
    yield from wait(200)
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    for i, cfg in enumerate(CFG):
        if "bias" in cfg: pawn.set_editor_property("night_exposure_bias", cfg["bias"])
        if "fill" in cfg: sky.set_editor_property("night_fill_lux", cfg["fill"])
        if "sky" in cfg: sky.set_editor_property("night_sky_gain", cfg["sky"])
        if "star" in cfg: sky.set_editor_property("star_gain", cfg["star"])
        sky.refresh_sky()
        say("第 %d 组：%s" % (i, json.dumps(cfg)))
        if "custom" in cfg:
            # 自己定一处：{"az":…, "r":…, "z":…, "yaw":…, "pitch":…, "H":…（可以不写）}
            c = cfg["custom"]; p = polar(c["az"], c["r"], c["z"])
            unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f night" % (p[0], p[1], p[2] + 8))
            pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=c["pitch"], yaw=c["yaw"]))
            if "H" in c: tc.set_forced_time(c["H"])
            yield from wait(90)
            sky.refresh_sky()
            yield from wait(20)
            say("  自定：%s" % str(tc.describe_state())[:60])
            yield from shoot(w, "%d 自定" % i)
            tc.clear_forced_time()
            continue
        for name, az, r, z, turn, pitch in SPOTS:
            if "spots" in cfg and not any(s in name for s in cfg["spots"]): continue
            p = polar(az, r, z)
            unreal.SystemLibrary.execute_console_command(w, "Dysis.Go %.1f %.1f %.1f night" % (p[0], p[1], p[2] + 8))
            pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=az + 180.0 + turn))
            yield from wait(70)
            sky.refresh_sky()
            yield from wait(20)
            say("  %s：%s" % (name, str(tc.describe_state())[:60]))
            yield from shoot(w, "%d %s" % (i, name))

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
_nt = R(); _nt.h = unreal.register_slate_post_tick_callback(_nt.tick)
