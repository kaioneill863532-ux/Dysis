# 界面截图测试（UE 编辑器 Python）：开 PIE，依次截主界面、局内（提示条 + 三片碎片）、四个角色的对话框、设置页。
# 截图在 项目/Saved/Screenshots/WindowsEditor/，对应关系写在 项目/Saved/ui_shots.txt。跑完会删掉测试时产生的存档。
import unreal, os, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "ui_shots.txt"
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
        if new: say("%s = %s" % (tag, new[-1].replace("\\", "/"))); return
    say("%s = 没生成" % tag)
def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
    yield from wait(25)
    hud = pc.get_hud()
    say("HUD %s 主界面开着 %s" % (hud.get_class().get_name(), hud.is_menu_open()))
    yield from shoot(w, "menu")
    # 主界面上时间在走（开局傍晚 → 约 8 秒日落 → 夜 → 约 50 秒天亮 → 白天）：挑几个时刻各截一张
    t = 0.0
    for mark in (6, 14, 28, 47, 54, 75):
        while t < mark:
            t += unreal.GameplayStatics.get_world_delta_seconds(w); yield
        yield from shoot(w, "menu_plus_%ds" % mark)
    hud.start_game(True)
    yield from wait(120)
    say("开始后 主界面开着 %s" % hud.is_menu_open())
    # 局内：三片碎片 + 一条提示
    for i in range(3): hud.set_shard(i, True)
    hud.show_notification("金苹果被放到了托盘上。流淌在其中的金黄逐渐褪色，将它化为一轮明月，递交给等候在天上的塞勒涅。", 30.0)
    yield from wait(60)
    yield from shoot(w, "ingame_hint_long")
    hud.show_notification("水雾溋溋升起，一条光路出现在你眼前。", 30.0)
    yield from wait(30)
    yield from shoot(w, "ingame_hint_short")
    hud.show_notification(" ", 0.5)
    # 对话：四个说话人各一句，自动翻页，每换一句截一张
    # （Python 里不能给角色加组件：借控制台命令 Dysis.Dialogue 造一个对话组件，再换成这里的四句）
    unreal.SystemLibrary.execute_console_command(w, "Dysis.Dialogue")
    yield from wait(3)
    d = pawn.get_components_by_class(unreal.DysisDialogueComponent)[-1]
    d.set_editor_property("lines", [
        "赫利俄斯：还在那里。当我的马车驶近西边的海，最后一缕日光落在它的身上，它便是你的金苹果了。此后，天空就要由你交给塞勒涅。",
        "狄西斯：月亮会借着它的光升起来，跟着我一路往下走。等把它放进水亭的月托，光就到她手里了。",
        "伊莉丝：是狄西斯把靛色的光送到了你的眼睛里。",
        "塞勒涅：狄西斯？她的时辰还没到，倒先来唤醒我了，难怪我梦见了许多颜色。我的夜里通常只有银灰。"])
    d.set_editor_property("seconds_per_line", 2.2)
    d.set_editor_property("forced_h", -1.0)
    d.play()
    last = -9
    for _ in range(1500):
        yield
        cur = d.get_editor_property("current_line")
        if cur < 0: break
        if cur != last:
            last = cur
            yield from wait(35)
            yield from shoot(w, "dialogue_%d" % cur)
    yield from wait(40)
    hud.open_settings()
    yield from wait(30)
    yield from shoot(w, "settings")
    hud.close_settings()
    yield from wait(10)
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
        say("DONE")
if os.path.exists(OUT): os.remove(OUT)
_ui = R(); _ui.h = unreal.register_slate_post_tick_callback(_ui.tick)
