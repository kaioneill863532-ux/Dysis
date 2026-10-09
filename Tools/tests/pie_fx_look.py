# “光和水”效果的截图（UE 编辑器 Python，开 PIE 跑）：光柱、水沫里的虹、虹桥、浮雕上的虹、殿顶的虹门、月虹、月桥、影桥、瀑布、海、水庭的水。
# 给人看画面用，不做判定。想只拍其中几段：把段名写在 Saved/fx_look_only.txt（逗号隔开：beams,water,rainbow,roof,night）。
# 结果写在 项目/Saved/pie_fx_look.txt（每张截图的文件名），最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_fx_look.txt"
RB = json.load(open(PROJ + "Tools/greybox/golden/greybox_rainbow.json", encoding="utf-8"))
N3 = json.load(open(PROJ + "Tools/greybox/golden/greybox_night3.json", encoding="utf-8"))
PTS = json.load(open(PROJ + "Tools/greybox/golden/greybox_night2.json", encoding="utf-8"))["consts"]["bridge"]["pts"]
C = RB["consts"]; BR = C["BRIDGE"]
ONLY = [x.strip() for x in open(SAVED + "fx_look_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "fx_look_only.txt") else []
def on(name): return not ONLY or name in ONLY
lines = []
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def shots(): return set(glob.glob(SAVED + "Screenshots/WindowsEditor/*.png"))
def wait(n):
    for _ in range(n): yield
def shoot(w, tag):
    before = shots()
    unreal.SystemLibrary.execute_console_command(w, "Shot")   # 只拍游戏画面，不受编辑器窗口大小影响
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)
def cmd(w, s): unreal.SystemLibrary.execute_console_command(w, s)
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]
def add(a, b, k=1.0): return [a[0] + b[0] * k, a[1] + b[1] * k, a[2] + b[2] * k]

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
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    night = [False]
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def fin(): return json.loads(director.describe_finale())
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def look_at(p):
        f = foot(); d = (p[0] - f[0], p[1] - f[1], p[2] - (f[2] + 150.0))
        look(math.degrees(math.atan2(d[1], d[0])), math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f%s" % (p[0], p[1], p[2] + dz, " night" if night[0] else ""))
    def seconds(t):
        t0 = now()
        while now() - t0 < t: yield
    def spot(tag, p, yaw=None, pitch=0.0, at=None, frames=110, dz=6.0):
        # 人站到 p、用人自己的镜头拍（玩的时候看到的样子）
        go(p, dz)
        if at is not None: look_at(at)
        else: look(yaw, pitch)
        yield from wait(frames)
        yield from shoot(w, tag)
    def view(tag, eye, at, frames=70, fov=75.0):
        # 另放一台镜头在 eye、看向 at 拍一张（人不动：时刻还是按人站的地方算）
        director.debug_camera(unreal.Vector(*eye), unreal.Vector(*at), fov)
        yield from wait(frames)
        yield from shoot(w, tag)
        director.debug_camera_off()
    def bow_eyes(target, dist, pitch):
        # 水沫里的虹只在“视线离背对太阳的方向 42°”的地方看得见：给一个要看的点，算出两个能看见虹的机位（一左一右）
        s = unreal.DysisSkyLibrary.dysis_sun_dir(tc.get_editor_property("h")); L = (-s.x, -s.y, -s.z)
        want = math.cos(math.radians(41.5)); out = []; prev = None
        for i in range(721):
            yaw = math.radians(i * 0.5)
            d = (math.cos(math.radians(pitch)) * math.cos(yaw), math.cos(math.radians(pitch)) * math.sin(yaw), math.sin(math.radians(pitch)))
            v = d[0] * L[0] + d[1] * L[1] + d[2] * L[2] - want
            if prev is not None and (prev < 0) != (v < 0): out.append([target[0] - d[0] * dist, target[1] - d[1] * dist, target[2] - d[2] * dist])
            prev = v
        return out

    yield from spot("岛上_开场的光和海", foot(), yaw=math.degrees(math.atan2(-foot()[1], -foot()[0])), pitch=4.0, dz=0.0)
    cmd(w, "Dysis.Sluice 1")
    yield from seconds(2.5)                                     # 等水放下来（下面那句会把水量、雾都定住）
    cmd(w, "Dysis.Mist 1 3150")
    if on("beams"):
        go(polar(250.0, 1400.0, 600.0)); yield from wait(60)
        yield from view("一层_第一束光（b1）", polar(215.0, 1380.0, 760.0), polar(95.0, 900.0, 250.0))
        yield from view("水庭_抬头看光（迎着光）", polar(330.0, 1180.0, 60.0), polar(120.0, 700.0, 700.0), fov=85.0)
        yield from view("水庭_背着光看", polar(100.0, 1100.0, 420.0), polar(300.0, 600.0, 200.0), fov=85.0)
        go(polar(20.0, 1400.0, 1450.0)); yield from wait(60)
        yield from view("二层_竖琴光", polar(350.0, 1380.0, 1610.0), polar(200.0, 900.0, 1750.0), fov=80.0)
        go(polar(250.0, 1400.0, 2300.0)); yield from wait(60)
        yield from view("三层_圆眼光柱", polar(250.0, 1380.0, 2460.0), [0.0, 0.0, 2650.0], fov=80.0)
        go(polar(100.0, 1300.0, 0.0), 8.0); yield from wait(60)
        yield from view("水庭_贴着光看（人在光里）", polar(60.0, 1150.0, 150.0), polar(100.0, 1300.0, 130.0), fov=70.0)
    if on("water"):
        go(polar(170.0, 1330.0, 0.0), 8.0); yield from wait(60)
        yield from view("水庭_瀑布", polar(200.0, 1180.0, 170.0), polar(128.0, 1245.0, 420.0), fov=80.0)
        yield from view("水庭_瀑布脚下", polar(165.0, 1250.0, 120.0), polar(128.0, 1245.0, 60.0), fov=70.0)
        yield from view("二层_看瀑布全身", polar(300.0, 1300.0, 760.0), polar(128.0, 1245.0, 1100.0), fov=85.0)
        yield from view("水庭_水面和水亭", polar(300.0, 1250.0, 130.0), [0.0, 0.0, 60.0], fov=75.0)
        yield from view("水庭_水边", polar(300.0, 1100.0, 110.0), polar(285.0, 1000.0, -45.0), fov=70.0)
        yield from view("一层_俯看水面", polar(300.0, 1300.0, 760.0), [0.0, 0.0, -45.0], fov=75.0)
        yield from view("殿外_海", [3500.0, 5200.0, 600.0], [0.0, 0.0, 1200.0], fov=75.0)
        yield from view("殿外_贴着海面", [2600.0, 4200.0, -1450.0], [-3000.0, 20000.0, -1600.0], fov=70.0)
        # 第一束光的光脚上、瀑布前面：水沫里的虹（背对太阳才看得见）
        for k, e in enumerate(bow_eyes(polar(104.0, 1080.0, 120.0), 620.0, -6.0)[:2]):
            yield from view("水庭_光脚上的虹_%d" % (k + 1), e, polar(104.0, 1080.0, 120.0), fov=70.0)
        for k, e in enumerate(bow_eyes(polar(128.0, 1170.0, 260.0), 560.0, -4.0)[:2]):
            yield from view("水庭_瀑布水沫里的虹_%d" % (k + 1), e, polar(128.0, 1170.0, 260.0), fov=75.0)
    if on("rainbow"):
        I = C["IRISREL"]; E = I["E"]; ax = BR["ax"] + [0.0] if len(BR["ax"]) == 2 else BR["ax"]; A = BR["A"]; B = BR["B"]; sd = BR["side"]
        mid_rel = [I["face"][0] + I["n"][0] * 45.0, I["face"][1] + I["n"][1] * 45.0, 740.0]
        # 还没对上：站在离对的位置差一点的地方，水帘里已经有一段虹了（它是找位置的线索）
        go([E[0] - I["T"][0] * 90.0 + I["n"][0] * 60.0, E[1] - I["T"][1] * 90.0 + I["n"][1] * 60.0, C["F1"]])
        look_at(mid_rel)
        yield from wait(50)
        yield from shoot(w, "浮雕_没对上时水帘里的虹（人的镜头）")
        for k, e in enumerate(bow_eyes(mid_rel, 300.0, 2.0)[:2]):
            yield from view("浮雕_水帘里的虹_机位%d" % (k + 1), e, mid_rel, fov=70.0)
        go([E[0], E[1], C["F1"]]); look_at(mid_rel)
        for _ in range(600):
            yield
            if json.loads(director.describe_rainbow())["done"]: break
        yield from wait(40)
        yield from shoot(w, "浮雕_对上了（人的镜头）")
        yield from view("浮雕上的虹_对上了", [E[0] - 40.0, E[1] - 230.0, 760.0], [302.0, 1512.0, 740.0], fov=60.0)
        for _ in range(900):
            yield
            if json.loads(director.describe_rainbow())["bridgeOn"]: break
        yield from wait(60)
        mid = [A[0] + ax[0] * BR["len"] * 0.5, A[1] + ax[1] * BR["len"] * 0.5, 1235.0]
        yield from view("虹桥_桥头", [A[0] - ax[0] * 330.0 + sd[0] * 150.0, A[1] - ax[1] * 330.0 + sd[1] * 150.0, 780.0], [A[0] + ax[0] * 260.0, A[1] + ax[1] * 260.0, 720.0])
        yield from view("虹桥_桥头落地（近）", [A[0] - ax[0] * 150.0 - sd[0] * 190.0, A[1] - ax[1] * 150.0 - sd[1] * 190.0, 700.0], [A[0] + ax[0] * 60.0, A[1] + ax[1] * 60.0, 615.0], fov=70.0)
        yield from view("虹桥_整座", polar(20.0, 1200.0, 900.0), mid, fov=80.0)
        yield from view("虹桥_从下面看", polar(250.0, 900.0, 120.0), mid, fov=80.0)
        yield from view("虹桥_桥尾接石沿", [B[0] - ax[0] * 380.0 - sd[0] * 170.0, B[1] - ax[1] * 380.0 - sd[1] * 170.0, 1750.0], [B[0] + ax[0] * 40.0, B[1] + ax[1] * 40.0, 1740.0])
        yield from view("虹桥_桥尾（从上往下）", [B[0] - ax[0] * 260.0 + sd[0] * 60.0, B[1] - ax[1] * 260.0 + sd[1] * 60.0, 1960.0], [B[0] - ax[0] * 30.0, B[1] - ax[1] * 30.0, 1745.0], fov=70.0)
        go([A[0] + ax[0] * BR["len"] * 0.45, A[1] + ax[1] * BR["len"] * 0.45, C["F1"] + 1150 * 0.45 + 125.0], 12.0)
        yield from wait(40)
        on_br = foot()
        yield from view("虹桥_走在桥上", [on_br[0] - ax[0] * 420.0 + sd[0] * 60.0, on_br[1] - ax[1] * 420.0 + sd[1] * 60.0, on_br[2] + 260.0], [on_br[0] + ax[0] * 300.0, on_br[1] + ax[1] * 300.0, on_br[2] + 150.0])
    if on("roof"):
        # 殿顶：走到最高一级（接光台），回头看虹门——喷泉水雾里的虹正好填在拱里
        alt = polar(27.83313, 1320.0, 3030.0)
        go(alt, 10.0); yield from wait(12)
        go(add(alt, [0.0, 0.0, 720.0]), 12.0); yield from wait(80)
        arch = [1246.343, 766.807, 3750.0 + 155.4]
        look_at(arch)
        yield from wait(40)
        yield from shoot(w, "殿顶_接光台上看虹门（人的镜头）")
        f = foot()
        yield from view("殿顶_虹门里的虹（眼睛的位置）", [f[0], f[1], f[2] + 162.0], arch, fov=80.0)
        go(add(polar(15.0, 1320.0, 3750.0), [0.0, 0.0, -90.0]), 12.0); yield from wait(60)
        for k, e in enumerate(bow_eyes(arch, 420.0, -3.0)[:2]):
            yield from view("殿顶_台阶上看虹门_%d" % (k + 1), e, arch, fov=80.0)
    if on("night"):
        fl = {f["step"]: f for f in N3["flow"]}; G3 = N3["consts"]
        cmd(w, "Dysis.WorldRelease"); cmd(w, "Dysis.Sluice 1")
        go(polar(250.0, 1400.0, 600.0))
        yield from wait(20)
        director.debug_set_night(True); night[0] = True
        director.debug_set_mirror_slot(1)                      # 三相像转到把月光反到瀑布上的那一格
        for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.stop_music(0.0)
        yield from seconds(10.5)
        director.debug_show_moon_bridge()
        yield from seconds(0.9)
        yield from view("月桥_正在显形", polar(330.0, 1180.0, 900.0), [450.0, -100.0, 330.0], fov=85.0)
        yield from seconds(2.5)
        m0 = [PTS[0][0], PTS[0][1], PTS[0][2]]; m1 = [PTS[-1][0], PTS[-1][1], PTS[-1][2]]
        yield from view("月桥_二层桥头", [-560.0, -1330.0, 800.0], [150.0, -980.0, 520.0], fov=80.0)
        yield from view("月桥_桥头接走廊（近）", add(m0, [-190.0, -60.0, 140.0]), add(m0, [40.0, 20.0, 0.0]), fov=70.0)
        yield from view("月桥_整座", polar(330.0, 1180.0, 900.0), [450.0, -100.0, 330.0], fov=85.0)
        yield from view("月桥_落地那一头", polar(75.0, 1200.0, 160.0), m1, fov=70.0)
        yield from view("月桥_落地（近）", add(m1, [120.0, 190.0, 130.0]), add(m1, [-20.0, -40.0, 5.0]), fov=70.0)
        director.debug_set_selene_talked(True); tc.set_forced_time(110.0)
        yield from seconds(5.0)
        yield from view("月虹", polar(300.0, 1250.0, 420.0), polar(128.0, 1040.0, 300.0), fov=75.0)
        yield from view("月虹_从水庭看", polar(310.0, 900.0, 130.0), polar(128.0, 1040.0, 330.0), fov=75.0)
        tc.clear_forced_time()
        go(polar(300.0, 1330.0, 0.0), 8.0); yield from wait(60)
        yield from view("夜_水面和水亭", polar(300.0, 1250.0, 130.0), [0.0, 0.0, 60.0], fov=75.0)
        yield from view("夜_瀑布", polar(200.0, 1180.0, 170.0), polar(128.0, 1245.0, 420.0), fov=80.0)
        yield from view("夜_殿外的海", [3500.0, 5200.0, 600.0], [0.0, 0.0, 1200.0], fov=75.0)
        # 影桥：在月桥上对的那一步站住 → 女神亮、半桥伸出来 → 站上半桥，细桥的月影接上
        go(PTS[fl["stand"]["i"]], 10.0); look(100.0, -6.0)
        for _ in range(900):
            yield
            if fin()["goddess"]: break
        yield from view("夜_月光照透水帘（看得见后面的女神）", polar(152.0, 980.0, 330.0), polar(134.0, 1250.0, 470.0), fov=70.0)
        yield from view("夜_月光照透水帘（远一点）", polar(200.0, 700.0, 260.0), polar(132.0, 1250.0, 480.0), fov=75.0)
        for _ in range(900):
            yield
            if fin()["halfExt"] >= 1: break
        p = fl["poolEdge"]; go(p["foot"]); look(G3["GOD"]["halfAz"] + 180.0, -8.0)
        yield from wait(60)
        half_end = polar(G3["GOD"]["halfAz"], 620.0, 20.0)
        yield from view("影桥_还没接上（一片暗影）", polar(G3["GOD"]["halfAz"] + 22.0, 1150.0, 330.0), polar(G3["GOD"]["halfAz"], 480.0, 20.0), fov=70.0)
        go(fl["onHalfBridge"]["foot"], 10.0)
        for _ in range(600):
            yield
            if fin()["shadowOn"] > 0.99: break
        say("影桥：%s | %s" % (str(tc.describe_state())[:70], director.describe_finale()))
        yield from wait(50)
        yield from shoot(w, "影桥_接上了（人的镜头）")
        yield from view("影桥_整座", polar(G3["GOD"]["halfAz"] + 22.0, 1150.0, 330.0), polar(G3["GOD"]["halfAz"], 480.0, 20.0), fov=70.0)
        yield from view("影桥_接半桥那一头（近）", add(half_end, [140.0, -200.0, 170.0]), half_end, fov=70.0)
        yield from view("影桥_接水亭那一头（近）", polar(G3["GOD"]["halfAz"] + 38.0, 520.0, 190.0), polar(G3["GOD"]["halfAz"], 300.0, 20.0), fov=70.0)

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
_fx = R(); _fx.h = unreal.register_slate_post_tick_callback(_fx.tick)
