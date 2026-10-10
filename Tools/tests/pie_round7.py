# 第七轮反馈（2026-10-10）的测试（UE 编辑器 Python，开 PIE 跑）：
#   暗门和楼梯窗补了拱、栏杆上跳不出去、岛边拦得住、柱廊两侧的台阶接进圆基座、月桥口补了石栏杆、
#   多余的铜杆不画了、虹之龛前的石沿和窗一样宽、瀑布开闸以后一直有水声。
# 想只跑其中几段：把段名写在 Saved/round7_only.txt（逗号隔开：parts,steps,edge,jump,fall,sill,apple）。
# 结果写在 项目/Saved/pie_round7.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_round7.txt"
ONLY = [x.strip() for x in open(SAVED + "round7_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "round7_only.txt") else []
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
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]
AX = math.radians(22.5)      # 柱廊朝这个方位；along(离殿心多远, 往旁边多远, 高)
def along(d, side, z): return [d * math.cos(AX) - side * math.sin(AX), d * math.sin(AX) + side * math.cos(AX), z]

# 各层没有高护栏的那几段（光路从栏杆上越过去的地方）：第几层、方位从哪到哪
ARCS = [(1, 85.5, 104.5), (1, 141.0, 156.5), (1, 334.5, 344.5), (2, 72.5, 86.0), (2, 142.0, 161.5), (2, 257.0, 270.5), (2, 281.0, 284.0), (3, 30.5, 46.0), (3, 111.0, 115.0), (3, 198.5, 207.5)]
FZ = [0.0, 600.0, 1450.0, 2300.0]

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
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    def cmd(s): unreal.SystemLibrary.execute_console_command(w, s)
    def now(): return unreal.GameplayStatics.get_time_seconds(w)
    def seconds(t):
        t0 = now()
        while now() - t0 < t: yield
    def det(): return json.loads(str(director.describe_details()))
    def foot():
        p = pawn.get_actor_location(); return [p.x, p.y, p.z - hh]
    def zone(): return str(tc.describe_state()).split("zone=")[1].split(" ")[0]
    def go(p, dz=6.0): cmd("Dysis.Go %.2f %.2f %.2f" % (p[0], p[1], p[2] + dz))
    def shoot(tag):
        before = shots()
        cmd("Shot")
        for _ in range(40):
            yield
            new = sorted(shots() - before)
            if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
        say("截图 %s 没生成" % tag)
    def view(tag, eye, at, fov=60.0, frames=22):
        director.debug_camera(unreal.Vector(*eye), unreal.Vector(*at), fov)
        yield from wait(frames)
        yield from shoot(tag)
        director.debug_camera_off()
    seen = set()
    def push(vec, t, jump=True):
        # 朝一个方向一边跑一边跳，记下一路踩过哪些区域
        t0 = now(); first = True
        while now() - t0 < t:
            pawn.add_movement_input(vec, 1.0, False)
            if first and jump: pawn.jump(); first = False
            seen.add(zone())
            yield

    # ── 添上去的这些东西都在 ──
    if on("parts"):
        d = det()
        say("小修补：" + json.dumps(d, ensure_ascii=False))
        check(d["arches"] == 9, "暗门 4 个、楼梯的窗 5 扇都补了拱（%d 个）" % d["arches"])
        check(d["guards"] >= 50 and d["guardsOn"] == d["guards"], "栏杆上的护栏 %d 段，人站在地上时全都拦着（%d 段）" % (d["guards"], d["guardsOn"]))
        check(d["edgeWalls"] >= 90, "岛边、台基边的护栏 %d 段" % d["edgeWalls"])
        check(d["gapSlab"] == 3 and d["gapSlabShown"] == 3, "月桥口补的石栏杆在（%d 块）" % d["gapSlabShown"])
        check(d["leverChainHidden"], "二层拉杆旁边通到三层的那根铜杆不画了")
        chain = unreal.GameplayStatics.get_all_actors_with_tag(w, "SM_Mech_Twins_Chain")
        check(len(chain) == 1 and not chain[0].static_mesh_component.is_visible(), "月桥口那根横着的铜杆不画了（换成了石栏杆）")
        # 人踩在光上的时候护栏让开；回到地上又拦着
        tc.set_zone_override("beam:b2"); yield from wait(4)
        off = det()["guardsOn"]
        tc.clear_zone_override(); yield from wait(4)
        check(off == 0 and det()["guardsOn"] == d["guards"], "人踩在光上（区域 beam:…）时护栏不在（%d 段开着），回到地上又都立起来" % off)
        yield from view("月桥口的石栏杆", polar(262.0, 950.0, 800.0), polar(256.8, 1233.0, 650.0), 50.0)

    # ── 柱廊两侧的台阶：一路接进圆基座，中间没有断头 ──
    if on("steps"):
        # 踏面只有 40 cm 宽，人的身子比它宽（站上去会被上一级的棱架住），所以直接从上往下量地面有多高（按挡镜头的那一档量：台阶挡人、挡镜头，不挡光）
        def ground(dd, s):
            h = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(*along(dd, s, 30.0)), unreal.Vector(*along(dd, s, -250.0)), unreal.TraceTypeQuery.TRACE_TYPE_QUERY2, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            return -999.0 if h is None else h.to_tuple()[4].z
        worst = 0.0; note = []
        for side in (1.0, -1.0):
            for lvl, s in ((-40.0, 1510.0), (-80.0, 1550.0)):
                for dd in (2600.0, 2200.0, 1900.0, 1860.0, 1840.0, 1820.0, 1800.0, 1780.0):
                    z = ground(dd, side * s)
                    if z > lvl + 1.0: continue            # 已经走进圆基座更高的那一级里了
                    worst = max(worst, abs(z - lvl))
                    if abs(z - lvl) > 1.0: note.append("%s侧 第 %.0f 级 离殿心 %.0f：地面在 %.1f" % ("东" if side > 0 else "西", -lvl / 40.0, dd, z))
            yield
        check(not note, "柱廊两侧的两级台阶从门廊一直接进圆基座：每一级上从离殿心 26 m 到 17.8 m 地面都在这一级的高度上（最多差 %.1f cm）%s" % (worst, "；" + "；".join(note[:4]) if note else ""))
        yield from view("柱廊台阶接圆基座", along(2350.0, 2250.0, 140.0), along(1930.0, 1520.0, -60.0), 45.0)

    # ── 岛边、台基边：跑着跳也出不去 ──
    if on("edge"):
        mv.set_editor_property("max_walk_speed", 520.0)
        res = []; ok = True
        for az in (70.0, 150.0, 250.0, 330.0):
            go(polar(az, 3000.0, -121.0)); yield from seconds(0.4)
            yield from push(unreal.Vector(math.cos(math.radians(az)), math.sin(math.radians(az)), 0.0), 2.0)
            f = foot(); r = math.hypot(f[0], f[1]); ok = ok and r < 3200.0 and f[2] > -140.0
            res.append("%.0f°→%.0f" % (az, r))
        for sd in (-1.0, 1.0):
            go(along(2900.0, sd * 1600.0, -121.0)); yield from seconds(0.4)
            yield from push(unreal.Vector(-math.sin(AX) * sd, math.cos(AX) * sd, 0.0), 2.0)
            f = foot(); s = abs(-f[0] * math.sin(AX) + f[1] * math.cos(AX)); ok = ok and s < 1800.0 and f[2] > -140.0
            res.append("台基%s侧→%.0f" % ("东" if sd > 0 else "西", s))
        mv.set_editor_property("max_walk_speed", 190.0)
        check(ok, "岛边、台基两侧朝外跑着跳，都被拦在边上（半径 32 m、台基侧边 17.75 m 以内）：%s" % "，".join(res))

    # ── 各层栏杆：光路越过去的那几段，站在回廊上跑着跳、走着跳都翻不过去 ──
    if on("jump"):
        out = []
        for fl, a0, a1 in ARCS:
            for az in ([a0 + 0.8, (a0 + a1) * 0.5, a1 - 0.8] if a1 - a0 > 4 else [(a0 + a1) * 0.5]):
                for run in (True, False):
                    go(polar(az, 1435.0 if run else 1385.0, FZ[fl])); yield from seconds(0.5)
                    mv.set_editor_property("max_walk_speed", 520.0 if run else 190.0)
                    seen.clear()
                    yield from push(unreal.Vector(-math.cos(math.radians(az)), -math.sin(math.radians(az)), 0.0), 2.0)
                    f = foot(); r = math.hypot(f[0], f[1])
                    if r < 1210.0 or f[2] < FZ[fl] - 60.0:
                        out.append("第 %d 层 %.1f° %s→半径 %.0f 高 %.0f，一路 %s" % (fl, az, "跑" if run else "走", r, f[2], sorted(seen)))
        mv.set_editor_property("max_walk_speed", 190.0)
        check(not out, "三层回廊 10 段没有高护栏的栏杆，跑着跳、走着跳都翻不进中庭%s" % ("" if not out else "——翻过去了：" + "；".join(out[:5])))

    # ── 瀑布：开闸以后水声一直在 ──
    if on("fall"):
        go(polar(100.0, 1300.0, 95.0)); yield from seconds(0.6)
        before = det()["loops"]
        cmd("Dysis.Sluice 1")
        yield from seconds(5.0)
        after = det()["loops"]
        check(not before["fallNear"] and after["fallNear"] and after["fallFar"] and after["mist"], "开闸前没有瀑布的水声；开闸 5 秒以后近、远两层水声和水雾都响着（%s）" % json.dumps(after))
        yield from seconds(8.0)
        check(det()["loops"]["fallNear"], "再过 8 秒水声还在（不是响一声就没了）")
        # 白沫的材质编不过的话会变成引擎的灰格子（头一版里有个变量叫 line，是 HLSL 的保留字）：编过了才有指令数
        mat = unreal.load_asset("/Game/Dysis/Fx/M_DysisFallFoam"); n = -1
        try: n = unreal.MaterialEditingLibrary.get_statistics(mat).num_pixel_shader_instructions
        except Exception as e: say("     量材质时出错：%s" % e)
        check(mat is not None and n > 0, "瀑布入水的白沫材质编得过（像素着色器 %d 条指令；编不过会是 0，画出来是灰格子）" % n)
        yield from view("瀑布入水", polar(128.0, 850.0, 120.0), polar(128.0, 1245.0, 0.0), 55.0, 40)
        # 镜头不进瀑布：人站在瀑布西头外面（差 3°），背对着瀑布看——镜头本该在人身后 4.6 m，正好落在瀑布那一块里面；
        # 现在水帘对镜头也是一堵墙，镜头被挡在外面。几个方向都转一遍。
        def cam_in_fall():
            c = pc.player_camera_manager.get_camera_location()
            caz = math.degrees(math.atan2(c.y, c.x)) % 360.0; cr = math.hypot(c.x, c.y)
            return (116.25 < caz < 139.75 and cr > 1245.0), caz, cr
        notes = []; worst = False
        for paz, pr, pz in ((113.0, 1400.0, 95.0), (143.0, 1400.0, 0.0), (113.5, 1300.0, 95.0)):
            go(polar(paz, pr, pz)); yield from seconds(0.6)
            for yaw in (23.0, 60.0, 100.0, 203.0, 250.0, 300.0, 340.0):
                pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=-8.0, yaw=yaw))
                yield from seconds(0.45)
                ins, caz, cr = cam_in_fall()
                worst = worst or ins
                if ins: notes.append("人在 %.0f° 半径 %.0f、朝 %.0f° 看：镜头在方位 %.1f 半径 %.0f" % (paz, pr, yaw, caz, cr))
        check(not worst, "人在瀑布外面，朝哪边转视角镜头都不进瀑布那一块%s" % ("" if not notes else "——进去了：" + "；".join(notes[:4])))

    # ── 虹之龛前的石沿：和窗一样宽、前沿是弧形 ──
    if on("sill"):
        G = json.load(open(PROJ + "Tools/greybox/golden/greybox_rainbow.json", encoding="utf-8"))
        C = G["consts"]; E = C["IRISREL"]["E"]
        go([E[0], E[1], C["F1"]])
        t0 = now()
        while now() - t0 < 8.0 and not json.loads(director.describe_rainbow())["done"]: yield
        yield from seconds(5.5)
        rb = json.loads(director.describe_rainbow())
        check(rb["done"] and rb["sillK"] == 1 and det()["ledgeParts"] >= 10, "浮雕解开以后石沿伸出来了（换成 %d 块拼的弧形石沿）" % det()["ledgeParts"])
        # 石沿的形状：从上往下量有没有石头（按挡镜头的那一档量——石沿什么都挡；虹桥那一排能踩的板只挡人，量不到，正好把两样分开。
        # 虹桥是斜着落过来的，桥尾的西半边盖在原来石沿往西多出来的那一截上面：站上去试的话分不清踩的是桥还是石头）
        def stone(az, r):
            p = polar(az, r, 0.0)
            h = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(p[0], p[1], 1800.0), unreal.Vector(p[0], p[1], 1600.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY2, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            return h is not None and abs(h.to_tuple()[4].z - 1750.0) < 1.5
        pts = (("窗的中线", 166.0, 1510.0, True), ("窗的西边沿里面", 163.2, 1530.0, True), ("窗的东边沿里面", 168.8, 1530.0, True),
               ("窗西边沿外面 0.4 m（原来的石沿盖着）", 161.2, 1520.0, False), ("原来往西多出来的那一头", 160.0, 1520.0, False), ("东边沿前面（弧收进去的地方）", 168.8, 1495.0, False))
        got = [(nm, stone(az, r), want) for nm, az, r, want in pts]
        check(all(g[1] == g[2] for g in got), "石沿和窗一样宽、前沿是弧形：" + "；".join("%s%s" % (g[0], "有石头" if g[1] else "没有") for g in got))
        res = []
        for nm, p in (("桥尾", G["flow"][3]["pts"][0]["foot"]), ("棱镜旁", G["flow"][5]["seq"][0]["foot"])):
            go(p); yield from seconds(0.7)
            f = foot()
            res.append((nm, abs(f[2] - 1752.0) < 6.0, zone(), str(director.debug_prompt_at(unreal.Vector(f[0], f[1], f[2])))))
        check(res[0][1] and res[1][1] and res[0][3] == "irisNiche" and res[1][2] == "sill", "站在虹桥落下来的那一点够得着虹之龛（提示 %s）；棱镜旁边站得住、算窗台（%s）" % (res[0][3], res[1][2]))
        yield from view("虹之龛前的石沿", polar(175.0, 1050.0, 1900.0), polar(165.0, 1545.0, 1780.0), 55.0)

    # ── 捧着金苹果的那一层声音：入夜（接住光、取下苹果）以后一直响着 ──
    if on("apple"):
        before = det()["loops"]["apple"]
        director.debug_set_night(True); yield from seconds(1.5)
        check(not before and det()["loops"]["apple"], "白天没有；接住光、取下金苹果以后，捧着苹果的那一层声音响着")
        director.debug_set_night(False); yield from seconds(2.6)
        check(not det()["loops"]["apple"], "回到白天（测试用）它就停了")
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
_r7 = R(); _r7.h = unreal.register_slate_post_tick_callback(_r7.tick)
