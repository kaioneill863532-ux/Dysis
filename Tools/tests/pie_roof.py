# 屋顶这一段的测试（UE 编辑器 Python，开 PIE 跑）：圆眼光柱和灰盒比；从细桥上环道、顺时针走，踏步升起来；
# 在浑天仪旁取下金苹果（入夜）；往回走，桥门锁上、另外半圈降成楼梯。
# 标准答案：Tools/greybox/golden/greybox_roof.json。结果写在 项目/Saved/pie_roof.txt，最后一行 DONE。
import unreal, os, json, math, glob, traceback
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = SAVED + "pie_roof.txt"
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_roof.json", encoding="utf-8"))
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
    cmd(w, "Dysis.Sluice 1")     # 正常玩到这里水闸早就开了：瀑布在流
    beams = {str(b.get_editor_property("greybox_id")): b for b in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DysisBeamActor)}
    tc = pawn.get_component_by_class(unreal.DysisTimeComponent)
    mv = pawn.get_component_by_class(unreal.DysisCharacterMovement)
    hh = pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    def foot():
        l = pawn.get_actor_location(); return (l.x, l.y, l.z - hh)
    def state(): return str(tc.describe_state())
    def roof(): return json.loads(director.describe_roof())
    def az_r():
        f = foot(); return math.degrees(math.atan2(f[1], f[0])) % 360, math.hypot(f[0], f[1])

    # ── A 圆眼光柱：不同的光圈大小 × 不同时刻，和灰盒比 ──
    check("oculus" in beams, "关卡里的圆眼光柱已按灰盒算法算（%s）" % sorted(beams))
    cmd(w, "Dysis.Mist 1 3150")
    n = ok = 0; notes = []
    for r in G["oculus"]:
        cmd(w, "Dysis.Iris %.1f" % r["a"])
        yield
        exp = r["beam"]; got = json.loads(beams["oculus"].debug_solve(r["H"]))
        good = exp["valid"] == got["valid"] and exp["clean"] == got["clean"] and exp["walkable"] == got["walkable"]
        if good and exp["valid"]:
            good = max(abs(a - b) for a, b in zip(exp["c"], got["c"])) <= 12 and max(abs(a - b) for a, b in zip(exp["edge"], got["edge"])) <= 12 \
                and max(abs(a - b) for a, b in zip(exp["O"], got["O"])) <= 1.5 and abs(exp["s1"] - got["s1"]) <= 12 and abs(exp["s0"] - got["s0"]) <= 1
        n += 1; ok += 1 if good else 0
        if not good and len(notes) < 6:
            notes.append("光圈 %.0f H=%.2f：灰盒 valid=%s clean=%s walk=%s c=%s / UE valid=%s clean=%s walk=%s c=%s" % (r["a"], r["H"], exp["valid"], exp["clean"], exp["walkable"], [round(x) for x in exp.get("c", [])], got["valid"], got["clean"], got["walkable"], [round(x) for x in got.get("c", [])]))
    check(ok == n, "圆眼光柱 %d/%d 组和灰盒一致" % (ok, n))
    for x in notes: say("     " + x)
    cmd(w, "Dysis.WorldRelease"); cmd(w, "Dysis.Mist 1 3150")

    # ── B 开局：踏步都落平 ──
    r0 = roof()
    ups = [p for p in r0["pieces"] if p["kind"] == 1]; dns = [p for p in r0["pieces"] if p["kind"] == 2 and p["hasTread"]]
    check(len(ups) == 16 and all(p["hasTread"] and abs(p["treadZ"] - 3030) < 0.5 for p in ups), "开局 16 级踏步都落在环道面上（找到 %d 级）" % sum(1 for p in ups if p["hasTread"]))
    check(len(dns) == 16 and sum(1 for p in r0["pieces"] if p["hasBlade"]) == 38, "夜里的 16 级楼梯和 38 片光圈叶片都认到了（%d 级、%d 片）" % (len(dns), sum(1 for p in r0["pieces"] if p["hasBlade"])))

    # ── C 从细桥上环道：桥门自己转开 ──
    S = G["bridge"]["S"]; E = G["bridge"]["end"]
    cmd(w, "Dysis.Go %.1f %.1f 3045" % (S[0] + 0.3 * (E[0] - S[0]), S[1] + 0.3 * (E[1] - S[1])))
    yield from wait(60)
    check("zone=rbridge" in state(), "站在屋顶细桥上：" + state()[:60])
    yield from shoot(w, "细桥上")
    for _ in range(900):
        f = foot(); dx, dy = E[0] - f[0], E[1] - f[1]; d = math.hypot(dx, dy)
        if d < 120: break
        pawn.add_movement_input(unreal.Vector(dx / d, dy / d, 0), 1.0, False); yield
    yield from wait(120)
    check(roof()["door"] > 0.9, "走近桥尾，桥门转开了（%.2f）" % roof()["door"])
    # 走上环道
    for _ in range(600):
        if "zone=crown" in state(): break
        a, r = az_r(); t = math.radians(a)
        pawn.add_movement_input(unreal.Vector(math.cos(t), math.sin(t), 0), 1.0, False); yield
    check("zone=crown" in state(), "走上环道：" + state()[:60])

    # ── D 顺时针往前走，前面的踏步跟着升成阶梯，一直走到最高一级 ──
    z_start = foot()[2]
    for i in range(4200):
        a, r = az_r()
        if (a - 227.655) % 360 > 158.0 and (a - 227.655) % 360 < 200: break
        t = math.radians(a)
        # 顺时针（方位角增大）的切向，加一点把半径拉回 13.2 m 的分量
        tx, ty = -math.sin(t), math.cos(t); corr = (1320.0 - r) / 300.0
        pawn.add_movement_input(unreal.Vector(tx + math.cos(t) * corr, ty + math.sin(t) * corr, 0), 1.0, False)
        if i == 600: yield from shoot(w, "踏步升起来")
        yield
    yield from wait(40)
    r1 = roof(); f = foot()
    check(r1["up"] >= 0.98 and abs(f[2] - 3750) < 6, "走到最高一级：踏步升到头（%.3f），脚高 %.0f cm（应 3750）" % (r1["up"], f[2]))
    check(mv.get_editor_property("respawn_count") == 0, "一路没有掉下去")
    yield from shoot(w, "最高一级")

    # ── E 走到浑天仪旁，取下金苹果 ──
    arm = G["armTop"]
    for _ in range(600):
        f = foot(); dx, dy = arm[0] - f[0], arm[1] - f[1]; d = math.hypot(dx, dy)
        if d < 110: break
        pawn.add_movement_input(unreal.Vector(dx / d, dy / d, 0), 1.0, False); yield
    yield from wait(30)
    say("   在浑天仪旁：" + state()[:80])
    yield from shoot(w, "浑天仪旁_提示")
    check(director.debug_interact("takeApple"), "按 E 取金苹果")
    yield from wait(30)
    r2 = roof()
    check(r2["caught"] and "night=1" in state(), "接住了最后一缕光，入夜：" + state()[:70])
    yield from shoot(w, "取下金苹果")
    yield from wait(330)
    yield from shoot(w, "入夜标题")

    # ── F 往回走（逆时针）到桥头：桥门已经锁上；另外半圈降成楼梯 ──
    for i in range(5000):
        a, r = az_r()
        if (a - 219.655) % 360 < 6.0: break
        t = math.radians(a)
        tx, ty = math.sin(t), -math.cos(t); corr = (1320.0 - r) / 300.0
        pawn.add_movement_input(unreal.Vector(tx + math.cos(t) * corr, ty + math.sin(t) * corr, 0), 1.0, False); yield
    yield from wait(60)
    r3 = roof()
    check(r3["up"] < 0.03 and r3["dn"], "回到桥头：升起的台阶落平了（%.3f），另外半圈开始降成楼梯" % r3["up"])
    check(r3["doorLocked"] and r3["door"] < 0.2, "桥门转回来锁上了（%.2f）" % r3["door"])
    yield from wait(1500)
    r4 = roof()
    spec = {(p["k"]): p["zNight"] for p in G["pieces"] if p["kind"] == "dn" and (p["a1"] - p["a0"]) > 0.5}
    worst = max(abs(p["z"] - spec[p["k"]]) for p in r4["pieces"] if p["kind"] == 2 and p["hasTread"])
    check(worst < 1.0 and abs(r4["iris"] - 1100) < 1, "25 秒后 16 级楼梯都降到位（最大差 %.1f cm），光圈全开（%.0f）" % (worst, r4["iris"]))
    yield from shoot(w, "夜里的楼梯")
    # 沿楼梯往下走到四层。经过瀑布那一段（方位 116°–140°）时楼梯只有靠中庭的内侧一半，外侧有挡墙：这一段贴着内侧走
    # 最后一级踏步和四层的地面一样高：刚踩上地面就停的话，脚下算哪一块会来回跳（区域在 crown / L3 之间跳），所以踩上以后再多走半秒
    extra = -1
    for i in range(5000):
        a, r = az_r()
        if extra < 0 and "zone=L3" in state() and foot()[2] < 2310: extra = 30
        if extra == 0: break
        if extra > 0: extra -= 1
        t = math.radians(a)
        want_r = 1160.0 if 118.0 < a < 152.0 else 1320.0
        tx, ty = math.sin(t), -math.cos(t); corr = (want_r - r) / 300.0
        pawn.add_movement_input(unreal.Vector(tx + math.cos(t) * corr, ty + math.sin(t) * corr, 0), 1.0, False); yield
    yield from wait(30)
    f = foot()
    a, r = az_r()
    check("zone=L3" in state() and abs(f[2] - 2300) < 6 and mv.get_editor_property("respawn_count") == 0, "沿夜里的楼梯走到四层：脚高 %.0f cm，方位 %.1f° 半径 %.0f，%s" % (f[2], a, r, state()[:60]))
    yield from shoot(w, "走下楼梯到四层")
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
_rf = R(); _rf.h = unreal.register_slate_post_tick_callback(_rf.tick)
