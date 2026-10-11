# 机关模型包里“自己做的那几件”：不是从美术的 glb 转出来的，是按尺寸现做的规整形状（弧线都是圆滑的）。
# 普通 Python 跑：python make_extra_shapes.py <输出目录>     （输出目录和 convert_mech_pack.py 用同一个：项目/Saved/pack8_out）
# 出来的 glb 和转换脚本出来的一个规矩（单位米、+Y 朝上、+Z 是正面、顶点色），导入还是用 ue_import_mech_pack.py。
#
# 为什么要自己做（都是 2026-10-11 用户看了游戏里的样子以后提的）：
#   MoonGateCrescent  月门上的月牙：美术那枚是几块大平面拼的，外弧是折线，“弧度有点怪”——换成两段圆弧围成的月牙，朝向、大小照原来的。
#   GateArch / GateDoor  屋顶细桥的桥门：美术那座拱“边缘不平滑、看着臃肿”——换成一座素的石拱（两根方柱 + 半圆的拱券，棱上倒角）和一扇拱顶的门板。
#   ArmTop* / ArmPav*   两座浑天仪的环和轴：灰盒导出来的模型把“太阳”“月亮 / 月托”都焊死在环上（位置是导出那一刻的），
#                       这里只留环和轴，日、月另外摆（太阳缩到和金苹果一样大，月托做成薄薄的一片；殿顶那座的太阳就是金苹果本身）。
#   MoonHolder        水亭浑天仪的月托：一片浅浅的薄碟子（原来是半个厚球壳）。
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_mech_pack as cv

G = cv.STONE_GREY
STONE = (G, G, G)

def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def unit(a):
    l = math.sqrt(dot(a, a)) or 1.0
    return (a[0] / l, a[1] / l, a[2] / l)

class Mesh:
    def __init__(s): s.P = []; s.N = []; s.C = []; s.T = []
    def patch(s, rows, col, hint, closed=False):
        """一片面：rows 是一排排点（每排一样多）。片子里面法线是连着的（弧面看着圆滑），片子和片子之间是硬棱。
        hint：这片面朝哪边（一个方向，或者“给一个点、回答那里朝哪边”的函数）——按最大的那个三角形定正反。"""
        nr, nc = len(rows), len(rows[0])
        P = [p for r in rows for p in r]; T = []
        for i in range(nr - 1):
            for j in range(nc if closed else nc - 1):
                j2 = (j + 1) % nc
                a, b, c, d = i * nc + j, i * nc + j2, (i + 1) * nc + j2, (i + 1) * nc + j
                T.extend((a, b, c, a, c, d))
        s._add(P, T, col, hint)
    def fan(s, center, ring, col, hint):
        P = [center] + list(ring); T = []
        for i in range(len(ring)): T.extend((0, 1 + i, 1 + (i + 1) % len(ring)))
        s._add(P, T, col, hint)
    def _add(s, P, T, col, hint):
        best = None; bl = 0.0
        for k in range(0, len(T), 3):
            n = cross(sub(P[T[k + 1]], P[T[k]]), sub(P[T[k + 2]], P[T[k]])); l = dot(n, n)
            if l > bl: bl = l; best = (n, k)
        if best is None: return
        n, k = best
        cen = tuple(sum(P[T[k + q]][m] for q in range(3)) / 3.0 for m in range(3))
        want = hint(cen) if callable(hint) else hint
        if dot(n, want) < 0.0:
            for k in range(0, len(T), 3): T[k + 1], T[k + 2] = T[k + 2], T[k + 1]
        N = cv.normals(P, T)
        base = len(s.P); s.P += P; s.N += N; s.C += [col] * len(P); s.T += [base + t for t in T]
    def box(s, lo, hi, col, faces="xXyYzZ"):
        x0, y0, z0 = lo; x1, y1, z1 = hi
        F = {"x": ([(x0, y0, z0), (x0, y1, z0)], [(x0, y0, z1), (x0, y1, z1)], (-1, 0, 0)), "X": ([(x1, y0, z0), (x1, y1, z0)], [(x1, y0, z1), (x1, y1, z1)], (1, 0, 0)),
             "y": ([(x0, y0, z0), (x1, y0, z0)], [(x0, y0, z1), (x1, y0, z1)], (0, -1, 0)), "Y": ([(x0, y1, z0), (x1, y1, z0)], [(x0, y1, z1), (x1, y1, z1)], (0, 1, 0)),
             "z": ([(x0, y0, z0), (x1, y0, z0)], [(x0, y1, z0), (x1, y1, z0)], (0, 0, -1)), "Z": ([(x0, y0, z1), (x1, y0, z1)], [(x0, y1, z1), (x1, y1, z1)], (0, 0, 1))}
        for f in faces: s.patch([F[f][0], F[f][1]], col, F[f][2])
    def torus(s, R, tube, axis, col, seg=72, tseg=10, center=(0.0, 0.0, 0.0)):
        """一道圆环：axis 是环面的法线（"x" / "y" / "z"）。法线按公式给，整圈没有接缝。"""
        base = len(s.P)
        for i in range(seg):
            a = 2.0 * math.pi * i / seg; ca, sa = math.cos(a), math.sin(a)
            for j in range(tseg):
                b = 2.0 * math.pi * j / tseg; cb, sb = math.cos(b), math.sin(b)
                # 环在“u–v”平面里，w 是法线方向
                u, v, w = (R + tube * cb) * ca, (R + tube * cb) * sa, tube * sb
                nu, nv, nw = cb * ca, cb * sa, sb
                p, n = {"z": ((u, v, w), (nu, nv, nw)), "y": ((v, w, u), (nv, nw, nu)), "x": ((w, u, v), (nw, nu, nv))}[axis]
                s.P.append(add(p, center)); s.N.append(n); s.C.append(col)
        for i in range(seg):
            for j in range(tseg):
                a, b, c, d = base + i * tseg + j, base + ((i + 1) % seg) * tseg + j, base + ((i + 1) % seg) * tseg + (j + 1) % tseg, base + i * tseg + (j + 1) % tseg
                s.T.extend((a, b, c, a, c, d))
    def sphere(s, c, r, col, seg=20, rings=12):
        base = len(s.P)
        for i in range(rings + 1):
            t = math.pi * i / rings
            for j in range(seg):
                a = 2.0 * math.pi * j / seg
                n = (math.sin(t) * math.cos(a), math.cos(t), math.sin(t) * math.sin(a))
                s.P.append(add(c, mul(n, r))); s.N.append(n); s.C.append(col)
        for i in range(rings):
            for j in range(seg):
                a, b, c2, d = base + i * seg + j, base + i * seg + (j + 1) % seg, base + (i + 1) * seg + (j + 1) % seg, base + (i + 1) * seg + j
                s.T.extend((a, b, c2, a, c2, d))
    def rod(s, a, b, r, col, seg=8):
        ax = unit(sub(b, a)); ref = (1.0, 0.0, 0.0) if abs(ax[0]) < 0.9 else (0.0, 1.0, 0.0)
        u = unit(cross(ax, ref)); v = cross(ax, u)
        ring = lambda c: [add(c, add(mul(u, r * math.cos(2 * math.pi * i / seg)), mul(v, r * math.sin(2 * math.pi * i / seg)))) for i in range(seg)]
        mid = mul(add(a, b), 0.5)
        s.patch([ring(a), ring(b)], col, lambda p: sub(p, add(mid, mul(ax, dot(sub(p, mid), ax)))), closed=True)
        s.fan(a, ring(a), col, mul(ax, -1.0)); s.fan(b, ring(b), col, ax)
    def fix_winding(s):
        """整件是封闭的实心体时用：按体积的正负统一正反（torus / sphere 是按公式排的，万一排反了这里翻回来）。"""
        vol = 0.0
        for k in range(0, len(s.T), 3):
            a, b, c = s.P[s.T[k]], s.P[s.T[k + 1]], s.P[s.T[k + 2]]
            vol += dot(a, cross(b, c))
        if vol < 0.0:
            for k in range(0, len(s.T), 3): s.T[k + 1], s.T[k + 2] = s.T[k + 2], s.T[k + 1]
        return vol
    def write(s, out_dir, name):
        cv.write_glb(os.path.join(out_dir, name + ".glb"), s.P, s.N, s.C, s.T, name)
        lo, hi = cv.bounds(s.P)
        print("  %-18s 顶点 %5d 三角 %5d  x %.3f…%.3f  y %.3f…%.3f  z %.3f…%.3f" % (name, len(s.P), len(s.T) // 3, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))

# ───────────────────────── 月牙 ─────────────────────────
def crescent(out):
    # 外圆半径 33 cm（月牙高 66 cm），圆心在门板中线上、离门板底 1.583 m（原来那枚的中心）；
    # 内圆半径 28 cm、圆心朝左上（150°）偏 14.5 cm：月牙的肚子在右下、两个尖一个朝上一个朝左，和原来一样。
    # 贴在门板面上（z = 0 是门板朝殿心的那个面）：凸出 2.2 cm，棱上倒 0.8 cm 的角。
    R, r, d, op = 0.33, 0.28, 0.145, math.radians(150.0)
    c0 = (0.0, 1.583); u = (math.cos(op), math.sin(op)); c1 = (c0[0] + d * u[0], c0[1] + d * u[1])
    a = (R * R - r * r + d * d) / (2.0 * d)
    phi = math.acos(a / R); psi = math.acos((a - d) / r)
    n = 72; zf, zb, bev = 0.022, -0.006, 0.008
    O = []; I = []
    for i in range(n + 1):
        t = i / n
        to = op + phi + t * (2.0 * math.pi - 2.0 * phi); ti = op + psi + t * (2.0 * math.pi - 2.0 * psi)
        O.append((c0[0] + R * math.cos(to), c0[1] + R * math.sin(to))); I.append((c1[0] + r * math.cos(ti), c1[1] + r * math.sin(ti)))
    Oin = []; Iin = []
    for o, i_ in zip(O, I):
        w = math.hypot(o[0] - i_[0], o[1] - i_[1]); f = min(bev / w, 0.5) if w > 1e-9 else 0.5
        Oin.append((o[0] + (i_[0] - o[0]) * f, o[1] + (i_[1] - o[1]) * f)); Iin.append((i_[0] + (o[0] - i_[0]) * f, i_[1] + (o[1] - i_[1]) * f))
    at = lambda L, z: [(p[0], p[1], z) for p in L]
    out_o = lambda p: (p[0] - c0[0], p[1] - c0[1], 0.0)          # 外弧：朝外 = 离开外圆的圆心
    out_i = lambda p: (c1[0] - p[0], c1[1] - p[1], 0.0)          # 内弧：朝外 = 朝着内圆的圆心（月牙的怀里）
    m = Mesh()
    m.patch([at(Oin, zf), at(Iin, zf)], STONE, (0.0, 0.0, 1.0))
    m.patch([at(O, zf - bev), at(Oin, zf)], STONE, lambda p: add(unit(out_o(p)), (0.0, 0.0, 1.0)))
    m.patch([at(I, zf - bev), at(Iin, zf)], STONE, lambda p: add(unit(out_i(p)), (0.0, 0.0, 1.0)))
    m.patch([at(O, zb), at(O, zf - bev)], STONE, out_o)
    m.patch([at(I, zb), at(I, zf - bev)], STONE, out_i)
    m.write(out, "MoonGateCrescent")

# ───────────────────────── 桥门：石拱和门板 ─────────────────────────
# 拱洞半宽、柱宽、进深、起拱的高度（米）。
# 2026-10-11 第二回：夜里屋顶往下的楼梯出现时，桥门有一部分悬空——桥门脚下那块不动的平台在离殿心 11.6 m 处只有 1.62 m 宽，
# 原来的拱连柱脚宽 1.75 m，一只脚踩到了旁边会沉下去的那一级上。拱洞收到 1.16 m、柱子 20 cm：连柱脚 1.59 m，整个落在平台里。
# 门推开以后和拱穿在一起——原来门板在拱的进深正中、门轴在拱洞边上：一推开，门板根上那一截就埋在柱子和柱脚里。
# 现在门板挂在拱朝外（朝环道）的那个面上，门轴在这个面上、拱洞的边：门是朝外开的，推开以后整扇都在拱的外面。
# 柱脚、腰线只往两侧和朝桥的那一面凸出来，朝外的那一面（挂门的）和拱洞里面是平的。
GATE_A, GATE_PW, GATE_DP, GATE_HS = 0.58, 0.20, 0.34, 1.85
def gate(out):
    A, PW, DP, HS = GATE_A, GATE_PW, GATE_DP, GATE_HS
    rc = A + PW * 0.5; ch = 0.03
    # 柱子 / 拱券的断面（u 朝外、v 是进深），四个角倒掉 3 cm
    hu, hv = PW * 0.5, DP * 0.5
    prof = [(hu, -hv + ch), (hu, hv - ch), (hu - ch, hv), (-hu + ch, hv), (-hu, hv - ch), (-hu, -hv + ch), (-hu + ch, -hv), (hu - ch, -hv)]
    path = [((-rc, 0.0), (-1.0, 0.0)), ((-rc, HS), (-1.0, 0.0))]
    N = 48
    for i in range(1, N):
        t = math.pi - math.pi * i / N
        path.append(((rc * math.cos(t), HS + rc * math.sin(t)), (math.cos(t), math.sin(t))))
    path += [((rc, HS), (1.0, 0.0)), ((rc, 0.0), (1.0, 0.0))]
    m = Mesh()
    for k in range(len(prof)):
        (u0, v0), (u1, v1) = prof[k], prof[(k + 1) % len(prof)]
        rows = [[(p[0] + o[0] * u0, p[1] + o[1] * u0, v0), (p[0] + o[0] * u1, p[1] + o[1] * u1, v1)] for p, o in path]
        mu, mv = (u0 + u1) * 0.5, (v0 + v1) * 0.5      # 这条边的中点方向就是它朝外的方向
        def hint(p, mu=mu, mv=mv):
            # 柱子上：朝外 = 离开柱心；拱券上：离开拱心那条线
            if p[1] <= HS: o = (-1.0, 0.0) if p[0] < 0.0 else (1.0, 0.0)
            else:
                l = math.hypot(p[0], p[1] - HS) or 1.0; o = (p[0] / l, (p[1] - HS) / l)
            return (o[0] * mu, o[1] * mu, mv)
        m.patch(rows, STONE, hint)
    # 柱脚（各一块矮墩）和起拱那里的一道腰线
    for sx in (-1.0, 1.0):
        cx = sx * rc; inner = cx - sx * hu           # 柱子朝拱洞的那个面
        for grow, y0, y1, top_bottom in ((0.015, 0.0, 0.15, "Y"), (0.012, HS - 0.055, HS + 0.025, "yY")):
            xa, xb = sorted((inner, cx + sx * (hu + grow)))
            side = "X" if sx > 0 else "x"            # 朝两侧的那个面
            m.box((xa, y0, -hv), (xb, y1, hv + grow + 0.02), STONE, faces=side + top_bottom + "Z")
    m.write(out, "GateArch")

    # 门板：拱顶的一块石板（半宽比拱洞小 1.5 cm），正反两面各有一圈边框、中间的板心凹下去 1.5 cm
    b, th, e, dz = A + 0.025, 0.07, 0.085, 0.015      # 挂在外面的门板比拱洞四周各大 2.5 cm（把洞口盖严）
    zc = -(DP * 0.5 + th * 0.5)                       # 门板的正中：拱朝外的那个面再往外半个板厚
    def outline(hw, y0, n=32):
        L = [(-hw, y0), (hw, y0), (hw, HS)]
        for i in range(1, n): L.append((hw * math.cos(math.pi * i / n), HS + hw * math.sin(math.pi * i / n)))
        L.append((-hw, HS))
        return L
    Lo = outline(b, 0.012); Li = outline(b - e, 0.012 + e); Lp = outline(b - e - 0.012, 0.012 + e + 0.012)
    cen = (0.0, HS * 0.55)
    d = Mesh()
    for sgn in (1.0, -1.0):
        zf = zc + sgn * th * 0.5
        at = lambda L, z: [(p[0], p[1], z) for p in L]
        ring = lambda L, z: at(L, z) + [at(L, z)[0]]
        d.patch([ring(Lo, zf), ring(Li, zf)], STONE, (0.0, 0.0, sgn))
        d.patch([ring(Li, zf), ring(Lp, zf - sgn * dz)], STONE, lambda p, sgn=sgn: (cen[0] - p[0], cen[1] - p[1], sgn * 0.6))
        d.fan((cen[0], cen[1], zf - sgn * dz), at(Lp, zf - sgn * dz), STONE, (0.0, 0.0, sgn))
    # 四周的侧面：底、两边、拱顶各一片（转角是硬棱）
    front = [(p[0], p[1], zc + th * 0.5) for p in Lo]; back = [(p[0], p[1], zc - th * 0.5) for p in Lo]
    seg = [(0, 1), (1, 2), (2, len(Lo) - 1), (len(Lo) - 1, len(Lo))]
    for i0, i1 in seg:
        idx = [i % len(Lo) for i in range(i0, i1 + 1)]
        d.patch([[back[i] for i in idx], [front[i] for i in idx]], STONE, lambda p: (p[0] - cen[0], p[1] - cen[1], 0.0))
    d.write(out, "GateDoor")

# ───────────────────────── 浑天仪 ─────────────────────────
BRONZE, GOLD, SILVER = (0.45, 0.25, 0.10), (0.83, 0.62, 0.22), (0.75, 0.77, 0.80)      # 只是占位的颜色：游戏里用的是原来那三种材质
def armillary(out, tag, R, with_moon_bead):
    # 灰盒的 armillary()：两道竖着的铜环（互相垂直）、一道横着的金环（0.96 R）、一道横着的银环（0.92 R）、一根斜着的轴（长 2.3 R）。
    # 下面的坐标是引擎里那个部件自己的坐标（厘米，Z 朝上）换过来的：这里的 (x, y, z) = 引擎的 (X, Z, Y) / 100。
    tube = 0.018
    b = Mesh(); b.torus(R, tube, "z", BRONZE); b.torus(R, tube, "x", BRONZE)
    ax = unit((0.0, 34.3, 49.0))                      # 引擎里是 (0, 49.0, 34.3)
    b.rod(mul(ax, -R * 1.15), mul(ax, R * 1.15), 0.012, BRONZE, seg=8)
    b.write(out, "Arm%sBronze" % tag)
    g = Mesh(); g.torus(R * 0.96, tube, "y", GOLD); g.write(out, "Arm%sGold" % tag)
    s = Mesh(); s.torus(R * 0.92, tube, "y", SILVER)
    if with_moon_bead: s.sphere((0.181, -0.261, -0.394), 0.035, SILVER, seg=14, rings=8)      # 殿顶那座的小月珠：还在原来的位置（引擎里是 (18.1, −39.4, −26.1)）
    s.write(out, "Arm%sSilver" % tag)

def moon_holder(out):
    # 薄薄的一片浅碟：口径 17 cm、深 2.8 cm、厚 4 mm。原点在碟口的圆心，碟口朝上（+Y）。
    Rm, h, th, seg, rings = 0.085, 0.028, 0.004, 36, 8
    rho = (Rm * Rm + h * h) / (2.0 * h); fmax = math.asin(Rm / rho)
    def ring(rad, f):      # 球心在碟口上方 rho − h 处；f = 从碟底往上转的角
        return [(rad * math.sin(f) * math.cos(2 * math.pi * j / seg), (rho - h) - rad * math.cos(f), rad * math.sin(f) * math.sin(2 * math.pi * j / seg)) for j in range(seg)]
    inner = [ring(rho, fmax * i / rings) for i in range(rings + 1)]
    outer = [ring(rho + th, fmax * i / rings) for i in range(rings + 1)]
    cen = (0.0, rho - h, 0.0)
    m = Mesh()
    m.patch(inner, SILVER, lambda p: sub(cen, p), closed=True)
    m.patch(outer, SILVER, lambda p: sub(p, cen), closed=True)
    m.patch([inner[-1], outer[-1]], SILVER, (0.0, 1.0, 0.0), closed=True)
    m.write(out, "MoonHolder")

if __name__ == "__main__":
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    crescent(out); gate(out)
    armillary(out, "Top", 0.55, True); armillary(out, "Pav", 0.52, False)
    moon_holder(out)
