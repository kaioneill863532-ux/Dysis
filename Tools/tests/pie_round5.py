# 第五轮反馈（2026-10-10）的测试（UE 编辑器 Python，开 PIE 跑）：
#   星轨（主界面入夜）、音乐的走法、对话不自己翻页、三相像从第 0 格起、镜头不乱跳不穿墙、
#   瀑布加厚并且开闸后进不去、夜里楼梯最下一级不闪、机关和墙同色齐平、脚下的影子、月虹。
# 想只跑其中几段：把段名写在 Saved/round5_only.txt（逗号隔开：menu,music,dialogue,mirror,camera,fall,mech,night）。
# 结果写在 项目/Saved/pie_round5.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_round5.txt"
ONLY = [x.strip() for x in open(SAVED + "round5_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "round5_only.txt") else []
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
def shoot(w, tag, ui=False):
    before = shots()
    unreal.SystemLibrary.execute_console_command(w, "Shot showui" if ui else "Shot")
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]
def az_of(v): return math.degrees(math.atan2(v.y, v.x)) % 360.0

# 墙上的机关：名字、方位、半径、高度（部件中心）
MECH = [("封石_TR（一层）", 112.0, 1558, 720), ("封石_TS（二层）", 321.7, 1558, 1570), ("月亮浮雕", 178.7, 1557, 1580), ("月之龛的盖板", 6.6, 1548, 1650),
        ("天鹅浮雕旁的门", 259.7, 1559, 2420), ("塞勒涅的浮雕", 355.0, 1541, 243)]
GODDESS = (-1016.0, 992.0)

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0); pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    music = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager)[0]
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisSkyActor)[0]
    hud = pc.get_hud()
    music.set_muted(True)      # 测试不出声；音量之外的状态照常走
    def mus(): return json.loads(str(music.describe_music()))
    def now(): return unreal.GameplayStatics.get_real_time_seconds(w)
    def seconds(t):
        t0 = now()
        while now() - t0 < t: yield
    def cmd(s): unreal.SystemLibrary.execute_console_command(w, s)

    # ── 主界面：白天的曲子在放；入夜以后星星带拖尾 ──
    if on("menu"):
        yield from seconds(1.0)
        m = mus()
        check(m["want"] == "day", "主界面：放的是白天的曲子（%s）" % m)
        t0 = now(); seen = None
        while now() - t0 < 70.0:
            yield
            d = json.loads(str(sky.describe_star_trails()))
            if d["shown"] and d["starOpacity"] > 0.85: seen = d; break
        check(seen is not None and seen["count"] == 1800 and seen["menu"], "主界面入夜：1800 颗星各拖一道尾（%s）" % seen)
        yield from seconds(2.0)
        yield from shoot(w, "主界面_星星的拖尾", ui=True)
        cam = pc.player_camera_manager
        cl, cr = cam.get_camera_location(), cam.get_camera_rotation()
        say("  主界面镜头：位置 (%.0f, %.0f, %.0f)，朝向 yaw %.1f pitch %.1f" % (cl.x, cl.y, cl.z, cr.yaw, cr.pitch))

    hud.start_game(True)
    yield from wait(60)
    music.set_muted(True)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    night = [False]
    def go(p, dz=8.0): cmd("Dysis.Go %.2f %.2f %.2f%s" % (p[0], p[1], p[2] + dz, " night" if night[0] else ""))
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - 90.0)
    def view(tag, eye, at, fov=75.0, frames=70):
        director.debug_camera(unreal.Vector(*eye), unreal.Vector(*at), fov)
        yield from wait(frames)
        yield from shoot(w, tag)
        director.debug_camera_off()
    def walk(dx, dy, frames):
        n = math.hypot(dx, dy)
        for _ in range(frames):
            pawn.add_movement_input(unreal.Vector(dx / n, dy / n, 0.0), 1.0, False)
            yield

    # ── 音乐 ──
    if on("music"):
        yield from seconds(0.5)
        m = mus()
        check(m["want"] == "none", "点了开始游戏、人还在岛上：曲子淡出去，只留海浪（%s）" % m)
        go(polar(23.7, 3050.0, -120.0)); yield from seconds(0.8)      # 人已经顺着那道光走上台地了（脚离开岛面往上）
        m = mus()
        check(m["want"] == "day" and m["day"] < 0.6, "踩上通往神殿的那道光：白天的曲子开始慢慢响起来（%s）" % m)
        away = polar(200.0, 1400.0, 600.0)                              # 一层走廊上旁边没有机关的地方
        go(away); yield from seconds(3.5)
        m0 = mus(); p0 = str(director.debug_prompt_at(unreal.Vector(*foot())))
        go(polar(328.5, 1420.0, 600.0)); yield from seconds(2.6)         # 一层的机关旁边
        m1 = mus(); p1 = str(director.debug_prompt_at(unreal.Vector(*foot())))
        check(p0 == "" and p1 != "" and m1["duck"] < 0.6 and m0["duck"] > 0.95, "走到机关旁边（%s）：曲子压低（%.2f → %.2f）" % (p1, m0["duck"], m1["duck"]))
        go(away); yield from seconds(4.2)
        m2 = mus()
        check(m2["duck"] > 0.95, "离开机关：曲子放回来（%.2f）" % m2["duck"])

    # ── 对话：不自己翻页；点一下翻一句，连点两下只翻一句 ──
    if on("dialogue"):
        hud.set_test_mode(False)
        cmd("Dysis.Dialogue"); yield from seconds(0.5)
        dlg = [c for c in pawn.get_components_by_class(unreal.DysisDialogueComponent) if c.is_playing()]
        check(len(dlg) == 1, "演示对话开始了")
        if dlg:
            d = dlg[0]
            yield from seconds(6.0)
            check(d.get_editor_property("current_line") == 0, "等了 6 秒：还停在第一句，没有自己翻页")
            m = mus()
            check(not on("music") or m["duck"] < 0.6, "过剧情时曲子压低（%.2f）" % m["duck"])
            pawn.interact_pressed(); pawn.interact_pressed()
            yield from wait(3)
            check(d.get_editor_property("current_line") == 1, "连点两下：只翻了一句（现在第 %d 句）" % (d.get_editor_property("current_line") + 1))
            yield from seconds(0.4)
            pawn.interact_pressed(); yield from wait(3)
            check(d.get_editor_property("current_line") == 2, "隔一会儿再点一下：翻到下一句")
            hud.set_test_mode(True)
            for _ in range(8):
                if not d.is_playing(): break
                yield from seconds(0.35); pawn.interact_pressed()
            check(not d.is_playing(), "一句一句点完，对话结束")
        hud.set_test_mode(True)

    # ── 三相像：开局在第 0 格，转两下才到白天那一格；没转过的话入夜她自己转回白天那一格 ──
    if on("mirror"):
        def mir(): return json.loads(str(director.describe_mirror()))
        go(polar(286.0, 1530.0, 1450.0)); yield from seconds(2.5)
        tc.set_forced_time(32.98); yield from seconds(1.5)
        m = mir()
        tc.clear_forced_time()
        check(m["slot"] == 0 and m["form"] != 1, "开局：三相像在第 0 格，不是日相（镜光还没有）：%s" % {k: m[k] for k in ("slot", "form", "yaw")})
        yield from view("三相像_开局（第 0 格）", polar(300.0, 1150.0, 1720.0), polar(286.0, 1350.0, 1640.0), 70.0)
        director.debug_set_night(True); yield from seconds(2.5)
        m = mir()
        check(m["slot"] == 2, "白天一次都没转过：入夜时她自己转回白天那一格（第 %d 格），夜里和灰盒一样从这里转起" % m["slot"])
        director.debug_set_night(False); yield from seconds(1.0)
        director.debug_set_mirror_slot(0); yield from seconds(2.5)
        check(director.debug_interact("mirrorCrank") and director.debug_interact("mirrorCrank"), "转两下绞盘")
        tc.set_forced_time(32.98)                                        # 灰盒里镜光照到日之龛的那个时刻
        yield from seconds(3.5)
        m = mir()
        check(m["slot"] == 2 and m["form"] == 1, "转了两下：到白天那一格，她举起镜子（日相），镜光出来了：%s" % {k: m[k] for k in ("slot", "form")})
        yield from view("三相像_转两下以后", polar(300.0, 1150.0, 1720.0), polar(286.0, 1350.0, 1640.0), 70.0)
        director.debug_set_night(True); yield from seconds(1.5)
        director.debug_set_mirror_slot(0); yield from seconds(0.5)
        check(mir()["slot"] == 0, "转过的人：入夜以后她不会自己动")
        director.debug_set_night(False); director.debug_set_mirror_slot(2); tc.clear_forced_time(); yield from seconds(1.0)

    # ── 镜头：贴着墙走不会整个弹到人身上 ──
    if on("camera"):
        cam = pc.player_camera_manager
        worst = 0.0; near = 0; total = 0
        for name, p, yaw in (("水庭", polar(285.0, 1250.0, 0.0), 105.0), ("一层走廊", polar(250.0, 1400.0, 600.0), 160.0), ("三层走廊", polar(250.0, 1400.0, 2300.0), 340.0)):
            go(p); look(yaw, -12.0); yield from wait(90)
            yr = math.radians(yaw); fwd = (math.cos(yr), math.sin(yr)); right = (-math.sin(yr), math.cos(yr))
            for d in (right, (-right[0], -right[1]), (right[0] + fwd[0], right[1] + fwd[1]), (-fwd[0], -fwd[1])):
                prev = None
                for i in range(50):
                    pawn.add_movement_input(unreal.Vector(d[0], d[1], 0.0), 1.0, False)
                    yield
                    arm = pawn.get_camera_arm_now()
                    if prev is not None:
                        # 收近是立刻的（被墙挡住），放回去是慢慢的（每秒 900 cm）；“突然变远”才是乱跳。按每秒算：帧率低的时候一帧走得多
                        rate = (arm - prev) / max(unreal.GameplayStatics.get_world_delta_seconds(w), 0.004)
                        if rate > worst: worst = rate
                    if arm < 5.0: near += 1
                    total += 1; prev = arm
                yield from wait(15)
        check(worst < 1000.0 and near < total * 0.2, "贴着墙来回走 %d 帧：镜头没有“弹到人身上又弹回去”（放远最快每秒 %.0f cm，贴到脸上的有 %d 帧）" % (total, worst, near))
        # 镜头和瞄点之间、镜头周围没有墙：朝四面扫，镜头到人头之间不被挡（挡镜头用的是“相机”那一路的碰撞）
        blocked = 0; n = 0
        for name, p in (("水庭边", polar(285.0, 1480.0, 0.0)), ("一层贴外墙", polar(250.0, 1490.0, 600.0)), ("一层贴内栏", polar(250.0, 1260.0, 600.0)), ("三层贴外墙", polar(40.0, 1490.0, 2300.0))):
            go(p); yield from wait(50)
            for yaw in range(0, 360, 30):
                look(float(yaw), -15.0); yield from wait(6)
                c = cam.get_camera_location(); l = pawn.get_actor_location()
                head = unreal.Vector(l.x, l.y, l.z + 60.0)
                hit = unreal.SystemLibrary.line_trace_single(w, head, c, unreal.TraceTypeQuery.TRACE_TYPE_QUERY2, False, [pawn], unreal.DrawDebugTrace.NONE, True)
                n += 1
                if hit is not None: blocked += 1
        check(blocked == 0, "贴着墙站、转一整圈（%d 个方向）：镜头始终和人在墙的同一边（被墙隔开的有 %d 次）" % (n, blocked))

    # ── 瀑布：加厚、两头封上；开闸以后进不去 ──
    if on("fall"):
        d = json.loads(str(director.describe_waterfall()))
        check(d["extraSheets"] == 4 and d["sideSheets"] == 2 and d["blocks"] == 6 and d["blocking"] == 0 and d["splashFoam"] == 3 and d["drops"] > 200,
              "瀑布：前后各多一层、两头各一片，脚下两层水花、水面一片白沫、三百来颗水珠；没开闸时不挡人：%s" % d)
        def dist_goddess(): f = foot(); return math.hypot(f[0] - GODDESS[0], f[1] - GODDESS[1])
        # 没开闸：从东边的石台能走到女神像跟前
        go(polar(110.0, 1430.0, 90.0)); yield from wait(40)
        t = polar(133.0, 1430.0, 90.0)
        for _ in range(260):
            f = foot()
            if math.hypot(t[0] - f[0], t[1] - f[1]) < 40: break
            yield from walk(t[0] - f[0], t[1] - f[1], 1)
        before = dist_goddess()
        check(before < 220.0, "没开闸：从东边的石台能走到女神像跟前（离她 %.0f cm）" % before)
        cmd("Dysis.Sluice 1"); yield from seconds(3.0)
        d = json.loads(str(director.describe_waterfall()))
        check(d["blocking"] == 6, "开了闸：挡人的墙生效（%d 道）" % d["blocking"])
        worst = 9999.0
        for name, start, target in (("东边石台", polar(110.0, 1430.0, 90.0), polar(133.0, 1430.0, 90.0)), ("水庭地面", polar(146.0, 1130.0, 0.0), [GODDESS[0], GODDESS[1], 0.0]),
                                    ("石台靠水帘", polar(112.0, 1300.0, 90.0), polar(128.0, 1300.0, 90.0)), ("一层走廊", polar(111.0, 1400.0, 600.0), polar(128.0, 1400.0, 600.0))):
            go(start); yield from wait(40)
            for _ in range(200):
                f = foot()
                yield from walk(target[0] - f[0], target[1] - f[1], 1)
            f = foot(); a = math.degrees(math.atan2(f[1], f[0])) % 360.0; r = math.hypot(f[0], f[1])
            inside = 116.25 < a < 139.75 and r > 1245.0
            worst = min(worst, dist_goddess())
            check(not inside, "开了闸，从%s往女神像走：进不去（停在方位 %.1f° 半径 %.0f 高 %.0f，离她 %.0f cm）" % (name, a, r, f[2], dist_goddess()))
        p = str(director.debug_prompt_at(unreal.Vector(*polar(144.0, 1150.0, 0.0))))
        check(p == "viewGoddess", "站在水庭这边还能“查看”女神像（%s）" % p)
        go(polar(285.0, 1250.0, 0.0)); yield from wait(60)
        yield from view("瀑布_正面", polar(300.0, 700.0, 260.0), polar(128.0, 1250.0, 500.0), 80.0)
        yield from view("瀑布_从东边石台看侧面", polar(96.0, 1380.0, 300.0), polar(124.0, 1380.0, 320.0), 80.0)
        yield from view("瀑布_斜着看厚度", polar(100.0, 1050.0, 240.0), polar(128.0, 1300.0, 500.0), 80.0)
        yield from view("瀑布_从一层看", polar(100.0, 1380.0, 780.0), polar(128.0, 1320.0, 900.0), 80.0)

    # ── 机关和墙同色、齐平；脚下的影子 ──
    if on("mech"):
        d = json.loads(str(director.describe_mech_look()))
        check(d["recolored"] >= 20, "石头做的机关换成了墙的颜色：%d 个面" % d["recolored"])
        ok = [f for f in d["flush"] if not f["origShown"] and f["back"] > 3.0]
        # 塞勒涅的是浮雕：齐平的是底板，眼睛的花纹还凸在墙面外 12 cm；银盘跟着月亮浮雕走
        inner = [f["inner"] for f in d["flush"] if f["tag"] not in ("SM_Mech_MoonRelief_Disk", "SM_Mech_Selene_Relief")]
        sel = [f["inner"] for f in d["flush"] if f["tag"] == "SM_Mech_Selene_Relief"]
        # 第八轮起：两段楼梯下口的封石、天鹅浮雕后面的门、月之龛的盖板改成照洞口现做的门板（pie_round8 管），这里只剩月亮浮雕（连银盘）和塞勒涅的浮雕板
        check(len(d["flush"]) == 3 and len(ok) == 3 and all(abs(x - 1549.0) < 2.5 for x in inner) and len(sel) == 1 and abs(sel[0] - 1537.0) < 2.5,
              "月亮浮雕（连银盘）、塞勒涅的浮雕板退到和墙面齐平（各退了 %s cm；朝殿心的面离殿心 %s cm，墙面是 1550；塞勒涅浮雕的花纹凸出到 %s）" % ([round(f["back"]) for f in d["flush"]], [round(x) for x in inner], [round(x) for x in sel]))
        for name, az, r, z in MECH:
            floor_z = max([f for f in (0, 600, 1450, 2300) if f <= z + 60] or [0])
            go(polar(az + 25.0, 1300.0, floor_z)); yield from wait(50)
            yield from view("机关_%s" % name, polar(az + 13.0, 1180.0, z + 30.0), polar(az, r, z), 60.0)
        yield from view("机关_封石TR_贴近看", polar(105.0, 1380.0, 760.0), polar(113.0, 1550.0, 720.0), 70.0)
        blob = pawn.get_components_by_class(unreal.DecalComponent)
        check(len(blob) == 1 and blob[0].is_visible() and blob[0].get_decal_material() is not None, "人脚下有一团影子（贴花）")
        go(polar(285.0, 1250.0, 0.0)); look(110.0, -30.0); yield from wait(120)
        yield from shoot(w, "脚下的影子_水庭（人的镜头）")
        go(polar(250.0, 1400.0, 600.0)); look(160.0, -35.0); yield from wait(100)
        yield from shoot(w, "脚下的影子_一层走廊（人的镜头）")

    # ── 夜里：楼梯最下一级不和楼面叠在一起；月虹 ──
    if on("night"):
        director.debug_set_night(True); night[0] = True
        cmd("Dysis.Sluice 1")
        go(polar(112.0, 1311.0, 2300.0)); yield from seconds(9.0)
        tops = []
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
            if any(str(t).startswith("SM_Mech_RoofSteps_Dn") and not str(t).endswith("_Curb") for t in a.tags):
                o, e = a.get_actor_bounds(False)
                if abs(o.z + e.z - 2300.0) < 3.0: tops.append(o.z + e.z)
        check(len(tops) >= 1 and all(t > 2300.3 for t in tops), "夜里的楼梯降到头：和四层楼面一样高的那一级抬高了一点（顶面 %s，楼面 2300），不再和楼面叠着闪" % [round(t, 2) for t in tops])
        look(72.0, -28.0); yield from wait(60)
        yield from shoot(w, "夜_四层楼梯口的地面（人的镜头）")
        director.debug_set_selene_talked(True); tc.set_forced_time(110.0)
        go(polar(285.0, 1250.0, 0.0)); yield from seconds(9.0)
        f = json.loads(str(director.describe_sky_fx()))
        check(f["moonbowShown"] and f["moonbow"] > 0.9, "月虹显出来了（%.2f）" % f["moonbow"])
        yield from view("月虹_水庭里看", polar(310.0, 900.0, 130.0), polar(128.0, 1040.0, 330.0), 75.0)
        yield from view("月虹_远一点", polar(300.0, 1250.0, 420.0), polar(128.0, 1040.0, 300.0), 75.0)
        yield from view("月虹_一层看", polar(300.0, 1380.0, 780.0), polar(128.0, 1040.0, 300.0), 70.0)
        yield from view("月虹_侧面", polar(60.0, 1100.0, 160.0), polar(128.0, 1040.0, 300.0), 80.0)
        tc.clear_forced_time()
        if on("music"):
            m = mus()
            check(m["want"] == "night", "入夜：换成夜晚的曲子（%s）" % m)
            ok = director.debug_interact("placeApple"); yield from seconds(0.6)
            m = mus()
            check(ok and m["want"] == "day", "把金苹果放上月托：夜晚的曲子淡下去，换回白天的曲子（%s）；剧情放完回主界面时接着放" % m)
        night[0] = False
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
_r5 = R(); _r5.h = unreal.register_slate_post_tick_callback(_r5.tick)
