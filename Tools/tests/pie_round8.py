# 第八轮反馈（2026-10-10）的测试（UE 编辑器 Python，开 PIE 跑）：
#   美术的机关模型换上了（没有被削成碎块）、暗门和楼梯窗的拱是照洞口现做的、暗门的门板和墙齐平并且跟着门走、
#   瀑布底下补了水面、殿里的地面和墙一个颜色、殿里的柱子让开了栏杆和楼板的边（挡人挡光的原模型没动）、
#   走路的步频和步幅、走和上台阶时上半身在动、镜湖那一版海面能切。
# 想只跑其中几段：把段名写在 Saved/round8_only.txt（逗号隔开：models,doors,water,look,walk,stairs）。
# 结果写在 项目/Saved/pie_round8.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_round8.txt"
ONLY = [x.strip() for x in open(SAVED + "round8_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "round8_only.txt") else []
def on(name): return not ONLY or name in ONLY
lines = []; bad = [0]
def say(s):
    lines.append(str(s)); open(OUT, "w", encoding="utf-8").write("\n".join(lines))
def check(ok, what):
    if not ok: bad[0] += 1
    say(("✔ " if ok else "✘ ") + what)
def gw(): return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
def wait(n):
    for _ in range(n): yield
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]

def main():
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
    for _ in range(6000):
        w = gw()
        if w and unreal.GameplayStatics.get_player_pawn(w, 0): break
        yield
    w = gw(); pc = unreal.GameplayStatics.get_player_controller(w, 0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisPrologueDirector): a.destroy_actor()
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisMusicManager): m.set_muted(True)
    pc.get_hud().start_game(True)
    yield from wait(120)
    director = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisDirector)[0]
    pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
    V = unreal.Vector
    def cmd(s): unreal.SystemLibrary.execute_console_command(w, s)
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def seconds(t):
        t0 = now()
        while now() - t0 < t: yield
    def go(p, dz=6.0): cmd("Dysis.Go %.2f %.2f %.2f" % (p[0], p[1], p[2] + dz))
    def look(yaw, pitch): pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    def det(): return json.loads(str(director.describe_details()))
    def r8(): return json.loads(str(director.describe_round8()))
    allact = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
    def tagged(tag): return [a for a in allact if any(str(t) == tag for t in a.tags)]
    def tagged_pfx(pfx): return [a for a in allact if any(str(t).startswith(pfx) for t in a.tags)]

    # ── 美术的机关模型 ──
    if on("models"):
        # 面数：这几件是照“保住原有大平面”的办法减的，面数落在各自的范围里（被引擎按面数硬减过的话会是整三千、五千五）
        want = {"TwinCastor": (3500, 4500), "TwinPollux": (3800, 4800), "Selene": (4300, 6000), "IrisRelief": (7000, 12500), "SwanRelief": (1500, 2600),
                "MirrorSun": (5000, 6600), "MirrorMoon": (4500, 6000), "MirrorDark": (4800, 6300), "FacadeStatue": (2400, 3200), "Goddess": (3800, 5200),
                "GateArch": (300, 650), "GateDoor": (200, 450), "NicheBox": (1800, 3400), "NicheLid": (600, 1200), "Apple": (200, 400)}
        got = {}
        for name in want:
            m = unreal.load_asset("/Game/Dysis/MechPack_20261010/SM_MP_" + name)
            got[name] = m.get_num_triangles(0) if m else -1
        off = [n for n in want if not (want[n][0] <= got[n] <= want[n][1])]
        check(not off, "机关模型的面数都在该在的范围里：%s%s" % (", ".join("%s %d" % (n, got[n]) for n in sorted(got)), ("；不对的：%s" % off) if off else ""))
        # 浮雕没有背板：最凸的地方离墙面十几厘米，整件的底就是墙面（Y 从 0 起）
        for name, deep in (("Selene", 30.0), ("IrisRelief", 14.0), ("SwanRelief", 18.0)):
            b = unreal.load_asset("/Game/Dysis/MechPack_20261010/SM_MP_" + name).get_bounding_box()
            check(-0.5 < b.min.y < 1.0 and 8.0 < b.max.y < deep, "%s：贴在墙面上（从墙面起 %.1f cm），最凸 %.1f cm" % (name, b.min.y, b.max.y))
        mp = str(director.describe_mech_pack())
        say("   机关模型：" + mp[:300])
        # 换上去的模型挂在原件身上：原件一个都不画
        shown = []
        for tag in ("SM_Mech_Twins_Castor", "SM_Mech_Twins_Pollux", "SM_Mech_Selene_Relief", "SM_Mech_IrisRelief_Relief", "SM_Mech_Swan_Relief", "SM_Mech_Mirror_Statue", "SM_Mech_Goddess_Statue"):
            for a in tagged(tag):
                comps = a.get_components_by_class(unreal.StaticMeshComponent)
                new = [c for c in comps if c.static_mesh and c.static_mesh.get_name().startswith("SM_MP_")]
                old = [c for c in comps if c.static_mesh and not c.static_mesh.get_name().startswith("SM_MP_") and c.is_visible()]
                if not new or old: shown.append(tag)
        check(not shown, "双子、塞勒涅、伊莉丝、天鹅浮雕、三相像、瀑布后女神：都换上了新模型，原来的灰盒件不画%s" % (("（不对的：%s）" % shown) if shown else ""))
        mpj = json.loads(mp)
        def bounds(c): return unreal.SystemLibrary.get_component_bounds(c)[:2]
        def named(tag, mesh): return [c for a in tagged(tag) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith(mesh)]
        # 三相像：放大到 1.4 倍、站在底座正中；底座半径缩到 0.62、看得见的那一份矮到 40 cm，手轮在底座外面
        st = tagged("SM_Mech_Mirror_Statue")[0]; pl = tagged("SM_Mech_Mirror_Plinth")[0]; crank = tagged("SM_Mech_Mirror_Crank")[0]
        form = [c for c in named("SM_Mech_Mirror_Statue", "SM_MP_Mirror") if c.is_visible()][0]
        fo = form.get_world_location(); so = st.get_actor_location(); plv = [c for c in pl.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
        po, pe = bounds(plv[0]) if plv else (unreal.Vector(), unreal.Vector())
        co, ce = crank.get_actor_bounds(False)
        gap = math.hypot(co.x - pl.get_actor_location().x, co.y - pl.get_actor_location().y) - max(ce.x, ce.y) - pe.x
        off = math.hypot(fo.x - so.x, fo.y - so.y)
        check(abs(form.get_world_scale().x - 1.4) < 0.01 and off < 0.5 and abs(pl.get_actor_scale3d().x - 1.0) < 0.01 and abs(pe.x - 106.6) < 3.0 and len(plv) == 1 and abs(pe.z * 2.0 - 40.0) < 1.0 and abs(fo.z - (po.z + pe.z)) < 1.0 and gap > 5.0,
              "三相像放大到 %.1f 倍、站在底座正中（偏 %.1f cm）、脚踩在底座面上；圆底座半径 %.0f cm、高 %.0f cm，手轮离它 %.0f cm" % (form.get_world_scale().x, off, pe.x, pe.z * 2.0, gap))
        # 日之龛：盖子是单装的（朝着人掀）、关着的时候是平的；碎片从盒子正中、盒面上面出来
        bo, be = bounds(named("SM_Mech_SunNiche_Box", "SM_MP_NicheBox")[0]); sb = mpj["sunShardBase"]
        check(mpj["lidModel"] and mpj["lidTiltDeg"] < 1.0 and math.hypot(sb[0] - bo.x, sb[1] - bo.y) < 2.0 and 0.0 < sb[2] - (bo.z + be.z) < 16.0 and bo.z + be.z > 2040.0,
              "日之龛：盖子关着是平的（%.1f°）；碎片从盒子正中出来（离中心 %.1f cm、比盒面高 %.0f cm）" % (mpj["lidTiltDeg"], math.hypot(sb[0] - bo.x, sb[1] - bo.y), sb[2] - (bo.z + be.z)))
        # 拉杆：缩小、装到墙上；原来立在地上的那两件不挡人
        lb = named("SM_Mech_LeverA_Base", "SM_MP_LeverBase")[0]
        lo, le = bounds(lb)
        old = named("SM_Mech_LeverA_Base", "SM_Kit_") + named("SM_Mech_LeverA_Arm", "SM_Kit_")
        free = bool(old) and all(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) == unreal.CollisionResponseType.ECR_IGNORE for c in old)
        check(abs(lb.get_world_scale().x - 0.62) < 0.01 and lo.z - le.z > 670.0 and 1515.0 < math.hypot(lo.x, lo.y) < 1545.0 and mpj["leverArm"][0] > 1515.0 and free,
              "拉杆缩到 %.2f、装在墙上（中心离殿心 %.0f cm，底离地 %.0f cm），原来立在地上的那两件不挡人了（%s）" % (lb.get_world_scale().x, math.hypot(lo.x, lo.y), lo.z - le.z - 600.0, free))
        # 金苹果：小了、换了金的材质、外面套着虹光的球
        ap = [c for c in director.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "SM_MP_Apple"]
        ah = bounds(ap[0])[1].z * 2.0 if ap else 99.0
        check(len(ap) == 1 and ah < 15.5 and ap[0].get_material(0).get_name() == "M_DysisAppleGold" and not mpj["appleHalo"], "金苹果高 %.0f cm，材质 %s（周围的虹光做出来以后用户说不要，没装）" % (ah, ap[0].get_material(0).get_name() if ap else "-"))
        # 瀑布后面的女神像：从灰盒的位置往瀑布中间转了 2°、往墙那边挪了 68 cm；模型自己带圆台座（原来的方墩子不画）
        gs = tagged("SM_Mech_Goddess_Statue")[0].get_actor_location(); gaz = math.degrees(math.atan2(gs.y, gs.x)) % 360.0
        gpl = [c for a in tagged("SM_Mech_Goddess_Plinth") for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
        check(abs(gaz - 133.68) < 0.05 and abs(math.hypot(gs.x, gs.y) - 1488.3) < 0.5 and not gpl, "瀑布后面的女神像挪到方位 %.2f°、离殿心 %.0f cm（灰盒 135.68° / 1420）；原来的方墩子不画了" % (gaz, math.hypot(gs.x, gs.y)))
        # 虹之龛做出了进深：门在柜子的前口上、碎片在柜子里面
        # （门和碎片的新位置要等石沿伸出来、整组重新摆的时候才落到位——pie_rainbow 里开龛那一段会走到；这里核四面龛壁加上了、进深 28 cm）
        walls = [c for c in tagged("SM_Mech_Sill_Niche")[0].get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "Cube"]
        deep = sorted(round(c.get_editor_property("relative_scale3d").y * 100.0) for c in walls)
        check(len(walls) == 4 and deep == [28, 28, 28, 28], "虹之龛：加了四面龛壁，做成一个 %s cm 深的小柜子（碎片 22 cm 大，放得进去）" % (deep[0] if deep else "?"))
        # 外立面神像：没有底座、往墙里收
        fo2, fe2 = bounds(named("SM_Facade_Statue_L3_01", "SM_MP_FacadeStatue")[0])
        check(math.hypot(fo2.x, fo2.y) < 1745.0 and abs((fo2.z - fe2.z) - (2420.0 + 13.0)) < 2.0, "外立面神像往墙里收了（中心离殿心 %.0f cm；原来 1757），脚踩在窗台上（底在 %.0f）" % (math.hypot(fo2.x, fo2.y), fo2.z - fe2.z))
        # 伊莉丝浮雕：下面有那个空着的人形；原来那一圈铺在浮雕前面的虹不画了
        check(mpj["shadowSpot"] and not mpj["bandsShown"], "伊莉丝浮雕下面补上了那个空着的人形（影子的头要落进去的地方）；原来铺在浮雕前面的那一圈虹不画了")
        fix = [c.get_material(i).get_name() for a in tagged_pfx("SM_Roof_Fix") for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible() for i in range(c.get_num_materials()) if c.get_material(i)]
        check(bool(fix) and set(fix) == {"MI_MATCH_WarmWhite"}, "屋顶桥门前那几级固定的台阶和别的台阶一个颜色（%s）" % sorted(set(fix)))

    # ── 暗门、拱 ──
    if on("doors"):
        d = det()
        check(d["arches"] == 9 and d["leaves"] == 4, "拱 %d 个（暗门 4、楼梯的窗 5）；照洞口做的门板 / 盖板 %d 块（两段楼梯的下口、天鹅浮雕后面的门、月之龛）" % (d["arches"], d["leaves"]))
        ml = json.loads(str(director.describe_mech_look()))
        flush = sorted(f["tag"] for f in ml["flush"])
        check("SM_Mech_StairSeal_TR" not in flush and "SM_Mech_Swan_Door" not in flush and "SM_Mech_MoonShrine_Block" not in flush, "那几块比洞口大一圈、凸出墙面的方石板不再画了（还退平着画的：%s）" % flush)
        for tag, az, top in (("SM_Mech_StairSeal_TR", 112.0, 840.0), ("SM_Mech_StairSeal_TS", 321.728, 1690.0), ("SM_Mech_Swan_Door", 259.728, 2540.0)):
            a = tagged(tag)[0]
            pm = a.get_components_by_class(unreal.ProceduralMeshComponent)
            vis = [c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
            ok = len(pm) == 1 and not vis
            where = ""
            if pm:
                o, e = unreal.SystemLibrary.get_component_bounds(pm[0])[:2]
                r_in = math.hypot(o.x, o.y)
                # 门板：宽 1.29 m、高 2.4 m、厚 30 cm（15.5–15.8 m），中心在门洞中线上
                caz = math.degrees(math.atan2(o.y, o.x)) % 360.0
                ok = ok and abs(caz - az) < 0.05 and abs((o.z + e.z) - top) < 0.5 and abs((o.z - e.z) - (top - 240.0)) < 0.5 and 1560.0 < r_in < 1568.0
                where = "中线方位 %.3f、高 %.1f…%.1f" % (caz, o.z - e.z, o.z + e.z)
                # 跟着门走：门往下沉 80 cm，门板也沉 80 cm
                z0 = pm[0].get_world_location().z; loc = a.get_actor_location()
                a.set_actor_location(V(loc.x, loc.y, loc.z - 80.0), False, False); yield
                moved = pm[0].get_world_location().z - z0
                a.set_actor_location(loc, False, False); yield
                ok = ok and abs(moved + 80.0) < 0.1
                where += "；门沉 80 cm 它跟着沉 %.1f cm" % -moved
            check(ok, "%s：换成照拱洞做的门板（%s），原来的方石板不画" % (tag[8:], where))
        # 碰撞没动：门还挡着人和光（原件的碰撞）
        solid = all(a.get_actor_enable_collision() for tag in ("SM_Mech_StairSeal_TR", "SM_Mech_StairSeal_TS", "SM_Mech_Swan_Door") for a in tagged(tag))
        check(solid, "三扇暗门的碰撞照旧开着（挡人、挡光的还是原件）")

    # ── 瀑布底下的水 ──
    if on("water"):
        s = r8()
        pms = [c for c in director.get_components_by_class(unreal.ProceduralMeshComponent) if c.get_material(0) and "AnimeWater" in c.get_material(0).get_path_name()]
        ok = s["inletWater"] and s["inletVisible"] and len(pms) == 1
        where = ""
        if pms:
            o, e = unreal.SystemLibrary.get_component_bounds(pms[0])[:2]
            caz = math.degrees(math.atan2(o.y, o.x)) % 360.0
            ok = ok and abs(caz - 128.0) < 1.0 and abs(o.z + 45.15) < 0.05 and e.z < 0.01 and 1100.0 < math.hypot(o.x, o.y) < 1250.0
            where = "中心方位 %.1f、离殿心 %.0f cm、水面高 %.2f（水池的水面 −45）" % (caz, math.hypot(o.x, o.y), o.z)
        check(ok, "瀑布底下的水槽里补了一片水面，用的是水池的材质：%s" % where)

    # ── 颜色、柱子、海面 ──
    if on("look"):
        s = r8()
        check(s["floor"] == "MI_MATCH_WarmWhite" and s["floorsLikeWalls"], "殿里各层的地面和墙一个材质（%s）" % s["floor"])
        cmd("Dysis.FloorTone 0"); yield from wait(3)
        back = r8()["floor"]
        cmd("Dysis.FloorTone 1"); yield from wait(3)
        check(back == "MI_MATCH_InteriorFloor" and r8()["floor"] == "MI_MATCH_WarmWhite", "Dysis.FloorTone 0 能切回原来偏紫的灰（%s），1 切回来" % back)
        thick = [c.get_material(i).get_name() for a in tagged("SM_Mech_Twins_ThickWall") for c in a.get_components_by_class(unreal.StaticMeshComponent) for i in range(c.get_num_materials()) if c.get_material(i)]
        check("MI_MATCH_Sand" not in thick and "MI_MATCH_WarmWhite" in thick, "双子那一段加厚的墙也是墙的颜色（%s）" % sorted(set(thick)))
        c = s["column"]
        check(s["columns"] == 28 and c and abs(c["axisR"] - 1300.0) < 0.5 and abs(c["skinR"] - 1313.5) < 0.6 and abs(c["scale"] - 0.86) < 0.005,
              "殿里 %d 根柱子画出来的那一层收细到 %.2f、往墙那边挪了 %.1f cm（原模型的轴还在 %.0f）" % (s["columns"], c["scale"] if c else 0, (c["skinR"] - c["axisR"]) if c else 0, c["axisR"] if c else 0))
        # 挡人挡光的原模型没动：从墙那边平着打过去，二层 210° 那根柱子还是在 13.55 m 上挡住
        col = tagged("SM_Col_L1_210")
        h = unreal.SystemLibrary.line_trace_single(w, V(*polar(210.0, 1500.0, 1000.0)), V(*polar(210.0, 1250.0, 1000.0)), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [pawn], unreal.DrawDebugTrace.NONE, True)
        hit_r = math.hypot(h.to_tuple()[4].x, h.to_tuple()[4].y) if h else 0.0
        check(col and h is not None and h.to_tuple()[9] == col[0] and abs(hit_r - 1355.4) < 1.5, "柱子的碰撞没动（二层 210° 那根从墙那边打过去，在半径 %.1f cm 挡住；原来 1355.4）" % hit_r)
        check(not s["seaMirror"] and s["seaMaterial"] == "M_DysisSea", "海面默认还是原来那一版（%s）" % s["seaMaterial"])
        cmd("Dysis.SeaMirror 1"); yield from wait(3)
        m = r8()
        cmd("Dysis.SeaMirror 0"); yield from wait(3)
        check(m["seaMirror"] and m["seaMaterial"] == "M_DysisSeaMirror" and r8()["seaMaterial"] == "M_DysisSea", "Dysis.SeaMirror 1 换成镜湖那一版（%s），0 换回来" % m["seaMaterial"])

    sk = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
    anim = sk.get_anim_instance()
    def gait(): return json.loads(str(anim.describe_gait()))
    def push(follow):
        p = pawn.get_actor_location(); r = math.hypot(p.x, p.y); d = (-p.y / r, p.x / r)
        pawn.add_movement_input(V(d[0], d[1], 0.0), 1.0, False)
        if follow is not None: look(math.degrees(math.atan2(d[1], d[0])), follow)
    def walk_for(t, follow=None, each=None):
        t0 = now()
        while now() - t0 < t:
            push(follow)
            if each: each()
            yield

    # ── 走路 ──
    if on("walk"):
        go(polar(186.0, 1420.0, 0.0)); yield from seconds(0.8)
        yield from walk_for(1.5)
        S = {"rate": [], "stride": [], "twist": [], "arm": [], "drop": [], "off": [], "sp": []}
        def each():
            g = gait(); v = pawn.get_velocity()
            S["rate"].append(g["cycleRate"]); S["stride"].append(g["stride"]); S["twist"].append(g["upper"]["twist"]); S["arm"].append(g["upper"]["armDeg"][0])
            S["drop"].append(g["extraDrop"]); S["off"].append(-g["offset"][2]); S["sp"].append(math.hypot(v.x, v.y))
        yield from walk_for(3.0, each=each)
        g = gait()
        sp = sorted(S["sp"])[len(S["sp"]) // 2]
        check(abs(sp - 160.0) < 6.0 and g["clip"] == "A_Walk" and abs(g["cycleRate"] - 1.084) < 0.02 and abs(g["stride"] - 1.265) < 0.03,
              "走 %.0f cm/s：每秒 %.3f 个循环，步幅 ×%.3f（一步 74 cm；试过 195 / 一步 90 cm，用户说步子太大）" % (sp, g["cycleRate"], g["stride"]))
        check(max(S["drop"]) < 4.0 and 3.0 < min(S["off"]) and max(S["off"]) < 7.6,
              "走的时候人矮 %.1f…%.1f cm（两脚分得最开时再矮 %.1f cm），不是蹲着走" % (min(S["off"]), max(S["off"]), max(S["drop"])))
        check(g["upper"]["k"] > 0.99 and g["upper"]["armGain"] > 1.5 and max(S["arm"]) - min(S["arm"]) > 14.0 and max(S["twist"]) - min(S["twist"]) > 5.0,
              "上半身在动：手臂的摆幅放大到 %.2f 倍（另外多摆 %.0f…%.0f°），肩膀跟着拧 %.1f…%.1f°" % (g["upper"]["armGain"], min(S["arm"]), max(S["arm"]), min(S["twist"]), max(S["twist"])))
        yield from seconds(0.8)

    # ── 上台阶 ──
    if on("stairs"):
        for a in tagged_pfx("SM_Mech_StairSeal_TR") + tagged_pfx("SM_Mech_StairWindows_TR"): a.set_actor_hidden_in_game(True)
        z0 = 600.0 + (124.0 - 110.0) / 71.0 * 850.0
        go(polar(124.0, 1630.0, z0 + 10.0)); yield from seconds(0.8)
        yield from walk_for(1.0, follow=8.0)
        S = {"clip": [], "arm": [], "k": [], "slope": [], "stride": [], "rate": []}
        def each():
            g = gait()
            S["clip"].append(g["clip"]); S["arm"].append(g["upper"]["armDeg"][0]); S["k"].append(g["upper"]["k"]); S["slope"].append(g["slope"]); S["stride"].append(g["stride"]); S["rate"].append(g["cycleRate"])
        yield from walk_for(2.5, follow=8.0, each=each)
        up = sum(1 for c in S["clip"] if c == "A_Walk"); sl = sorted(S["slope"])[len(S["slope"]) // 2]
        check(up == len(S["clip"]) and 0.3 < sl < 0.5, "上楼梯：腿用的是走路的动作（%d / %d 帧；不再换成美术“上台阶”那一段），量到脚下的坡度 %.2f（楼梯是 0.42）" % (up, len(S["clip"]), sl))
        mid = lambda v: sorted(v)[len(v) // 2]
        check(abs(mid(S["stride"]) - 1.265 * 0.85) < 0.07, "坡上步子收小一点：步幅 ×%.2f（平地 1.27），每秒 %.2f 个循环" % (mid(S["stride"]), mid(S["rate"])))
        check(min(S["k"]) > 0.9 and max(S["arm"]) - min(S["arm"]) > 10.0, "上楼梯时手臂在前后摆（另外多摆 %.0f…%.0f°），不是架在胸前不动" % (min(S["arm"]), max(S["arm"])))
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
_r8 = R(); _r8.h = unreal.register_slate_post_tick_callback(_r8.tick)
