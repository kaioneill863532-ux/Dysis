# 找出“灰盒里不挡光、UE 里却挡了”的部件（UE 编辑器 Python，不用开 PIE）：
# 按灰盒标准答案里每束光的四个角和方向，在编辑器世界里打光线；灰盒说能照到更远的，看 UE 里是被谁挡住的。
# 另外对窗洞上的取样点朝太阳打光线，统计挡住它们的部件。结果打印出来（部件标签 × 次数）。
import unreal, json, math, collections
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_beams.json", encoding="utf-8"))
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
VIS = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1
R_IN = 1550.0
def trace(s, d, far):
    e = unreal.Vector(s.x + d.x * far, s.y + d.y * far, s.z + d.z * far)
    hit = unreal.SystemLibrary.line_trace_single(w, s, e, VIS, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit: return None
    t = hit.to_tuple()
    return t[3], t[9]   # 距离、Actor
def label(a):
    return "%s  [%s]" % (a.get_actor_label(), a.get_folder_path()) if a else "?"
def sun_dir(H):
    v = unreal.DysisSkyLibrary.dysis_sun_dir(H); return v
short = collections.Counter(); sunblk = collections.Counter()
for r in G["sets"][0]["rows"]:
    H = r["H"]; sun = sun_dir(H)
    for b in r["beams"]:
        win = G["WIN"][b["id"]]
        # 1) 沿光的四个角
        if b["valid"]:
            O, U, V, L = b["O"], b["U"], b["V"], b["L"]
            d = unreal.Vector(*L)
            C = [O, [O[i] + U[i] for i in range(3)], [O[i] + V[i] for i in range(3)], [O[i] + U[i] + V[i] for i in range(3)]]
            for k in range(4):
                s = unreal.Vector(C[k][0] + L[0] * 5, C[k][1] + L[1] * 5, C[k][2] + L[2] * 5)
                h = trace(s, d, 12000)
                got = (h[0] + 5) if h else 12000
                if got < b["c"][k] - 12: short[(b["id"], label(h[1]))] += 1
        # 2) 窗洞取样点朝太阳
        hw = math.degrees((win["w"] * 100 / 2) / R_IN); lit = 0
        blk = collections.Counter()
        for i in range(8):
            for j in range(5):
                u, v = (i + 0.5) / 8, (j + 0.5) / 5
                az = math.radians(win["az"] - hw + 2 * hw * u); z = win["y0"] * 100 + win["h"] * 100 * v
                p = unreal.Vector((R_IN - 2) * math.cos(az) + sun.x * 0.1, (R_IN - 2) * math.sin(az) + sun.y * 0.1, z + sun.z * 0.1)
                h = trace(p, sun, 40000)
                if h is None: lit += 1
                else: blk[label(h[1])] += 1
        if sun.z > 0.003 and abs(lit / 40.0 - b["lit"]) > 0.026 and lit / 40.0 < b["lit"]:
            for k2, n in blk.items(): sunblk[(b["id"], k2)] += n
print("== 沿光方向：UE 里比灰盒先被挡住，挡住的是 ==")
for (bid, lab), n in sorted(short.items(), key=lambda x: -x[1])[:40]: print("  %-5s %4d  %s" % (bid, n, lab))
print("== 窗洞朝太阳：UE 里照亮比例比灰盒低的那些时刻，挡住取样点的是 ==")
for (bid, lab), n in sorted(sunblk.items(), key=lambda x: -x[1])[:40]: print("  %-5s %4d  %s" % (bid, n, lab))
