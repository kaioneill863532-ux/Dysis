# 开场那束光的实走测试（UE 编辑器 Python，开 PIE 跑）：
# 出生在岛上 → 往神殿迈一步 → 光 3 秒伸到岛上 → 沿光走上台地；中途往边上走不会掉下去（护栏）。
# 结果写在 项目/Saved/pie_isle_walk.txt，最后一行 DONE。
import unreal, os, math, glob, json, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_isle_walk.txt"
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
    yield from wait(120)
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    isle = [b for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor) if str(b.get_editor_property("greybox_id")) == "isle"][0]
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def state(): return str(tc.describe_state())
    def walk_to(tx, ty, frames, stop=60.0):
        for _ in range(frames):
            f = foot(); dx, dy = tx - f[0], ty - f[1]; d = math.hypot(dx, dy)
            if d < stop: return True
            pawn.add_movement_input(unreal.Vector(dx / d, dy / d, 0), 1.0, False)
            yield
        return False

    # 1 开局：光还没伸出来
    check("beam:isle" not in state() and "zone=out" in state(), "开局站在岛上：" + state()[:60])
    yield from shoot(w, "开局_光还没出来")
    # 2 往神殿迈一步（1 m）→ 光开始伸；3 秒后伸到岛上
    f0 = foot()
    yield from walk_to(f0[0] - 0.915 * 110, f0[1] - 0.402 * 110, 120, 8.0)
    yield from wait(60)
    yield from shoot(w, "光正在伸出来")
    yield from wait(170)
    info = json.loads(isle.debug_solve(22.369623899246214))
    check(info["valid"] and info["walkable"], "迈出一步约 4 秒后：光伸到岛上、能踩（照到 %s）" % [round(x) for x in info.get("c", [])])
    yield from shoot(w, "光伸到岛上")
    # 3 走到光脚，再沿光走上去
    yield from walk_to(4690, 2047, 400, 40.0)
    yield from wait(10)
    yield from walk_to(3800, 1646, 900, 60.0)      # 光的中段
    f = foot()
    check("beam:isle" in state() and -900 < f[2] < -500, "走到光的中段：脚高 %.0f cm，%s" % (f[2], state()[:50]))
    yield from shoot(w, "走在光上")
    # 4 护栏：站在光中段往一侧横着走 1.5 秒，不会掉下去
    z_before = foot()[2]
    for _ in range(90):
        pawn.add_movement_input(unreal.Vector(-0.411, 0.912, 0), 1.0, False); yield
    f = foot()
    check(abs(f[2] - z_before) < 60 and "beam:isle" in state(), "往光的边上走 1.5 秒：还在光上（高度变了 %.0f cm）" % (f[2] - z_before))
    # 5 继续走上台地
    yield from walk_to(2960, 1268, 1200, 50.0)
    yield from walk_to(2600, 1105, 400, 50.0)
    yield from wait(30)
    f = foot()
    check(-140 < f[2] < 20 and "zone=out" in state(), "走上台地、走到门廊前：脚高 %.0f cm（台地 −120，门廊 0），%s" % (f[2], state()[:50]))
    check(mv.get_editor_property("respawn_count") == 0, "一路没有掉下去回落脚点")
    yield from shoot(w, "走上台地")
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
_iw = R(); _iw.h = unreal.register_slate_post_tick_callback(_iw.tick)
