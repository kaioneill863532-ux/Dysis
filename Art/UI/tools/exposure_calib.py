# 曝光取样（UE 编辑器 Python）：主界面机位，白天 / 半夜各按几档固定曝光截图，再进游戏截室内；用来定“固定曝光”的数值。
# 对应关系写在 项目/Saved/exposure_calib.txt。
import unreal, os, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "exposure_calib.txt"
BIASES = [-4.0, -3.0, -2.0, -1.0, 0.0]
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
        if new: say("%s = %s" % (tag, os.path.basename(new[-1]))); return
    say("%s = 没生成" % tag)
def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    hud = pc.get_hud()
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisSkyActor)[0]
    sky.set_editor_property("moon_lux_per_greybox_unit", sky.get_editor_property("sun_lux_per_greybox_unit"))
    hud.set_editor_property("menu_day_seconds", 0.0)
    hud.set_editor_property("menu_time_h", 20.0)
    yield from wait(200)
    yield from shoot(w, "menu_day_auto")
    hud.set_editor_property("menu_time_h", 180.0)
    yield from wait(200)
    yield from shoot(w, "menu_night_auto")
    # 固定曝光写在当前镜头的后期设置里（Python 里没法在 PIE 世界生成后期体积）
    cam = {"c": unreal.GameplayStatics.get_all_actors_of_class(w, unreal.CameraActor)[0].get_component_by_class(unreal.CameraComponent)}
    def set_bias(b, on=True):
        s = cam["c"].get_editor_property("post_process_settings")
        s.set_editor_property("override_auto_exposure_method", on)
        s.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
        s.set_editor_property("override_auto_exposure_bias", on)
        s.set_editor_property("auto_exposure_bias", b)
        s.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", on)
        s.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
        cam["c"].set_editor_property("post_process_settings", s)
    for b in BIASES:
        hud.set_editor_property("menu_time_h", 20.0); set_bias(b)
        yield from wait(70)
        yield from shoot(w, "menu_day_bias_%+.0f" % b)
        hud.set_editor_property("menu_time_h", 180.0); set_bias(b + 1.0)
        yield from wait(70)
        yield from shoot(w, "menu_night_bias_%+.0f(+1)" % b)
    hud.start_game(True)
    yield from wait(150)
    cam["c"] = unreal.GameplayStatics.get_player_pawn(w, 0).get_component_by_class(unreal.CameraComponent)
    for b in BIASES:
        set_bias(b)
        yield from wait(50)
        yield from shoot(w, "indoor_bias_%+.0f" % b)
    set_bias(0.0, False)
    yield from wait(220)
    yield from shoot(w, "indoor_auto")
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
        say("DONE"); s.g = None; import gc; gc.collect()   # 放掉对 PIE 里那些对象的引用
if os.path.exists(OUT): os.remove(OUT)
_ex = R(); _ex.h = unreal.register_slate_post_tick_callback(_ex.tick)
