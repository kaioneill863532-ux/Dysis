# 2026-10-08 这一轮反馈的测试和截图（UE 编辑器 Python，开 PIE 跑）：
#   · 没开水闸（没有水雾）时窗光不显出来，开了才有；
#   · 虹桥长出来时人正站在桥头的路上：不会被卡住，走得开；
#   · 入夜以后音乐换成夜里的那一首；
#   · 月之龛透开以后石台和碎片看得见；
#   · 月虹：伊莉丝和塞勒涅的话说完了、月亮不高不低时才有；
#   · 结局（集齐三片碎片）：镜头抬起来，星连成双子座和天鹅座；
#   · 一路拍照：E 键提示、叶片半开、夜里各处、星空。
# 结果写在 项目/Saved/pie_feedback.txt，最后一行 DONE。想只跑其中几段：把段名写在 Saved/feedback_only.txt（逗号隔开：prompt,mist,iris,bridge,night,shrine,moonbow,ending）。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_feedback.txt"
RB = json.load(open(PROJ + "Tools/greybox/golden/greybox_rainbow.json", encoding="utf-8"))
N2G = json.load(open(PROJ + "Tools/greybox/golden/greybox_night2.json", encoding="utf-8")); N2 = N2G["consts"]
C = RB["consts"]; BR = C["BRIDGE"]
ONLY = [x.strip() for x in open(SAVED + "feedback_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "feedback_only.txt") else []
def on(name): return not ONLY or name in ONLY
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
def cmd(w, s): unreal.SystemLibrary.execute_console_command(w, s)
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    music = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager)
    pc.get_hud().start_game(True)
    yield from wait(90)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    night = [False]
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def look_at(p):
        f = foot(); d = (p[0] - f[0], p[1] - f[1], p[2] - (f[2] + 150.0))
        look(math.degrees(math.atan2(d[1], d[0])), math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))))
    def go(p, dz=6.0): cmd(w, "Dysis.Go %.2f %.2f %.2f%s" % (p[0], p[1], p[2] + dz, " night" if night[0] else ""))
    def sky(): return json.loads(director.describe_sky_fx())
    def seconds(t):
        t0 = now()
        while now() - t0 < t: yield
    def beams_shown():
        n = 0
        for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor):
            for c in b.get_components_by_class(unreal.PrimitiveComponent):
                if "Mesh" in c.get_class().get_name() and c.is_visible() and "Visual" in c.get_name(): n += 1
        return n

    # ── E 键提示 ──
    if on("prompt"):
        go(polar(328.5, 1400.0, 600.0)); look(328.5, 0.0)
        yield from wait(60)
        yield from shoot(w, "提示_一层的机关旁")
        go([-425.0, 964.0, 90.0]); look_at([-469.73, 1065.01, 120.0])
        yield from wait(60)
        yield from shoot(w, "提示_水闸旁")

    # ── 没有水雾就没有光路 ──
    if on("mist"):
        go(polar(250.0, 1400.0, 600.0)); look(250.0 + 180.0 - 25.0, 8.0)
        yield from wait(120)
        dry = beams_shown()
        yield from shoot(w, "一层_没开水闸")
        cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")
        yield from wait(90)
        wet = beams_shown()
        check(dry <= 1 and wet >= 3, "没开水闸时显出来的光 %d 道（只该有开场岛上那一道），开了水闸、水雾起来以后 %d 道" % (dry, wet))
        yield from shoot(w, "一层_开了水闸")
    cmd(w, "Dysis.Sluice 1"); cmd(w, "Dysis.Mist 1 3150")

    # ── 叶片半开 ──
    if on("iris"):
        for a in (700.0, 350.0):
            cmd(w, "Dysis.Iris %.0f" % a)
            go(polar(300.0, 1420.0, 3035.0)); look(300.0 + 180.0, -14.0)
            yield from wait(90)
            yield from shoot(w, "叶片_光圈 %.0f_屋顶上看" % a)
            go(polar(250.0, 1400.0, 2300.0)); look(250.0 + 180.0, 38.0)
            yield from wait(90)
            yield from shoot(w, "叶片_光圈 %.0f_三层往上看" % a)
        r = json.loads(director.describe_roof())
        check(True, "叶片：屋顶现在 %s" % str({k: r[k] for k in r if k in ("iris", "irisA", "open")})[:80])

    # ── 虹桥长出来时人在桥头的路上 ──
    if on("bridge"):
        E = C["IRISREL"]["E"]; ax = BR["ax"]; A = BR["A"]
        go([E[0], E[1], C["F1"]])
        for _ in range(600):
            yield
            if json.loads(director.describe_rainbow())["done"]: break
        d = json.loads(director.describe_rainbow())
        check(d["done"], "站到对的位置：虹醒过来")
        t = 0.05   # 桥头往里 1 m：桥面在脚上方 0.76 m，两边是护栏
        spot = [A[0] + ax[0] * BR["len"] * t, A[1] + ax[1] * BR["len"] * t, C["F1"]]
        go(spot); look(math.degrees(math.atan2(ax[1], ax[0])), 0.0)
        for _ in range(900):
            yield
            if json.loads(director.describe_rainbow())["bridgeOn"]: break
        yield from wait(30)
        check(json.loads(director.describe_rainbow())["bridgeOn"], "虹桥长到头、能踩了（这时候人就站在桥头的路上，离桥头 %.0f cm：桥面和两边的护栏都把人夹在里面）" % (BR["len"] * t))
        yield from shoot(w, "虹桥_长出来时人在桥头")
        # 人在桥底下（不在桥上）：护栏不挡，往旁边走得出去；出去以后再走回来也行（还是在桥底下）
        f0 = foot()
        for _ in range(50):
            pawn.add_movement_input(unreal.Vector(BR["side"][0], BR["side"][1], 0), 1.0, False); yield
        f1 = foot(); m_side = math.dist(f0[:2], f1[:2])
        check(m_side > 150.0, "往旁边走得出去：走了 %.0f cm（被护栏圈住的话只有 50 cm 上下）" % m_side)
        # 回到刚才那一处（桥面在脚上方 0.76 m），朝桥头走：前面的桥面越来越低，低到迈得上去的高度就挡住了（那是桥，绕到桥头上去）；
        # 这时候往两边还是走得开，不会被圈住
        go(spot); yield from wait(20)
        for _ in range(60):
            pawn.add_movement_input(unreal.Vector(-ax[0], -ax[1], 0), 1.0, False); yield
        f0 = foot()
        for _ in range(50):
            pawn.add_movement_input(unreal.Vector(-BR["side"][0], -BR["side"][1], 0), 1.0, False); yield
        f1 = foot(); m_side2 = math.dist(f0[:2], f1[:2])
        check(m_side2 > 100.0, "桥头底下走到头以后往另一边走：走了 %.0f cm（走得开）" % m_side2)
        # 走开以后桥是实的：从桥头走上去，脚会升高
        go(RB["flow"][2]["pts"][0]["foot"], 8.0)
        yield from wait(30)
        z0 = foot()[2]
        for _ in range(240):
            pawn.add_movement_input(unreal.Vector(ax[0], ax[1], 0), 1.0, False); yield
        check(foot()[2] - z0 > 60.0, "走开以后桥还是实的：顺着桥走，脚升高了 %.0f cm" % (foot()[2] - z0))
        go(polar(250.0, 1400.0, 600.0)); yield from wait(20)   # 回到一层的地上（入夜时虹桥会消失）

    # ── 入夜 ──
    cmd(w, "Dysis.WorldRelease"); cmd(w, "Dysis.Sluice 1")   # 光圈、水雾恢复自动
    director.debug_set_night(True); night[0] = True
    if on("night"):
        yield from seconds(9.0)
        ok = False; info = ""
        for m in music:
            comps = {c.get_name(): c.is_playing() for c in m.get_components_by_class(unreal.AudioComponent)}
            info = "%s 音量 %.1f" % (comps, m.get_editor_property("music_volume"))
            ok = m.is_night() and comps.get("NightAudio") and not comps.get("DayAudio")
        check(ok, "入夜 9 秒后：夜里的曲子在放、白天的停了（%s）" % info)
        for m in music: m.stop_music(0.5)
        for name, az, r, z, turn, pitch in [("屋顶_刚入夜", 100.0, 1420.0, 3035.0, 0.0, -8.0), ("屋顶_刚入夜_看东边", 100.0, 1420.0, 3035.0, 160.0, 12.0), ("四层_天鹅旁", 250.0, 1400.0, 2300.0, 20.0, 4.0),
                                             ("三层_三相像旁", 300.0, 1400.0, 1450.0, -30.0, 6.0), ("二层_厚墙前", 152.0, 1400.0, 600.0, 60.0, 4.0), ("水庭_看水亭", 60.0, 1250.0, 0.0, 0.0, 2.0)]:
            go(polar(az, r, z), 8.0); look(az + 180.0 + turn, pitch)
            yield from wait(150)
            yield from shoot(w, "夜_" + name)
    for m in music: m.stop_music(0.5)

    # ── 月之龛 ──
    if on("shrine"):
        S = N2["MSHRINE"]
        director.debug_set_mirror_slot(S["slot"]); tc.set_forced_time(N2["H_3X"] - 3.0)
        go(polar(S["az"] + 9.0, 1400.0, 1450.0)); look_at([S["pos"][0], S["pos"][1], S["pos"][2] - 40.0])
        yield from wait(120)
        # 灰盒里量过的那几个时刻挨个摆一遍，摆到月光落在龛上为止
        for row in N2G["shrine"]:
            if json.loads(director.describe_twins())["shrinePerm"]: break
            tc.set_forced_time(row["H"])
            yield from wait(40)
        yield from wait(90)
        d = json.loads(director.describe_twins())
        shown = {}
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
            for t in a.tags:
                if str(t).startswith("SM_Mech_MoonShrine_"): shown[str(t)[19:]] = (not a.get_editor_property("hidden"), round(math.hypot(a.get_actor_location().x, a.get_actor_location().y)))
        check(d["shrinePerm"] and shown.get("Shard", (False,))[0] and shown.get("Niche", (False,))[0] and not shown.get("Block", (True,))[0], "月之龛透开：石台和碎片显出来、挡着的那块墙没了（显示, 离殿心 cm）%s" % shown)
        yield from shoot(w, "月之龛_透开了")
        go(polar(S["az"] - 3.0, 1440.0, 1450.0)); look_at([S["pos"][0], S["pos"][1], S["pos"][2] - 20.0])
        yield from wait(60)
        yield from shoot(w, "月之龛_走近_有提示")
        check(director.debug_interact("moonShrine") and json.loads(director.describe_twins())["moonShard"], "走近按 E：拿到月亮碎片")
        tc.clear_forced_time()

    # ── 月虹 ──
    if on("moonbow"):
        go(polar(300.0, 1330.0, 600.0)); look_at(polar(128.0, 1040.0, 330.0))
        director.debug_set_selene_talked(False); tc.set_forced_time(110.0)
        yield from seconds(4.0)
        a = sky()
        director.debug_set_selene_talked(True)
        yield from seconds(5.0)
        b = sky()
        check(a["moonbow"] < 0.02 and not a["moonbowShown"] and b["moonbow"] > 0.9 and b["moonbowShown"], "月虹：那段话没说过时没有（%.2f），说完了、月亮在东边 27° 高时显出来（%.2f）" % (a["moonbow"], b["moonbow"]))
        yield from shoot(w, "月虹_二层对面看")
        go(polar(305.0, 1330.0, 0.0), 10.0); look_at(polar(128.0, 1040.0, 300.0))
        yield from wait(90)
        yield from shoot(w, "月虹_水庭里看")
        tc.set_forced_time(170.0)
        yield from seconds(5.0)
        c = sky()
        check(c["moonbow"] < 0.05, "月亮升高以后（68°）月虹淡掉（%.2f）" % c["moonbow"])
        tc.clear_forced_time()

    # ── 结局：星座 ──
    if on("ending"):
        cmd(w, "Dysis.Iris 1100")
        go([90.0, 60.0, 20.0], 8.0); look(200.0, 0.0)
        yield from wait(120)
        yield from shoot(w, "结局_放苹果之前")
        director.debug_play_ending(True)
        marks = [(2.6, "镜头抬起来"), (6.0, "双子座画到一半"), (9.2, "开始画天鹅座"), (12.4, "都画完了")]
        t0 = now()
        while marks and now() - t0 < 90.0:
            yield
            s = sky()
            if s["starShow"] >= marks[0][0]:
                yield from shoot(w, "结局_星座_%s（%.1f 秒）" % (marks[0][1], s["starShow"])); marks.pop(0)
        s = sky(); f = json.loads(director.describe_finale())
        check(not marks and s["conStars"] == 25 and s["conLines"] == 23 and s["endingAll"] and f["endingLines"] == 3, "结局（三片碎片都有）：星座 %d 颗星、%d 条线，动画共 %.1f 秒；对话 %d 句" % (s["conStars"], s["conLines"], s["starShowEnd"], f["endingLines"]))
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
_r = R(); _r.h = unreal.register_slate_post_tick_callback(_r.tick)
