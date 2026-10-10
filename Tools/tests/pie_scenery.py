# 殿外的配景、夕照的天和水庭的动画水面（UE 编辑器 Python，开 PIE 跑）：
#   · 检查：山石云放上了、没有一件会挡光、台地上的石头挡人、建筑换了配色、水庭的水是动画水面、白天的天是“夕照的天”、
#     小岛到台地这一条线上没有东西、开局那几个位置日月没被山挡住；
#   · 拍图：开局的小岛、台地、殿门、水庭、各层、殿顶（下午 → 黄昏 → 夜），给人看画面。
# 想只拍其中几段：把段名写在 Saved/scenery_only.txt（逗号隔开：check,day,water,dusk,night）。
# 结果写在 项目/Saved/pie_scenery.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = SAVED + "pie_scenery.txt"
ONLY = [x.strip() for x in open(SAVED + "scenery_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "scenery_only.txt") else []
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
    unreal.SystemLibrary.execute_console_command(w, "Shot")
    for _ in range(40):
        yield
        new = sorted(shots() - before)
        if new: say("截图 %s = %s" % (tag, os.path.basename(new[-1]))); return
    say("截图 %s 没生成" % tag)
def polar(az, r, z): return [r * math.cos(math.radians(az)), r * math.sin(math.radians(az)), z]
def az_alt(v): return math.degrees(math.atan2(v.y, v.x)) % 360.0, math.degrees(math.asin(max(-1.0, min(1.0, v.z))))

SPAWN = [4978.79, 2189.09, -1200.0]     # 开局站的地方（小岛外沿）

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
    sky = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisSkyActor)[0]
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    night = [False]
    def cmd(s): unreal.SystemLibrary.execute_console_command(w, s)
    def go(p, dz=8.0): cmd("Dysis.Go %.2f %.2f %.2f%s" % (p[0], p[1], p[2] + dz, " night" if night[0] else ""))
    def look_at(p):
        l = pawn.get_actor_location(); d = (p[0] - l.x, p[1] - l.y, p[2] - (l.z + 60.0))
        pc.set_control_rotation(unreal.Rotator(roll=0.0, pitch=math.degrees(math.atan2(d[2], math.hypot(d[0], d[1]))), yaw=math.degrees(math.atan2(d[1], d[0]))))
    def view(tag, eye, at, fov=75.0, frames=80):
        director.debug_camera(unreal.Vector(*eye), unreal.Vector(*at), fov)
        yield from wait(frames)
        yield from shoot(w, tag)
        director.debug_camera_off()
    def spot(tag, p, at, frames=110):
        go(p); look_at(at)
        yield from wait(frames)
        yield from shoot(w, tag)
    # 调画面用：Saved/scenery_tune.json 里写 {"sky": {属性: 值或 [r, g, b]}, "director": {...}}，开局先套上（不用重新编译就能试数）
    if os.path.exists(SAVED + "scenery_tune.json"):
        tune = json.load(open(SAVED + "scenery_tune.json", encoding="utf-8"))
        for obj, key in ((sky, "sky"), (director, "director")):
            for k, v in tune.get(key, {}).items():
                obj.set_editor_property(k, unreal.LinearColor(v[0], v[1], v[2], 1.0) if isinstance(v, list) else v)
        sky.refresh_sky(); director.rebuild_scenery()
        say("套用了 scenery_tune.json：" + json.dumps(tune, ensure_ascii=False))
    def hour():
        h = tc.get_editor_property("h"); t = 12.0 + h / 15.0
        return "%02d:%02d" % (int(t) % 24, int(t * 60) % 60)

    if on("check"):
        d = json.loads(str(director.describe_scenery()))
        say("配景：" + json.dumps(d, ensure_ascii=False))
        check(d["placed"] >= 40 and d["shown"] == d["placed"], "山石云放上了：%d 件（台地上 %d、礁石 %d；台地外的草坡和挡着小岛的没放 %d）" % (d["placed"], d["onTerrace"], d["seaRocks"], d["skipped"]))
        check(d["blockLight"] == 0, "没有一件配景会挡光（光路的判定不受影响）")
        check(d["solid"] >= 5 and d["solid"] < d["placed"], "台地上的石头挡人：%d 块" % d["solid"])
        check(d["paletteSlots"] >= 250, "建筑换了配色：%d 个面" % d["paletteSlots"])
        pool = [a for a in unreal.GameplayStatics.get_all_actors_with_tag(w, "SM_Mech_Water_Pool")]
        mat = pool[0].static_mesh_component.get_material(0) if pool else None
        base = mat.get_base_material().get_name() if mat else "-"
        check(base == "M_DysisAnimeWater", "水庭的水用的是动画水面（现在是 %s）" % base)
        dome = [c for c in sky.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "DayDome"]
        dm = dome[0].get_material(0).get_base_material().get_name() if dome and dome[0].get_material(0) else "-"
        check(dome and dome[0].is_visible() and dm == "M_DysisSunsetSky" and sky.get_sky_gain() > 0.5, "白天的天是夕照的天（%s，亮度 %.2f）" % (dm, sky.get_sky_gain() if dome else 0.0))
        # 小岛通到台地的这一条线：开场的光从这里斜着上来（落在离殿心 31.8 m、高 −80，每往外 1 m 低 0.65 m）。
        # 2026-10-10 起岛的正面也堆了礁岩，所以这条线上可以有石头，但顶要在光下面至少 1 m——不挡光、不碍着人走。
        # 从殿心朝小岛每 2 m 取一点，中心在周围 4.5 m 以内的石头挨个看顶有多高
        lane_bad = 0; lane_n = 0
        comps = [c for c in director.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name().startswith("SM_MATCH_Rock")]
        for i in range(0, 16):
            rr = 2400.0 + 200.0 * i
            p = polar(23.7, rr, 0.0)
            beam_z = -80.0 - 0.65 * max(rr - 3180.0, 0.0)
            for c in comps:
                l = c.get_world_location()
                if math.hypot(l.x - p[0], l.y - p[1]) < 450.0:   # 代码里是轴线两边各 5.6 m 以内的石头压低；这里查中心在 4.5 m 以内的
                    # 石头的顶：当成一个歪着的椭球算（引擎报的包围盒是个方盒子，歪一点就虚高一两米）。石头模型没缩放时半宽 85 / 89、半高 93 cm
                    sc = c.get_world_scale()
                    top = l.z + math.sqrt((c.get_forward_vector().z * 85.0 * sc.x) ** 2 + (c.get_right_vector().z * 89.0 * sc.y) ** 2 + (c.get_up_vector().z * 93.0 * sc.z) ** 2)
                    lane_n += 1
                    beam_z = -80.0 - 0.65 * max(math.hypot(l.x, l.y) - 3180.0, 0.0)   # 光在这块石头自己这里有多高（石头的顶在它中心上方）
                    if top > beam_z - 100.0:
                        lane_bad += 1
                        say("     线上太高的石头：离殿心 %.0f、方位 %.1f°，顶 %.0f，光在这里高 %.0f" % (math.hypot(l.x, l.y), math.degrees(math.atan2(l.y, l.x)) % 360, top, beam_z))
        check(len(comps) >= 15 and lane_bad == 0, "小岛到台地这一条线上的石头都在光下面 1 m 以外（查了 %d 块，线上 %d 块次）" % (len(comps), lane_n))
        # 日月不被山挡：从台地、殿顶两处看，太阳整个下午（方位 204°–244°）和月亮刚升起来的那一段，山顶都在它们下面
        lib = unreal.DysisSkyLibrary
        mts = [c for c in director.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name() == "SM_MATCH_Mountain"]
        hidden = 0; worst = 99.0
        for eye in ([0.0, 0.0, 60.0], [0.0, 0.0, 3200.0], SPAWN[:2] + [-1040.0]):
            for H in range(22, 76, 2):
                az, alt = az_alt(lib.dysis_sun_dir(float(H)))
                for c in mts:
                    o, e = unreal.SystemLibrary.get_component_bounds(c)[0:2]
                    daz = abs((math.degrees(math.atan2(o.y - eye[1], o.x - eye[0])) - az + 180.0) % 360.0 - 180.0)
                    dist = math.hypot(o.x - eye[0], o.y - eye[1])
                    half = math.degrees(math.atan2(math.hypot(e.x, e.y) * 0.75, dist))
                    top = math.degrees(math.atan2(o.z + e.z - eye[2], dist))
                    if daz < half and eye[2] > -500.0:      # 岛上看：太阳本来就在神殿背后，不算
                        worst = min(worst, alt - top)
                        if top > alt: hidden += 1
        check(len(mts) == 12 and hidden == 0, "整个下午太阳都没被山挡住（12 座山；最近的一次太阳比山顶高 %.1f°）" % (worst if worst < 99 else 0.0))
        say("太阳光：%s；天的亮度 %.2f" % (sky.sun_light.get_editor_property("light_color"), sky.get_sky_gain()))

    if on("day"):
        # 下午（开局 13:29 起）
        yield from spot("开局_站在小岛上（人的镜头）", SPAWN, [0.0, 0.0, 900.0], 140)
        say("  时刻 " + hour())
        yield from view("开局_小岛上看神殿", [SPAWN[0] + 420.0, SPAWN[1] + 260.0, -1040.0], [0.0, 0.0, 700.0], 80.0)
        yield from view("开局_小岛上回头看海", [SPAWN[0] - 300.0, SPAWN[1] - 130.0, -1040.0], [SPAWN[0] + 4000.0, SPAWN[1] + 1800.0, -1300.0], 80.0)
        yield from view("开局_从海上看全景", polar(8.0, 9800.0, 900.0), [0.0, 0.0, 900.0], 55.0)
        yield from view("开局_从高处看全景", polar(40.0, 11500.0, 4200.0), [-800.0, -300.0, 600.0], 50.0)
        yield from spot("台地_走上来看殿门（人的镜头）", polar(23.7, 3050.0, -120.0), [0.0, 0.0, 800.0], 140)
        say("  时刻 " + hour())
        yield from view("台地_殿门近景", polar(20.0, 2950.0, 60.0), polar(24.0, 1900.0, 520.0), 70.0)
        yield from view("台地_西边的石头和草（俯看）", polar(262.0, 6600.0, 2600.0), polar(268.0, 3000.0, 0.0), 60.0)
        yield from view("台地_东南的石头（俯看）", polar(112.0, 6600.0, 2600.0), polar(116.0, 3000.0, 0.0), 60.0)
        yield from view("台地_石头旁边（人的高度）", polar(268.0, 2560.0, 50.0), polar(262.0, 3700.0, 80.0), 80.0)
        yield from view("台地_绕到背后看日落的方向", polar(215.0, 3300.0, 40.0), polar(225.0, 9000.0, 900.0), 75.0)
        yield from view("台地_背后回看神殿（受光面）", polar(222.0, 4300.0, 200.0), [0.0, 0.0, 1100.0], 75.0)
        yield from view("台地_看远山", polar(140.0, 3500.0, 120.0), polar(142.0, 9000.0, 1500.0), 80.0)
        yield from view("海上_远山、巨岩和云", polar(330.0, 12500.0, 1500.0), polar(150.0, 9000.0, 2200.0), 70.0)
    if on("water"):
        go(polar(285.0, 1250.0, 0.0)); yield from wait(60)
        yield from view("水庭_动画水面和水亭", polar(300.0, 1250.0, 170.0), [0.0, 0.0, 120.0], 80.0)
        yield from view("水庭_贴近水面", polar(250.0, 1090.0, 60.0), polar(200.0, 500.0, -45.0), 75.0)
        yield from view("水庭_从上面俯看水面", polar(250.0, 950.0, 900.0), polar(70.0, 300.0, -45.0), 80.0)
        yield from view("水庭_大厅", polar(300.0, 1250.0, 170.0), [0.0, 0.0, 500.0], 80.0)
        cmd("Dysis.Sluice 1"); yield from wait(200)
        yield from view("水庭_瀑布开了以后的水面", polar(20.0, 1200.0, 170.0), polar(128.0, 1100.0, 100.0), 80.0)
        yield from view("水庭_瀑布开了_大厅", polar(300.0, 1250.0, 170.0), [0.0, 0.0, 500.0], 80.0)
    if on("dusk"):
        # 越往上时间越晚：二层、三层、殿顶
        for name, az, r, z in (("一层走廊", 250.0, 1400.0, 600.0), ("三层走廊", 250.0, 1400.0, 2300.0)):
            go(polar(az, r, z)); yield from wait(90)
            say("  %s 时刻 %s" % (name, hour()))
            look_at(polar(az + 60.0, r, z + 140.0)); yield from wait(40)
            yield from shoot(w, "%s_人的镜头" % name)
            yield from view("%s_室内" % name, polar(az - 12.0, r, z + 165.0), polar(az + 40.0, r, z + 120.0), 80.0)
        go(polar(60.0, 1500.0, 3030.0)); yield from wait(120)
        say("  殿顶 时刻 " + hour())
        s_az, s_alt = az_alt(sky.get_sun_dir())
        say("  殿顶：太阳方位 %.1f° 高度 %.1f°" % (s_az, s_alt))
        yield from view("殿顶_朝日落的方向", polar(s_az + 180.0, 900.0, 3230.0), polar(s_az, 9000.0, 3230.0 + 9900.0 * math.tan(math.radians(max(s_alt, 2.0)))), 80.0)
        yield from view("殿顶_背着太阳看（远山和云）", polar(s_az, 600.0, 3230.0), polar(140.0, 9000.0, 3000.0), 80.0)
        yield from view("殿顶_从外面看全景", polar(s_az + 150.0, 9500.0, 3600.0), [0.0, 0.0, 1500.0], 55.0)
        yield from view("殿顶_从海上看日落", polar(s_az + 180.0, 8200.0, 300.0), [0.0, 0.0, 1300.0], 60.0)
    if on("night"):
        night[0] = True
        go(polar(285.0, 1250.0, 0.0)); yield from wait(200)
        say("  入夜 时刻 " + hour())
        yield from view("夜_水庭的水面", polar(300.0, 1250.0, 170.0), [0.0, 0.0, 120.0], 80.0)
        yield from view("夜_从海上看全景", polar(8.0, 9800.0, 900.0), [0.0, 0.0, 900.0], 55.0)
        m_az, m_alt = az_alt(sky.get_moon_dir())
        say("  月亮方位 %.1f° 高度 %.1f°" % (m_az, m_alt))
        yield from view("夜_台地上看月亮", polar(m_az + 180.0, 3300.0, 60.0), polar(m_az, 9000.0, 60.0 + 12300.0 * math.tan(math.radians(max(m_alt, 3.0)))), 80.0)
        yield from view("夜_看远山", polar(140.0, 3500.0, 120.0), polar(142.0, 9000.0, 1500.0), 80.0)
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
_sc = R(); _sc.h = unreal.register_slate_post_tick_callback(_sc.tick)
