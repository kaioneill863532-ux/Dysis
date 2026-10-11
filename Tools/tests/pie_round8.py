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
        # （2026-10-11：塞勒涅、伊莉丝减面时护住了弯的棱线——弧不再是折线——面数多了一倍；拱和门板换成现做的素石拱）
        # （第九批：三相像、双子、瀑布后女神也护住了弯的棱线——衣褶细了、棱角没那么硬——面数是原来的两三倍；新加了天鹅女神像）
        want = {"TwinCastor": (8500, 12000), "TwinPollux": (8000, 11500), "Selene": (9000, 13500), "IrisRelief": (14000, 20000), "SwanRelief": (1500, 2600),
                "MirrorSun": (12000, 19000), "MirrorMoon": (12000, 19000), "MirrorDark": (12000, 19000), "FacadeStatue": (2400, 3200), "Goddess": (12500, 17500), "SwanGoddess": (8000, 11500),
                "GateArch": (700, 1000), "GateDoor": (350, 500), "NicheBox": (1800, 3400), "NicheLid": (600, 1200), "Apple": (200, 400)}
        got = {}
        for name in want:
            m = unreal.load_asset("/Game/Dysis/MechPack_20261010/SM_MP_" + name)
            got[name] = m.get_num_triangles(0) if m else -1
        off = [n for n in want if not (want[n][0] <= got[n] <= want[n][1])]
        check(not off, "机关模型的面数都在该在的范围里：%s%s" % (", ".join("%s %d" % (n, got[n]) for n in sorted(got)), ("；不对的：%s" % off) if off else ""))
        # 浮雕没有背板：最凸的地方离墙面十几厘米，整件的底就是墙面（Y 从 0 起）
        for name, deep in (("Selene", 30.0), ("IrisRelief", 16.5), ("SwanRelief", 18.0)):
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
                old = [c for c in comps if c.static_mesh and not c.static_mesh.get_name().startswith("SM_MP_") and c.static_mesh.get_name() != "Cylinder" and c.is_visible()]      # Cylinder = 女神像脚下另放的圆台
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
        # （第九批：脚下两层的圆台换成一层小的——半径 70、高 20；像矮了 20 cm，放大到 1.567 倍，镜子还在离地 1.85 m）
        mirror_z = fo.z + 105.7 * form.get_world_scale().x - 1450.0
        check(abs(form.get_world_scale().x - 1.567) < 0.01 and off < 0.5 and abs(pl.get_actor_scale3d().x - 1.0) < 0.01 and abs(pe.x - 70.0) < 3.0 and len(plv) == 1 and plv[0].static_mesh.get_name() == "Cylinder"
              and abs(pe.z * 2.0 - 20.0) < 1.0 and abs(fo.z - (po.z + pe.z)) < 1.0 and gap > 5.0 and abs(mirror_z - 185.0) < 3.0,
              "三相像放大到 %.2f 倍、站在底座正中（偏 %.1f cm）、脚踩在底座面上；圆底座只有一层：半径 %.0f cm、高 %.0f cm，手轮离它 %.0f cm；镜子的中心离地 %.0f cm（灰盒 185）" % (form.get_world_scale().x, off, pe.x, pe.z * 2.0, gap, mirror_z))
        # 日之龛：换回原来那个盒子，但是空心的（四面壁 + 底，五块）；盖子单装（朝着人掀）、关着的时候是平的；碎片在盒子里面、正中
        sbox = tagged("SM_Mech_SunNiche_Box")[0]
        walls = [c for c in sbox.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "Cube" and c.is_visible()]
        rest = [c.static_mesh.get_name() for c in sbox.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() != "Cube" and c.is_visible()]
        lidc = [n for n in rest if "Lid" in n]; solid = [n for n in rest if "Lid" not in n]
        los = [bounds(c) for c in walls]
        bx0 = min(o.x - e.x for o, e in los); bx1 = max(o.x + e.x for o, e in los); by0 = min(o.y - e.y for o, e in los); by1 = max(o.y + e.y for o, e in los)
        bz0 = min(o.z - e.z for o, e in los); bz1 = max(o.z + e.z for o, e in los); sb = mpj["sunShardBase"]
        check(len(walls) == 5 and len(lidc) == 1 and not solid and mpj["lidModel"] and mpj["lidTiltDeg"] < 1.0 and math.hypot(sb[0] - (bx0 + bx1) * 0.5, sb[1] - (by0 + by1) * 0.5) < 2.0 and bz0 + 4.0 < sb[2] < bz1,
              "日之龛：原来的盒子换成空心的（%d 块壁板），盖子关着是平的（%.1f°）；碎片在盒子里面正中（离中心 %.1f cm，比盒底高 %.0f cm）" % (len(walls), mpj["lidTiltDeg"], math.hypot(sb[0] - (bx0 + bx1) * 0.5, sb[1] - (by0 + by1) * 0.5), sb[2] - bz0))
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
        # 瀑布后面的女神像：挪到瀑布正中（人的轴线离正中 128° 不到 1°）、站低了；脚下是一个圆台（原来的方墩子不画）
        gm = [c for c in tagged("SM_Mech_Goddess_Statue")[0].get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "SM_MP_Goddess"]
        gped = [c for c in tagged("SM_Mech_Goddess_Statue")[0].get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "Cylinder" and c.is_visible()]
        gl = gm[0].get_world_location() if gm else unreal.Vector(); gaz = math.degrees(math.atan2(gl.y, gl.x)) % 360.0
        gpl = [c for a in tagged("SM_Mech_Goddess_Plinth") for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
        # （第十批：上一版放低以后膝盖以下埋在瀑布后面 90 cm 高的石台里。现在缩到 0.68 倍、站回石台上：脚下是三级圆台，台座底在石台面上）
        glow = min((c.get_world_location().z - c.get_world_scale().z * 50.0) for c in gped) if gped else -999.0
        check(len(gm) == 1 and abs(gaz - 128.0) < 0.5 and 1430.0 < math.hypot(gl.x, gl.y) < 1460.0 and abs(gm[0].get_world_scale().x - 0.68) < 0.01 and len(gped) == 3 and 84.0 < glow < 91.0
              and abs(gl.z - (glow + 58.5 * 0.68)) < 1.0 and not gpl,
              "瀑布后面的女神像：人的轴线在方位 %.2f°（瀑布正中 128°）、离殿心 %.0f cm，缩到 %.2f 倍；脚下三级圆台，台座底在高 %.0f（石台面 90），脚在 %.0f；原来的方墩子不画了" % (gaz, math.hypot(gl.x, gl.y), gm[0].get_world_scale().x if gm else 0, glow, gl.z))
        # 三相像的手轮：轮子另挂了一份会转的（原件上轮子那一块不画）
        cw = [c for c in crank.get_components_by_class(unreal.StaticMeshComponent) if c.get_attach_parent() and c.get_attach_parent().get_owner() == crank and c.static_mesh and c.static_mesh.get_name() == "SM_Kit_Mech_Mirror_Crank"]
        check(len(cw) == 1, "三相像的手轮：有一份单独会转的轮子（%d 份；转像的时候它绕轮轴转）" % len(cw))
        # 虹之龛做出了进深：门在柜子的前口上、碎片在柜子里面
        # 虹之龛：深色的背板不画了，两扇门贴在墙面上，碎片沉在墙里（门开了才出来）。石沿还缩在墙里的时候它们都比伸出来以后靠外 90 cm
        plate = [c for c in tagged("SM_Mech_Sill_Niche")[0].get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
        sx, sy = 90.0 * math.cos(math.radians(166.0)), 90.0 * math.sin(math.radians(166.0))
        nr = {t: math.hypot(tagged(t)[0].get_actor_location().x - sx, tagged(t)[0].get_actor_location().y - sy) for t in ("SM_Mech_Sill_NicheDoorL", "SM_Mech_Sill_NicheShard")}
        check(not plate and abs(nr["SM_Mech_Sill_NicheDoorL"] - 1548.4) < 0.6 and abs(nr["SM_Mech_Sill_NicheShard"] - (1536.0 + 28.0)) < 0.6,
              "虹之龛：背板不画了，两扇门贴在墙面上（离殿心 %.1f cm，墙面 1550），碎片沉在墙里（%.0f cm），门开了才出来" % (nr["SM_Mech_Sill_NicheDoorL"], nr["SM_Mech_Sill_NicheShard"]))
        # 波吕丢刻斯还封在墙里：新模型不画
        pol = named("SM_Mech_Twins_Pollux", "SM_MP_TwinPollux")
        check(len(pol) == 1 and not pol[0].is_visible(), "波吕丢刻斯还封在墙里的时候，他的新模型不画（墙化开了才画）")

        # ── 2026-10-11 这一批 ──
        dcomps = [c for c in director.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
        def dmesh(name): return [c for c in dcomps if c.static_mesh.get_name() == name]
        def raz(v): return math.hypot(v.x, v.y), math.degrees(math.atan2(v.y, v.x)) % 360.0
        # 月牙：现做的圆滑月牙（七百来个面；美术那枚只有 84 个大平面）
        cres = unreal.load_asset("/Game/Dysis/MechPack_20261010/SM_MP_MoonGateCrescent")
        check(cres.get_num_triangles(0) > 600, "月门上的月牙换成了弧线圆滑的（%d 个面）" % cres.get_num_triangles(0))
        # 日之龛：盒子矮到 32 cm
        check(abs((bz1 - bz0) - 32.0) < 1.0, "日之龛的盒子高 %.0f cm（原来 50）" % (bz1 - bz0))
        # 拉杆：没扳的时候杆贴着墙竖直朝上
        arm = [c for c in dmesh("SM_MP_LeverArm") if abs(raz(c.get_world_location())[1] - 328.5) < 3.0]
        check(len(arm) == 1 and arm[0].get_up_vector().z > 0.98, "拉杆没扳的时候竖直朝上（杆的方向 z = %.2f）" % (arm[0].get_up_vector().z if arm else -9))
        # 水闸的开关也换成了拉杆：水没开的时候杆立着；原来那根小铜柱不画了
        sarm = [c for c in dmesh("SM_MP_LeverArm") if abs(raz(c.get_world_location())[1] - 113.8) < 3.0]
        sbase = [c for c in dmesh("SM_MP_LeverBase") if abs(raz(c.get_world_location())[1] - 113.8) < 3.0]
        marker = [c for c in dcomps if c.get_name() == "SluiceMarker"]
        check(len(sarm) == 1 and len(sbase) == 1 and sarm[0].get_up_vector().z > 0.98 and marker and not marker[0].is_visible(),
              "水闸的开关是一根拉杆，水没开的时候立着（杆的方向 z = %.2f）；原来的小铜柱不画了" % (sarm[0].get_up_vector().z if sarm else -9))
        # 天鹅女神像：换上了美术的人形像，原来那个圆锥不画
        sgc = tagged("SM_Mech_Swan_Goddess")[0].get_components_by_class(unreal.StaticMeshComponent)
        check(any(c.static_mesh and c.static_mesh.get_name() == "SM_MP_SwanGoddess" and c.is_visible() for c in sgc) and not any(c.static_mesh and c.static_mesh.get_name().startswith("SM_Kit_") and c.is_visible() for c in sgc),
              "四层的天鹅女神像换上了新模型（变成天鹅以前的人形）")
        # 波吕丢刻斯缩到 0.9；墙上给他的龛挖开了（第二个洞）
        check(abs(pol[0].get_world_scale().x - 0.9) < 0.01, "波吕丢刻斯缩到 %.2f 倍（龛只有 1.36 m 宽）" % pol[0].get_world_scale().x)
        # 桥门：换成素石拱（不放大），整个在叶片盘的铜边（离殿心 11.18 m）外面；金苹果那里的虹门不画、不挡人
        ga = dmesh("SM_MP_GateArch"); gr, _ = raz(ga[0].get_world_location()) if ga else (0.0, 0.0)
        arch = tagged("SM_Mech_RainbowArch_Arch")[0]
        arch_seen = [c for c in arch.get_components_by_class(unreal.StaticMeshComponent) if c.is_visible()]
        arch_blocks = [c for c in arch.get_components_by_class(unreal.PrimitiveComponent) if c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN) != unreal.CollisionResponseType.ECR_IGNORE]
        check(len(ga) == 1 and abs(ga[0].get_world_scale().x - 1.0) < 0.01 and gr - 17.0 > 1125.0 and not arch_seen and not arch_blocks,
              "桥门是素石拱（拱心离殿心 %.0f cm，最靠里的面 %.0f，叶片盘的铜边到 1118）；金苹果那里的虹门不画、不挡人" % (gr, gr - 17.0))
        # 两座浑天仪：环和轴换成不带球的；殿顶那座在最高一级的正中（方位 34.33°、半径 13.225 m）；水亭那座的太阳和金苹果一样大（14 cm），月托是薄碟子
        at = tagged("SM_Mech_Armillary_Top")[0]; ap_ = tagged("SM_Mech_Armillary_Pavilion")[0]
        old_arm = [c for a in (at, ap_) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith("SM_Kit_") and c.is_visible()]
        new_arm = [c for a in (at, ap_) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith("SM_MP_Arm")]
        tr, ta = raz(at.get_actor_location())
        bead = [c for c in dcomps if c.static_mesh.get_name() == "Sphere" and abs(c.get_world_scale().x - 0.14) < 0.005]
        hold = dmesh("SM_MP_MoonHolder")
        bd = bead[0].get_world_location() if bead else unreal.Vector(); hd = hold[0].get_world_location() if hold else unreal.Vector()
        check(not old_arm and len(new_arm) == 6 and abs(ta - 34.333) < 0.05 and abs(tr - 1322.5) < 1.0 and len(bead) == 1 and abs(math.dist((bd.x, bd.y, bd.z), (0.0, 0.0, 180.0)) - 52.0) < 0.5
              and len(hold) == 1 and abs(math.dist((hd.x, hd.y, hd.z + 4.2), (0.0, 0.0, 180.0)) - 47.84) < 0.5,
              "浑天仪：环上不再焊着球（新环 %d 件）；殿顶那座在方位 %.2f°、半径 %.0f cm（最高一级的正中）；水亭那座的太阳直径 14 cm、在 52 cm 的圈上，月托是薄碟子、在 47.8 cm 的圈上" % (len(new_arm), ta, tr))
        # 金苹果就是殿顶浑天仪的太阳：在浑天仪中心朝着太阳的方向、55 cm 的圈上
        al = ap[0].get_world_location() if ap else unreal.Vector(); ac = at.get_actor_location()
        check(abs(math.dist((al.x, al.y, al.z), (ac.x, ac.y, ac.z)) - 55.0) < 1.0, "金苹果在殿顶浑天仪的环上（离中心 %.0f cm）——它就是那座浑天仪的太阳" % math.dist((al.x, al.y, al.z), (ac.x, ac.y, ac.z)))
        # 天鹅像：整组往墙那边挪了 45 cm（台座中心离殿心 13.4 m，栏杆在 12.5 m）
        sp = tagged("SM_Mech_Swan_Plinth")[0].get_actor_location(); sw = tagged("SM_Mech_Swan_Swan")[0].get_actor_location(); sg = tagged("SM_Mech_Swan_Goddess")[0].get_actor_location()
        check(abs(math.hypot(sp.x, sp.y) - 1340.0) < 1.0 and abs(math.hypot(sw.x, sw.y) - 1340.0) < 1.0 and abs(math.hypot(sg.x, sg.y) - 1340.0) < 1.0 and 2325.0 < sg.z < 2340.0,
              "天鹅像往墙那边挪了：台座、女神像、天鹅都在离殿心 %.0f cm（原来 1295），顺着月光矮了 %.0f cm" % (math.hypot(sp.x, sp.y), 2350.0 - sg.z))
        # 二层外立面：九尊神像（单数开间；正门上面那一间、开着楼梯窗的那一间、窗洞开着的 172.5° 那一间不放），缩到 0.7、悬在盲拱正中
        l1 = [c for c in dmesh("SM_MP_FacadeStatue") if 900.0 < c.get_world_location().z < 960.0]
        l1az = sorted(round(raz(c.get_world_location())[1], 1) for c in l1)
        check(l1az == [52.5, 82.5, 112.5, 202.5, 232.5, 262.5, 292.5, 322.5, 352.5] and all(abs(c.get_world_scale().x - 0.70) < 0.01 for c in l1), "二层外立面放了 %d 尊神像：方位 %s" % (len(l1), l1az))
        # 虹之龛：墙换成了能挖洞的材质，龛里的五块板先藏着（石沿伸到头、门到位了洞才开）
        wall = [c for a in tagged("SM_Wall") for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "LP_SM_Wall"]
        cut = [m for c in wall for m in c.get_materials() if m and m.get_base_material().get_name() == "M_DysisWallCut"]
        rec = [c for c in dcomps if c.static_mesh.get_name() == "Cube" and abs(raz(c.get_world_location())[1] - 161.07) < 1.5 and 1830.0 < c.get_world_location().z < 1910.0 and raz(c.get_world_location())[0] > 1549.0]
        check(len(wall) == 1 and len(cut) == wall[0].get_num_materials() and len(rec) == 5 and not any(c.is_visible() for c in rec),
              "虹之龛：墙用上了能挖洞的材质（%d 个材质段），龛里的五块板备着、先不画（%d 块）" % (len(cut), len(rec)))
        # 外立面神像：没有底座、往墙里收
        fo2, fe2 = bounds(named("SM_Facade_Statue_L3_01", "SM_MP_FacadeStatue")[0])
        # （第九批：再缩小一点、放在盲拱的正中——四层的缩到 0.6，脚离窗台 105 cm，脚下垫一块托石）
        fst = named("SM_Facade_Statue_L3_01", "SM_MP_FacadeStatue")[0]
        stone = [c for c in tagged("SM_Facade_Statue_L3_01")[0].get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "Cube" and c.is_visible()]
        check(math.hypot(fo2.x, fo2.y) < 1745.0 and abs(fst.get_world_scale().x - 0.60) < 0.01 and abs((fo2.z - fe2.z) - 2540.2) < 2.0 and len(stone) == 1,
              "外立面神像往墙里收了（中心离殿心 %.0f cm；原来 1757）、缩到 %.2f，悬在盲拱正中（脚在 %.0f，窗台 2435），脚下一块托石" % (math.hypot(fo2.x, fo2.y), fst.get_world_scale().x, fo2.z - fe2.z))
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
        # （2026-10-11 用户定了：海面用镜湖；M 键 / Dysis.SeaMirror 0 还能换回原来的）
        check(s["seaMirror"] and s["seaMaterial"] == "M_DysisSeaMirror", "海面默认是镜湖那一版（%s）" % s["seaMaterial"])
        cmd("Dysis.SeaMirror 0"); yield from wait(3)
        m = r8()
        cmd("Dysis.SeaMirror 1"); yield from wait(3)
        check(not m["seaMirror"] and m["seaMaterial"] == "M_DysisSea" and r8()["seaMaterial"] == "M_DysisSeaMirror", "Dysis.SeaMirror 0 换回原来那一版（%s），1 再换成镜湖" % m["seaMaterial"])

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
