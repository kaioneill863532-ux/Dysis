# 按灰盒导出的数据（Tools/greybox/golden/greybox_walls.json）在关卡里摆“看不见的墙”（UE 编辑器 Python，可以重复跑；跑完存关卡）。
# 灰盒里这些墙没有模型，只挡人：每层走到瀑布边到头的挡墙、栏杆上方 2.3 m 的高护栏（光路越过栏杆的地方留了口）、
# 台地和小岛边缘的护栏、墙顶的护栏、窗洞里的挡板……
# 每堵是一个 ADysisInvisibleWall（线框盒子，游戏里看不见），都放在大纲的 Dysis_InvisibleWalls/<类别> 文件夹里。
# 重新跑会先把这个文件夹里原来的全删掉再摆。
import unreal, json, math
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_walls.json", encoding="utf-8"))
FOLDER = "Dysis_InvisibleWalls"
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
old = [a for a in sub.get_all_level_actors() if str(a.get_folder_path()).startswith(FOLDER)]
for a in old: sub.destroy_actor(a)

def spawn(group, center, yaw, ext, label):
    a = sub.spawn_actor_from_class(unreal.DysisInvisibleWall, unreal.Vector(*center), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    a.set_editor_property("extent_cm", unreal.Vector(*ext))
    a.set_editor_property("group", group)
    a.set_actor_label(label); a.set_folder_path(FOLDER + "/" + group)
    return a

n = {}
for w in G["walls"]:
    g = w["group"]; n[g] = n.get(g, 0) + 1
    label = "Wall_%s_%03d" % (g, n[g])
    zc, hz = (w["z0"] + w["z1"]) / 2.0, (w["z1"] - w["z0"]) / 2.0
    if w["t"] == "sector":
        # 一段环用直板顶着：每块不超过 5°（弧和弦只差一厘米多），板的厚度把这点差（弓高）也包进去
        A0, A1, r0, r1 = w["a0"], w["a1"], w["r0"], w["r1"]
        parts = max(1, int(math.ceil((A1 - A0) / 5.0)))
        for i in range(parts):
            a0 = A0 + (A1 - A0) * i / parts; a1 = A0 + (A1 - A0) * (i + 1) / parts
            half = math.radians((a1 - a0) / 2.0); mid = math.radians((a0 + a1) / 2.0)
            r_in = r0 * math.cos(half)                   # 内弧的弦离圆心最近处
            rc, hr = (r_in + r1) / 2.0, (r1 - r_in) / 2.0
            ht = r1 * math.sin(half)                     # 沿切向的半长（按外弧取）
            spawn(g, (rc * math.cos(mid), rc * math.sin(mid), zc), math.degrees(mid), (hr, ht, hz), label if parts == 1 else "%s_%d" % (label, i + 1))
    else:
        p = w["poly"]                                    # a−n、b−n、b+n、a+n
        cx = sum(q[0] for q in p) / 4.0; cy = sum(q[1] for q in p) / 4.0
        dx, dy = p[1][0] - p[0][0], p[1][1] - p[0][1]
        tx, ty = p[3][0] - p[0][0], p[3][1] - p[0][1]
        spawn(g, (cx, cy, zc), math.degrees(math.atan2(dy, dx)), (math.hypot(dx, dy) / 2.0, math.hypot(tx, ty) / 2.0, hz), label)
print("删掉旧的 %d 堵；新摆：" % len(old), n, "共", sum(n.values()))
print("存关卡：", unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
