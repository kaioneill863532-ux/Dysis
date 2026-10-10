# 低模外观的测试（UE 编辑器 Python，开 PIE 跑）：建筑部件换上了低模、原模型不画但碰撞还在；
# 控制台 Dysis.LowPoly 0|1 能当场切换；同一个机位各拍一张（原模型 / 低模）给人对比。
# 光路、能走的地方不受影响由别的测试（pie_beams 等）管。结果写在 项目/Saved/pie_lowpoly.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_lowpoly.txt"
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
    unreal.SystemLibrary.execute_console_command(w, "Shot")
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]

VIEWS = [   # 名字、人站哪（方位、半径、高）、镜头、看向、视角
    ("殿外_正面", (203.7, 5600.0, 0.0), [5200.0, 2300.0, 900.0], [0.0, 0.0, 1300.0], 60.0),
    ("殿外_柱廊近景", (203.7, 5600.0, 0.0), [2600.0, 1150.0, 420.0], [1500.0, 600.0, 700.0], 60.0),
    ("水庭_大厅", (300.0, 1330.0, 0.0), polar(300.0, 1250.0, 170.0), [0.0, 0.0, 500.0], 80.0),
    ("一层_走廊", (250.0, 1400.0, 600.0), polar(240.0, 1400.0, 760.0), polar(285.0, 1400.0, 720.0), 75.0),
    ("三层_抬头看圆眼", (250.0, 1400.0, 2300.0), polar(250.0, 1380.0, 2460.0), [0.0, 0.0, 2950.0], 80.0),
]

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
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    def skin(): return json.loads(str(director.describe_skin()))
    def cmd(s): unreal.SystemLibrary.execute_console_command(w, s)

    d = skin()
    check(d["pieces"] >= 80 and d["on"], "开局：%d 个建筑部件挂上了低模" % d["pieces"])
    # 第八轮起：外立面的 20 尊神像、屋顶的细桥换成了美术的机关模型，它们的低模不画（画的是新模型）
    swapped = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        if any(c.static_mesh and c.static_mesh.get_name().startswith("LP_") and not c.is_visible() for c in a.get_components_by_class(unreal.StaticMeshComponent)):
            swapped.append(next((str(t) for t in a.tags if str(t).startswith("SM_")), a.get_name()))
    odd = [t for t in swapped if not (t.startswith("SM_Facade_Statue_") or t == "SM_RoofBridge")]
    check(d["lowShown"] + len(swapped) == d["pieces"] and d["origShown"] == 0 and not odd and len(swapped) == 21,
          "画的是低模（%d 个；另有 %d 件换成了美术的机关模型%s），原模型都不画（还在画的 %d 个）" % (d["lowShown"], len(swapped), ("，不该换的：%s" % odd) if odd else "", d["origShown"]))
    solid0 = d["origSolid"]
    check(solid0 >= d["pieces"] * 0.6, "原模型的碰撞还在：%d / %d 个带碰撞（其余本来就不挡东西，比如外立面的装饰）" % (solid0, d["pieces"]))
    cmd("Dysis.LowPoly 0"); yield from wait(5); d = skin()
    check(d["lowShown"] == 0 and d["origShown"] == d["pieces"] and d["origSolid"] == solid0, "Dysis.LowPoly 0：切回原模型，碰撞没变")
    cmd("Dysis.LowPoly 1"); yield from wait(5); d = skin()
    check(d["lowShown"] == d["pieces"] and d["origShown"] == 0 and d["origSolid"] == solid0, "Dysis.LowPoly 1：切回低模，碰撞没变")

    # 人站的高度两种样子下一样（托着人的一直是原模型的碰撞）：水庭的地面、一层走廊各站一次
    for name, az, r, z in (("水庭的地面", 285.0, 1250.0, 0.0), ("一层走廊", 265.0, 1400.0, 600.0)):
        hs = []
        for mode in (0, 1):
            cmd("Dysis.LowPoly %d" % mode)
            cmd("Dysis.Go %.1f %.1f %.1f" % tuple(polar(az, r, z + 8.0)))
            yield from wait(50)
            hs.append(pawn.get_actor_location().z - 90.0)
        check(abs(hs[0] - hs[1]) < 0.5 and abs(hs[1] - z) < 3.0, "站在%s：原模型时脚高 %.1f、低模时 %.1f（应为 %.0f）" % (name, hs[0], hs[1], z))

    cmd("Dysis.Sluice 1")
    for name, stand, eye, at, fov in VIEWS:
        cmd("Dysis.Go %.1f %.1f %.1f" % tuple(polar(stand[0], stand[1], stand[2] + 8.0)))
        yield from wait(50)
        for mode, label in ((0, "原模型"), (1, "低模")):
            cmd("Dysis.LowPoly %d" % mode)
            director.debug_camera(unreal.Vector(*eye), unreal.Vector(*at), fov)
            yield from wait(70)
            yield from shoot(w, "%s_%s" % (name, label))
            director.debug_camera_off()
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
_lp = R(); _lp.h = unreal.register_slate_post_tick_callback(_lp.tick)
