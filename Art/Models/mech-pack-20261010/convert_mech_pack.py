# 美术 2026-10-10 的机关模型包（《打包.zip》，19 个 glb）→ 游戏里用的轻量 glb。
# 普通 Python 跑（不依赖第三方库；读的是别人给的文件，用 -I）：
#   python -I convert_mech_pack.py <解包目录> <输出目录> [只做哪几件…]
# 原件大多是 30–100 万面 + 2048 / 4096 贴图（共 331 MB），不能照搬进仓库。这里做四件事：
#   1 底色贴图 → 顶点色（贴图要先用 make_bmp.ps1 转成 512 的 BMP，这里没有 JPEG 解码）；没贴图的按材质的颜色；
#   2 按格子合并顶点，先压到几万面（格子多大各件自己定）。真正降到游戏里用的几千面是导入时用引擎自带的减面做的（ue_import_mech_pack.py）：
#     试过在这里用粗格子直接合并（一步到位），出来的面坑坑洼洼；引擎的减面能保住美术原来那种“大块平面”的造型；
#   3 摆成统一的样子：单位米、+Y 朝上、+Z 是正面、脚底中心在原点、已经是游戏里的大小；
#   4 浮雕：背板抹平到一个面上、四边收进墙里、顺着圆墙弯好（背板那个面 = 墙面）。
# 输出：<输出目录>/<件名>.glb（位置、法线、顶点色）和一张 report.txt（各件的尺寸，写运行时代码要用）。
import os, sys, struct, json, math

# ───────────────────────── 读 glb ─────────────────────────
FMT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
NUM = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}

def load(path):
    f = open(path, "rb"); f.read(12)
    clen, _ = struct.unpack("<I4s", f.read(8)); j = json.loads(f.read(clen).decode("utf-8"))
    blen, _ = struct.unpack("<I4s", f.read(8)); bin_ = f.read(blen)
    return j, bin_

def accessor(j, bin_, idx):
    a = j["accessors"][idx]; bv = j["bufferViews"][a["bufferView"]]
    n = NUM[a["type"]]; fmt = FMT[a["componentType"]]; size = struct.calcsize(fmt) * n
    stride = bv.get("byteStride") or size
    off = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    if stride == size: flat = struct.unpack_from("<%d%s" % (a["count"] * n, fmt), bin_, off)
    else:
        flat = []
        for i in range(a["count"]): flat.extend(struct.unpack_from("<%d%s" % (n, fmt), bin_, off + i * stride))
    if a.get("normalized") and fmt in "BH":
        k = 255.0 if fmt == "B" else 65535.0; flat = [v / k for v in flat]
    return flat, n, a["count"]

def mat_mul(a, b): return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]
def node_matrix(nd):
    if "matrix" in nd:
        m = nd["matrix"]; return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    t = nd.get("translation", [0, 0, 0]); x, y, z, w = nd.get("rotation", [0, 0, 0, 1]); s = nd.get("scale", [1, 1, 1])
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[r[i][0] * s[0], r[i][1] * s[1], r[i][2] * s[2], t[i]] for i in range(3)] + [[0, 0, 0, 1]]

def read_bmp(path):
    b = open(path, "rb").read()
    off, = struct.unpack_from("<I", b, 10); w, h, _, bpp = struct.unpack_from("<iiHH", b, 18)
    assert bpp in (24, 32), bpp
    step = bpp // 8; row = (w * step + 3) & ~3
    return w, abs(h), b, off, row, step, h > 0      # h > 0：行从下往上存

def srgb_to_lin(c): return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
def lin_to_srgb(c): return 12.92 * c if c <= 0.0031308 else 1.055 * (c ** (1 / 2.4)) - 0.055
LUT = [srgb_to_lin(i / 255.0) for i in range(256)]

def gather(j, bin_, texdir, key, want):
    """按节点名挑出要的部分。返回 (点 [(x,y,z)], 颜色 [(r,g,b) 线性], 三角形 [i…])，都已乘上节点的变换。"""
    P = []; C = []; T = []
    nodes = j.get("nodes", []); mats = j.get("materials", [])
    tex_cache = {}
    def tex_for(mi):
        m = mats[mi]; t = m.get("pbrMetallicRoughness", {}).get("baseColorTexture")
        if not t: return None
        img = j["textures"][t["index"]]["source"]
        if img not in tex_cache:
            p = os.path.join(texdir, "%s_%d.bmp" % (key, img))
            tex_cache[img] = read_bmp(p) if os.path.exists(p) else None
            if tex_cache[img] is None: print("    （没有 %s：这一块按材质的颜色）" % p)
        return tex_cache[img]
    def walk(i, parent, anc=""):
        nd = nodes[i]; m = mat_mul(parent, node_matrix(nd)); name = nd.get("name", "")
        if "mesh" in nd and want(anc + "/" + name):
            for p in j["meshes"][nd["mesh"]]["primitives"]:
                pos, _, cnt = accessor(j, bin_, p["attributes"]["POSITION"])
                base = len(P)
                for k in range(0, cnt * 3, 3):
                    x, y, z = pos[k], pos[k + 1], pos[k + 2]
                    P.append((m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3], m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3], m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3]))
                mi = p.get("material"); tex = tex_for(mi) if mi is not None else None
                fac = (mats[mi].get("pbrMetallicRoughness", {}).get("baseColorFactor") or [0.8, 0.8, 0.8, 1])[:3] if mi is not None else [0.8, 0.8, 0.8]
                if tex and "TEXCOORD_0" in p["attributes"]:
                    uv, _, _ = accessor(j, bin_, p["attributes"]["TEXCOORD_0"])
                    w, h, b, off, row, step, bottom_up = tex
                    for k in range(0, cnt * 2, 2):
                        u = uv[k] % 1.0; v = uv[k + 1] % 1.0
                        px = min(w - 1, int(u * w)); py = min(h - 1, int(v * h))
                        if bottom_up: py = h - 1 - py
                        o = off + py * row + px * step
                        C.append((LUT[b[o + 2]] * fac[0], LUT[b[o + 1]] * fac[1], LUT[b[o]] * fac[2]))
                else:
                    C.extend([tuple(fac)] * cnt)
                idx = accessor(j, bin_, p["indices"])[0] if "indices" in p else range(cnt)
                det = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))
                if det < 0:      # 镜像过的节点：三角形的绕向要反过来
                    idx = list(idx)
                    for k in range(0, len(idx) - 2, 3): idx[k + 1], idx[k + 2] = idx[k + 2], idx[k + 1]
                T.extend(base + q for q in idx)
        for c in nd.get("children", []): walk(c, m, anc + "/" + name)
    I = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
    for r in j.get("scenes", [{}])[j.get("scene", 0)].get("nodes", list(range(len(nodes)))): walk(r, I)
    return P, C, T

# ───────────────────────── 合并顶点、算法线 ─────────────────────────
def cluster(P, C, T, cell):
    """按边长 cell 的格子合并顶点（同一格的取平均），丢掉退化成线 / 点的三角形和重复的三角形。"""
    if not isinstance(cell, tuple) and cell <= 0:      # 不压：只把位置、颜色都一样的点并起来
        cell_key = lambda p, c: (round(p[0] * 1e5), round(p[1] * 1e5), round(p[2] * 1e5), round(c[0] * 255), round(c[1] * 255), round(c[2] * 255))
    if isinstance(cell, tuple): ix, iy, iz = 1.0 / cell[0], 1.0 / cell[1], 1.0 / cell[2]; cell = cell[0]
    else: ix = iy = iz = (1.0 / cell if cell > 0 else 0.0)
    ids = {}; sx = []; sy = []; sz = []; sr = []; sg = []; sb = []; cnt = []; remap = [0] * len(P)
    for i, p in enumerate(P):
        c = C[i]
        k = (math.floor(p[0] * ix), math.floor(p[1] * iy), math.floor(p[2] * iz)) if cell > 0 else cell_key(p, c)
        q = ids.get(k)
        if q is None:
            q = ids[k] = len(sx); sx.append(0.0); sy.append(0.0); sz.append(0.0); sr.append(0.0); sg.append(0.0); sb.append(0.0); cnt.append(0)
        sx[q] += p[0]; sy[q] += p[1]; sz[q] += p[2]; sr[q] += c[0]; sg[q] += c[1]; sb[q] += c[2]; cnt[q] += 1; remap[i] = q
    NP = [(sx[q] / cnt[q], sy[q] / cnt[q], sz[q] / cnt[q]) for q in range(len(sx))]
    NC = [(sr[q] / cnt[q], sg[q] / cnt[q], sb[q] / cnt[q]) for q in range(len(sx))]
    seen = set(); NT = []
    for k in range(0, len(T) - 2, 3):
        a, b, c = remap[T[k]], remap[T[k + 1]], remap[T[k + 2]]
        if a == b or b == c or a == c: continue
        key = (a, b, c) if a < b and a < c else ((b, c, a) if b < c else (c, a, b))
        if key in seen: continue
        seen.add(key); NT.extend((a, b, c))
    used = {}; OP = []; OC = []
    for k, v in enumerate(NT):
        q = used.get(v)
        if q is None: q = used[v] = len(OP); OP.append(NP[v]); OC.append(NC[v])
        NT[k] = q
    return OP, OC, NT

def fix_black(P, C, T, rounds=6, dark=0.035):
    """近乎全黑的顶点（贴图的缝）：换成相邻顶点里不黑的那些的平均色，一圈一圈往里填。"""
    C = list(C)
    bad = set(i for i, c in enumerate(C) if 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2] < dark)
    if not bad: return C, 0
    total = len(bad)
    nb = {}
    for k in range(0, len(T) - 2, 3):
        tri = (T[k], T[k + 1], T[k + 2])
        for a_ in tri:
            if a_ in bad:
                for b_ in tri:
                    if b_ != a_: nb.setdefault(a_, set()).add(b_)
    for _ in range(rounds):
        done = {}
        for i in bad:
            good = [C[j] for j in nb.get(i, ()) if j not in bad]
            if good: done[i] = tuple(sum(g[q] for g in good) / len(good) for q in range(3))
        if not done: break
        for i, c in done.items(): C[i] = c; bad.discard(i)
    for i in bad: C[i] = (STONE_GREY, STONE_GREY, STONE_GREY)
    return C, total

def drop_islands(P, C, T, min_tris):
    """丢掉零碎的小块（和主体不连着、三角形不到 min_tris 个的）：高模上飘着的碎片，减面以后就是一粒一粒的渣。"""
    par = list(range(len(P)))
    def find(a):
        while par[a] != a: par[a] = par[par[a]]; a = par[a]
        return a
    for k in range(0, len(T) - 2, 3):
        a, b, c = find(T[k]), find(T[k + 1]), find(T[k + 2])
        if a != b: par[b] = a
        c = find(c); a = find(a)
        if a != c: par[c] = a
    cnt = {}
    for k in range(0, len(T) - 2, 3): r = find(T[k]); cnt[r] = cnt.get(r, 0) + 1
    keep = set(r for r, n in cnt.items() if n >= min_tris)
    NT = []
    for k in range(0, len(T) - 2, 3):
        if find(T[k]) in keep: NT.extend((T[k], T[k + 1], T[k + 2]))
    used = {}; OP = []; OC = []
    for k, v in enumerate(NT):
        q = used.get(v)
        if q is None: q = used[v] = len(OP); OP.append(P[v]); OC.append(C[v])
        NT[k] = q
    return OP, OC, NT, len(cnt) - len(keep)

def taubin(P, C, T, iters, pin=None, lam=0.60, mu=-0.63, color=0.5):
    """把细碎的褶皱抹平（Taubin：一步往邻居的平均靠、一步往回推，大的形体不缩）。pin[i] 为真的点不动。颜色也跟着往邻居匀一点（不然减面以后一块一块的）。"""
    nb = [set() for _ in P]
    for k in range(0, len(T) - 2, 3):
        a, b, c = T[k], T[k + 1], T[k + 2]
        nb[a].add(b); nb[a].add(c); nb[b].add(a); nb[b].add(c); nb[c].add(a); nb[c].add(b)
    nb = [tuple(x) for x in nb]
    X = [list(p) for p in P]; K = [list(c) for c in C]
    for it in range(iters * 2):
        f = lam if it % 2 == 0 else mu
        Y = [None] * len(X)
        for i, p in enumerate(X):
            n = nb[i]
            if not n or (pin is not None and pin[i]): Y[i] = p; continue
            inv = 1.0 / len(n)
            ax = ay = az = 0.0
            for j in n: q = X[j]; ax += q[0]; ay += q[1]; az += q[2]
            Y[i] = [p[0] + f * (ax * inv - p[0]), p[1] + f * (ay * inv - p[1]), p[2] + f * (az * inv - p[2])]
        X = Y
    for _ in range(2 if color > 0 else 0):
        Y = [None] * len(K)
        for i, c in enumerate(K):
            n = nb[i]
            if not n: Y[i] = c; continue
            inv = 1.0 / len(n)
            Y[i] = [c[q] + color * (sum(K[j][q] for j in n) * inv - c[q]) for q in range(3)]
        K = Y
    return [tuple(p) for p in X], [tuple(c) for c in K]

def simplify(P, C, T, eps, accent_sat=0.38, keep_border=True, crease=0.0, crease_cos=0.80):
    """保住大平面的减面。美术的这些模型本身是“一块一块平面”的低模造型，只是每块平面切成了几十上百个小三角：
    这里把同一块平面里多余的点一个个并掉（并掉以后形状偏离不超过 eps 米的才并），棱线、转角上的点留着——出来就是原来那些大平面，一块对一块。
    （头一版是让引擎按“降到几千面”硬减，面数一到不管是不是棱线都削，削出来是碎块。）
    做法：边收缩 + 二次误差（Garland–Heckbert），收缩到边的一头；有颜色的点缀（金、蓝、彩虹）和石头之间不并，点缀之间颜色差得多的也不并。
    crease > 0：棱线（两个面夹角大于 crease_cos 对应的角度）也护着——顺着直的棱线并点不要钱，把弯的棱线并成折线要算偏差。
    不护的话，一条平顶的弯带子（浮雕上的弧、彩虹）顶面是平的、怎么并都“没偏差”，会被并成寥寥几段折线
    （2026-10-11 用户：塞勒涅左边和上边的弧、伊莉丝的彩虹太生硬）。数越大护得越紧：2 = 棱线最多偏 eps，8 = 最多偏半个 eps。"""
    import heapq
    nv = len(P)
    tris = [[T[k], T[k + 1], T[k + 2]] for k in range(0, len(T) - 2, 3)]
    vt = [set() for _ in range(nv)]
    for i, t in enumerate(tris):
        vt[t[0]].add(i); vt[t[1]].add(i); vt[t[2]].add(i)
    Q = [[0.0] * 10 for _ in range(nv)]; W = [0.0] * nv      # W：这个点名下的面一共多大（面积的两倍），误差按它取平均
    def plane_q(n, d, w=1.0):
        a, b, c = n
        return (w * a * a, w * a * b, w * a * c, w * a * d, w * b * b, w * b * c, w * b * d, w * c * c, w * c * d, w * d * d)
    def tri_n(a, b, c):
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        return (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
    edge_cnt = {}
    for i, t in enumerate(tris):
        a, b, c = P[t[0]], P[t[1]], P[t[2]]
        n = tri_n(a, b, c); l = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
        if l < 1e-14: continue
        n = (n[0] / l, n[1] / l, n[2] / l); q = plane_q(n, -(n[0] * a[0] + n[1] * a[1] + n[2] * a[2]), l)
        for v in t:
            Qv = Q[v]; W[v] += l
            for k in range(10): Qv[k] += q[k]
        for x, y in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            key = (x, y) if x < y else (y, x); edge_cnt.setdefault(key, []).append(n)
    if crease > 0.0:     # 棱线：两个面各加一张“竖在这条棱上、垂直于那个面”的墙
        for (x, y), lst in edge_cnt.items():
            if len(lst) != 2: continue
            n1, n2 = lst
            if n1[0] * n2[0] + n1[1] * n2[1] + n1[2] * n2[2] > crease_cos: continue
            a, b = P[x], P[y]
            e = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            for n in (n1, n2):
                m = (e[1] * n[2] - e[2] * n[1], e[2] * n[0] - e[0] * n[2], e[0] * n[1] - e[1] * n[0]); l = math.sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2])
                if l < 1e-14: continue
                m = (m[0] / l, m[1] / l, m[2] / l); q = plane_q(m, -(m[0] * a[0] + m[1] * a[1] + m[2] * a[2]), crease * (W[x] + W[y]) * 0.5)
                for v in (x, y):
                    Qv = Q[v]
                    for k in range(10): Qv[k] += q[k]
    if keep_border:      # 敞着的边（只有一个面用到）：加一张竖在边上的“墙”，边线不许挪
        for (x, y), lst in edge_cnt.items():
            if len(lst) != 1: continue
            n = lst[0]; a, b = P[x], P[y]
            e = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            m = (e[1] * n[2] - e[2] * n[1], e[2] * n[0] - e[0] * n[2], e[0] * n[1] - e[1] * n[0]); l = math.sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2])
            if l < 1e-14: continue
            m = (m[0] / l, m[1] / l, m[2] / l); q = plane_q(m, -(m[0] * a[0] + m[1] * a[1] + m[2] * a[2]), 25.0 * (W[x] + W[y]) * 0.5)
            for v in (x, y):
                Qv = Q[v]
                for k in range(10): Qv[k] += q[k]
    del edge_cnt
    def accent(c):      # 鲜艳程度按显示出来的颜色（sRGB）算：米色的石头大约 0.2，金、蓝、彩虹在 0.5 以上
        mx = lin_to_srgb(max(c)); mn = lin_to_srgb(min(c))
        return mx > 0.08 and (mx - mn) / mx > accent_sat
    acc = [accent(c) for c in C]
    eps2 = eps * eps; wc = eps2 / (0.12 * 0.12)
    alive = [True] * nv; ver = [0] * nv
    def cost(u, v):
        if acc[u] != acc[v]: return 1e30
        qu, qv = Q[u], Q[v]; x, y, z = P[v]
        e = ((qu[0] + qv[0]) * x * x + 2 * (qu[1] + qv[1]) * x * y + 2 * (qu[2] + qv[2]) * x * z + 2 * (qu[3] + qv[3]) * x + (qu[4] + qv[4]) * y * y
             + 2 * (qu[5] + qv[5]) * y * z + 2 * (qu[6] + qv[6]) * y + (qu[7] + qv[7]) * z * z + 2 * (qu[8] + qv[8]) * z + qu[9] + qv[9])
        e = e / (W[u] + W[v]) if e > 0.0 else 0.0
        if acc[u]:
            cu, cv = C[u], C[v]
            e += wc * ((cu[0] - cv[0]) ** 2 + (cu[1] - cv[1]) ** 2 + (cu[2] - cv[2]) ** 2)
        pu = P[u]
        return e + 1e-9 * ((pu[0] - x) ** 2 + (pu[1] - y) ** 2 + (pu[2] - z) ** 2)
    heap = []
    for v in range(nv):
        nb = set()
        for i in vt[v]: nb.update(tris[i])
        nb.discard(v)
        for w in nb:
            c1 = cost(v, w)
            if c1 <= eps2: heap.append((c1, v, w, 0, 0))
    heapq.heapify(heap)
    push = heapq.heappush; pop = heapq.heappop
    while heap:
        c, u, v, su, sv = pop(heap)
        if c > eps2: break
        if not (alive[u] and alive[v]) or ver[u] != su or ver[v] != sv: continue
        shared = vt[u] & vt[v]
        if not shared: continue
        # 并掉以后，u 周围剩下的面不能翻过去、不能缩成一条线
        ok = True; pv = P[v]
        for i in vt[u]:
            if i in shared: continue
            t = tris[i]
            a, b, c3 = P[t[0]], P[t[1]], P[t[2]]
            n0 = tri_n(a, b, c3)
            a2 = pv if t[0] == u else a; b2 = pv if t[1] == u else b; c2 = pv if t[2] == u else c3
            n1 = tri_n(a2, b2, c2)
            l0 = n0[0] * n0[0] + n0[1] * n0[1] + n0[2] * n0[2]; l1 = n1[0] * n1[0] + n1[1] * n1[1] + n1[2] * n1[2]
            if l1 < 1e-16 or (n0[0] * n1[0] + n0[1] * n1[1] + n0[2] * n1[2]) < 0.5 * math.sqrt(l0 * l1): ok = False; break
        if not ok: continue
        for i in shared:
            for w in tris[i]: vt[w].discard(i)
            tris[i] = None
        for i in vt[u]:
            t = tris[i]
            if t[0] == u: t[0] = v
            elif t[1] == u: t[1] = v
            else: t[2] = v
            vt[v].add(i)
        vt[u] = set(); alive[u] = False
        Qu, Qv = Q[u], Q[v]; W[v] += W[u]
        for k in range(10): Qv[k] += Qu[k]
        ver[v] += 1
        nb = set()
        for i in vt[v]: nb.update(tris[i])
        nb.discard(v)
        for w in nb:
            c1 = cost(v, w)
            if c1 <= eps2: push(heap, (c1, v, w, ver[v], ver[w]))
            c2 = cost(w, v)
            if c2 <= eps2: push(heap, (c2, w, v, ver[w], ver[v]))
    used = {}; OP = []; OC = []; NT = []; seen = set()
    for t in tris:
        if t is None or t[0] == t[1] or t[1] == t[2] or t[0] == t[2]: continue
        key = tuple(sorted(t))
        if key in seen: continue
        seen.add(key)
        for v in t:
            q = used.get(v)
            if q is None: q = used[v] = len(OP); OP.append(P[v]); OC.append(C[v])
            NT.append(q)
    return OP, OC, NT

def add_cylinder(P, C, T, base, axis, radius, height, segs, color, caps=(True, True)):
    """加一个规整的圆柱（圆台座的一级、圆盘）：base = 底面圆心，axis = 朝哪边长（单位向量），caps = 底面 / 顶面做不做。"""
    ax = axis
    ref = (0.0, 1.0, 0.0) if abs(ax[1]) < 0.9 else (1.0, 0.0, 0.0)
    u = (ax[1] * ref[2] - ax[2] * ref[1], ax[2] * ref[0] - ax[0] * ref[2], ax[0] * ref[1] - ax[1] * ref[0])
    l = math.sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]); u = (u[0] / l, u[1] / l, u[2] / l)
    v = (ax[1] * u[2] - ax[2] * u[1], ax[2] * u[0] - ax[0] * u[2], ax[0] * u[1] - ax[1] * u[0])      # u × v = axis
    top = (base[0] + ax[0] * height, base[1] + ax[1] * height, base[2] + ax[2] * height)
    k = len(P)
    for i in range(segs):
        a = 2.0 * math.pi * i / segs; ca, sa = math.cos(a) * radius, math.sin(a) * radius
        d = (u[0] * ca + v[0] * sa, u[1] * ca + v[1] * sa, u[2] * ca + v[2] * sa)
        P.append((base[0] + d[0], base[1] + d[1], base[2] + d[2])); P.append((top[0] + d[0], top[1] + d[1], top[2] + d[2])); C.append(color); C.append(color)
    for i in range(segs):
        b0, t0, b1, t1 = k + 2 * i, k + 2 * i + 1, k + 2 * ((i + 1) % segs), k + 2 * ((i + 1) % segs) + 1
        T.extend((b0, b1, t1, b0, t1, t0))
    # 两个端面各用自己的一圈点（和侧面不共用：按面着色时无所谓，平滑着色时棱才是硬的）
    for is_top, on in ((False, caps[0]), (True, caps[1])):
        if not on: continue
        c0 = top if is_top else base; kc = len(P)
        P.append(c0); C.append(color)
        for i in range(segs):
            a = 2.0 * math.pi * i / segs; ca, sa = math.cos(a) * radius, math.sin(a) * radius
            P.append((c0[0] + u[0] * ca + v[0] * sa, c0[1] + u[1] * ca + v[1] * sa, c0[2] + u[2] * ca + v[2] * sa)); C.append(color)
        for i in range(segs):
            a_, b_ = kc + 1 + i, kc + 1 + (i + 1) % segs
            T.extend((kc, a_, b_) if is_top else (kc, b_, a_))

def round_disc(P, C, T, name):
    """立像手里那面圆盘换成一个规整的圆盘。圆盘的正面是整件东西上最大的一块平面：按“法线 + 离原点多远”把三角形归堆，面积最大的那一堆就是它——
    量出圆心、朝向、半径，原来盘上的面（落在这个圆盘范围里、颜色不是石头那种黄的）丢掉，另做一个 40 边的圆盘。
    （试过按颜色认——盘是白的——减面以后盘上只剩几十个点，还混着发饰上的白点，圆心和半径都量不准。）"""
    tris = []; bins = {}
    for k in range(0, len(T) - 2, 3):
        a, b, c = P[T[k]], P[T[k + 1]], P[T[k + 2]]
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        l = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
        if l < 1e-12: tris.append(None); continue
        n = (n[0] / l, n[1] / l, n[2] / l); d = n[0] * a[0] + n[1] * a[1] + n[2] * a[2]
        tris.append((n, d, l * 0.5, k))
        if n[2] > 0.5:      # 只看朝正面的
            key = (round(n[0] * 16), round(n[1] * 16), round(n[2] * 16), round(d / 0.012))
            bins[key] = bins.get(key, 0.0) + l * 0.5
    if not bins: return P, C, T, None
    best = max(bins, key=lambda q: bins[q])
    # 这一堆的平均法线、平均离原点多远；再把贴着这个面的都收进来
    seed = [t for t in tris if t and t[0][2] > 0.5 and (round(t[0][0] * 16), round(t[0][1] * 16), round(t[0][2] * 16), round(t[1] / 0.012)) == best]
    A = sum(t[2] for t in seed)
    n = tuple(sum(t[0][q] * t[2] for t in seed) / A for q in range(3)); l = math.sqrt(sum(v * v for v in n)); n = tuple(v / l for v in n)
    D = sum(t[1] * t[2] for t in seed) / A
    face = [t for t in tris if t and t[0][0] * n[0] + t[0][1] * n[1] + t[0][2] * n[2] > 0.995 and abs(t[1] - D) < 0.008]
    A = sum(t[2] for t in face)
    cen = [0.0, 0.0, 0.0]; verts = set()
    for t in face:
        k = t[3]; g = [(P[T[k]][q] + P[T[k + 1]][q] + P[T[k + 2]][q]) / 3.0 for q in range(3)]
        for q in range(3): cen[q] += g[q] * t[2]
        verts.update((T[k], T[k + 1], T[k + 2]))
    cen = tuple(v / A for v in cen)
    rr = sorted(rad_of([P[i] for i in verts], cen, n))
    R = rr[-1]
    # 圆心：边上那一圈点的外接圆——先用面积重心，再按“最远的点一样远”修两遍
    for _ in range(4):
        far = [P[i] for i in verts if rad_of([P[i]], cen, n)[0] > 0.9 * R]
        if len(far) < 6: break
        c2 = tuple(sum(q_[k] for q_ in far) / len(far) for k in range(3))
        off = (c2[0] - cen[0]) * n[0] + (c2[1] - cen[1]) * n[1] + (c2[2] - cen[2]) * n[2]
        cen = (c2[0] - n[0] * off, c2[1] - n[1] * off, c2[2] - n[2] * off)
        R = max(rad_of([P[i] for i in verts], cen, n))
    thick = 0.07
    def sat(c):
        mx = lin_to_srgb(max(c)); mn = lin_to_srgb(min(c))
        return (mx - mn) / mx if mx > 1e-4 else 0.0
    col = tuple(sum(C[i][k] for i in verts) / len(verts) for k in range(3))
    keep = []; gone = 0
    for k in range(0, len(T) - 2, 3):
        a, b, c = P[T[k]], P[T[k + 1]], P[T[k + 2]]
        g = ((a[0] + b[0] + c[0]) / 3.0, (a[1] + b[1] + c[1]) / 3.0, (a[2] + b[2] + c[2]) / 3.0)
        d = (g[0] - cen[0]) * n[0] + (g[1] - cen[1]) * n[1] + (g[2] - cen[2]) * n[2]      # 正面是 0，往盘里是负的
        r_ = math.sqrt(max(0.0, sum((g[q] - cen[q]) ** 2 for q in range(3)) - d * d))
        if -0.13 < d < 0.02 and r_ < R * 1.08 and (sat(C[T[k]]) + sat(C[T[k + 1]]) + sat(C[T[k + 2]])) / 3.0 < 0.17: gone += 1
        else: keep.extend((T[k], T[k + 1], T[k + 2]))
    P = list(P); C = list(C)
    base = (cen[0] - n[0] * thick, cen[1] - n[1] * thick, cen[2] - n[2] * thick)
    add_cylinder(P, C, keep, base, n, R, thick, 40, col)
    print("    （%s 的圆盘：正面的圆心 (%.3f, %.3f, %.3f)，朝向 (%.3f, %.3f, %.3f)，半径 %.3f，这块平面 %.2f m²（圆 %.2f m²），换掉原来的 %d 个面）" % (name, cen[0], cen[1], cen[2], n[0], n[1], n[2], R, A, math.pi * R * R, gone))
    return P, C, keep, (cen, n, R)

def flat_mirror(P, C, T, name):
    """三相像手里那面镜子：镜面在贴图上是一片发蓝的反光、带一个放射状的花纹（转成顶点色以后在游戏里是一道道深浅）。
    2026-10-11 用户：镜子要是一个平整的石面，花纹不要。——镜面本来就是平的，花纹只是颜色：按发蓝的点找出镜面（圆心、朝向、半径），
    这个圆里贴着镜面的点一律改成标准的石头灰、压到同一个平面上。镜框（外面那一圈凸起来的边）不动。
    要在减面之前做：颜色统一了以后，减面才会把镜面并成几块大平面（不然花纹的颜色会被当成“点缀”护着）。"""
    blue = [i for i, c in enumerate(C) if c[2] > 1.25 * c[0] and c[2] > 0.08]
    if len(blue) < 30:
        print("    （%s：发蓝的点只有 %d 个，没找到镜面）" % (name, len(blue))); return P, C, None
    cen = tuple(sum(P[i][k] for i in blue) / len(blue) for k in range(3))
    bset = set(blue)
    n = [0.0, 0.0, 0.0]
    for k in range(0, len(T) - 2, 3):
        if T[k] in bset and T[k + 1] in bset and T[k + 2] in bset:
            a, b, c = P[T[k]], P[T[k + 1]], P[T[k + 2]]
            e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            m = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
            s = 1.0 if m[2] >= 0.0 else -1.0      # 都按朝正面的算
            n[0] += m[0] * s; n[1] += m[1] * s; n[2] += m[2] * s
    l = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
    if l < 1e-12: return P, C, None
    n = (n[0] / l, n[1] / l, n[2] / l)
    # 只留贴着这个平面的发蓝的点，再量一遍圆心和半径
    on = [i for i in blue if abs((P[i][0] - cen[0]) * n[0] + (P[i][1] - cen[1]) * n[1] + (P[i][2] - cen[2]) * n[2]) < 0.02]
    cen = tuple(sum(P[i][k] for i in on) / len(on) for k in range(3))
    rr = sorted(rad_of([P[i] for i in on], cen, n))
    R = rr[int(len(rr) * 0.97)] * 1.03
    P = list(P); C = list(C); done = 0
    for i, p in enumerate(P):
        d = (p[0] - cen[0]) * n[0] + (p[1] - cen[1]) * n[1] + (p[2] - cen[2]) * n[2]
        r_ = math.sqrt(max(0.0, sum((p[k] - cen[k]) ** 2 for k in range(3)) - d * d))
        c = C[i]
        if abs(d) <= 0.03 and r_ <= R:      # 镜面上的点：压平、改成石头灰（镜面中间比边上凹一点，容差给到 3 cm）
            P[i] = (p[0] - n[0] * d, p[1] - n[1] * d, p[2] - n[2] * d)
            C[i] = (STONE_GREY, STONE_GREY, STONE_GREY); done += 1
        elif c[2] > 1.12 * c[0] and c[2] > 0.06 and r_ <= R * 1.6 and abs(d) < 0.12:      # 镜子附近剩下的发蓝的点（镜框内沿、反光溅到的地方）：只改颜色
            C[i] = (STONE_GREY, STONE_GREY, STONE_GREY); done += 1
    # 2026-10-11 用户：镜子有点缺，也不是正圆——镜面抹平以后边上还是美术做的那一圈花边（一瓣一瓣、凸一块凹一块的），减面以后缺缺丫丫。
    # 镜子（连镜框）整个换成一块规整的圆板（mirror_plate，减面以后再加）。这里先把原来镜子、镜框上的点前后压进板的厚度里：
    # 量出来镜框到镜面半径的 1.2 倍为止、前后各 2–3 cm。压扁的只是这一片里的点（托着镜子的手指贴着镜框的那一面也跟着贴到板上，
    # 手指别的地方不动），它们都落在新圆板的里面，外面看不见——不删面，所以不会漏洞。
    squashed = 0
    for i, p in enumerate(P):
        d = (p[0] - cen[0]) * n[0] + (p[1] - cen[1]) * n[1] + (p[2] - cen[2]) * n[2]
        r_ = math.sqrt(max(0.0, sum((p[k] - cen[k]) ** 2 for k in range(3)) - d * d))
        if r_ <= R * MIRROR_PLATE_R and abs(d) <= 0.036 and abs(d) > MIRROR_PLATE_HALF - 0.003:
            t = (MIRROR_PLATE_HALF - 0.003) * (1.0 if d > 0 else -1.0) - d
            P[i] = (p[0] + n[0] * t, p[1] + n[1] * t, p[2] + n[2] * t); squashed += 1
    print("    （%s 的镜面：圆心 (%.3f, %.3f, %.3f)，朝向 (%.3f, %.3f, %.3f)，半径 %.3f；%d 个点抹成了石头面，%d 个点压进圆板里）" % (name, cen[0], cen[1], cen[2], n[0], n[1], n[2], R, done, squashed))
    return P, C, (cen, n, R)

# 新圆板：半径 = 镜面半径 × MIRROR_PLATE_R（盖住原来的镜框），前后各厚 MIRROR_PLATE_HALF（米，像还没放大之前的尺寸；游戏里再乘 1.567）
MIRROR_PLATE_R, MIRROR_PLATE_HALF = 1.24, 0.013
def mirror_plate(P, C, T, mir):
    cen, n, R = mir
    P = list(P); C = list(C); T = list(T)
    base = (cen[0] - n[0] * MIRROR_PLATE_HALF, cen[1] - n[1] * MIRROR_PLATE_HALF, cen[2] - n[2] * MIRROR_PLATE_HALF)
    add_cylinder(P, C, T, base, n, R * MIRROR_PLATE_R, 2.0 * MIRROR_PLATE_HALF, 56, (STONE_GREY, STONE_GREY, STONE_GREY))
    return P, C, T

def rad_of(pts, cen, n):
    out = []
    for q in pts:
        d = (q[0] - cen[0]) * n[0] + (q[1] - cen[1]) * n[1] + (q[2] - cen[2]) * n[2]
        out.append(math.sqrt(max(0.0, sum((q[k] - cen[k]) ** 2 for k in range(3)) - d * d)))
    return out

def normals(P, T):
    nx = [0.0] * len(P); ny = [0.0] * len(P); nz = [0.0] * len(P)
    for k in range(0, len(T) - 2, 3):
        a, b, c = P[T[k]], P[T[k + 1]], P[T[k + 2]]
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        x = e1[1] * e2[2] - e1[2] * e2[1]; y = e1[2] * e2[0] - e1[0] * e2[2]; z = e1[0] * e2[1] - e1[1] * e2[0]
        for v in (T[k], T[k + 1], T[k + 2]): nx[v] += x; ny[v] += y; nz[v] += z
    out = []
    for i in range(len(P)):
        l = math.sqrt(nx[i] * nx[i] + ny[i] * ny[i] + nz[i] * nz[i]) or 1.0
        out.append((nx[i] / l, ny[i] / l, nz[i] / l))
    return out

def flatten(P, C, T):
    """把网格拆成“一个面一个颜色、一个法线”的样子（低模的平着色）。"""
    FP = []; FN = []; FC = []; FT = []
    for k in range(0, len(T) - 2, 3):
        a, b, c = P[T[k]], P[T[k + 1]], P[T[k + 2]]
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2]); e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        l = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
        if l < 1e-12: continue
        n = (n[0] / l, n[1] / l, n[2] / l)
        ca, cb, cc = C[T[k]], C[T[k + 1]], C[T[k + 2]]
        col = ((ca[0] + cb[0] + cc[0]) / 3.0, (ca[1] + cb[1] + cc[1]) / 3.0, (ca[2] + cb[2] + cc[2]) / 3.0)
        base = len(FP)
        FP.extend((a, b, c)); FN.extend((n, n, n)); FC.extend((col, col, col)); FT.extend((base, base + 1, base + 2))
    return FP, FN, FC, FT

def write_glb(path, P, N, C, T, name):
    lo = [min(p[k] for p in P) for k in range(3)]; hi = [max(p[k] for p in P) for k in range(3)]
    bp = struct.pack("<%df" % (len(P) * 3), *[v for p in P for v in p])
    bn = struct.pack("<%df" % (len(N) * 3), *[v for p in N for v in p])
    # 顶点色按 glTF 的规矩存线性值（16 位，免得暗的颜色糊成一档）
    bc = struct.pack("<%dH" % (len(C) * 4), *[v for c in C for v in (min(65535, int(max(0.0, c[0]) * 65535 + 0.5)), min(65535, int(max(0.0, c[1]) * 65535 + 0.5)), min(65535, int(max(0.0, c[2]) * 65535 + 0.5)), 65535)])
    bi = struct.pack("<%dI" % len(T), *T)
    chunks = [bp, bn, bc, bi]; views = []; off = 0; blob = b""
    for b in chunks:
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(b)}); blob += b
        pad = (4 - len(blob) % 4) % 4; blob += b"\x00" * pad; off = len(blob)
    views[0]["target"] = views[1]["target"] = views[2]["target"] = 34962; views[3]["target"] = 34963
    j = {"asset": {"version": "2.0", "generator": "Dysis convert_mech_pack"}, "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": name}],
         "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2}, "indices": 3, "material": 0}]}],
         "materials": [{"name": "MechPackVertexColor", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 0.9}}],
         "buffers": [{"byteLength": len(blob)}], "bufferViews": views,
         "accessors": [{"bufferView": 0, "componentType": 5126, "count": len(P), "type": "VEC3", "min": lo, "max": hi},
                       {"bufferView": 1, "componentType": 5126, "count": len(N), "type": "VEC3"},
                       {"bufferView": 2, "componentType": 5123, "normalized": True, "count": len(C), "type": "VEC4"},
                       {"bufferView": 3, "componentType": 5125, "count": len(T), "type": "SCALAR"}]}
    js = json.dumps(j, separators=(",", ":")).encode("utf-8"); js += b" " * ((4 - len(js) % 4) % 4)
    open(path, "wb").write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(blob)) + struct.pack("<I4s", len(js), b"JSON") + js + struct.pack("<I4s", len(blob), b"BIN\x00") + blob)

# ───────────────────────── 各件怎么做 ─────────────────────────
WALL_R = 15.50      # 内墙面的半径（米）：浮雕顺着它弯
STONE_GREY = 0.42   # “标准的石头灰”：材质 M_DysisMechPack 里的 LumRef，这么亮的灰在游戏里正好就是墙的颜色
def has(*words): return lambda n: any(w in n for w in words)
def hasnt(*words): return lambda n: not any(w in n for w in words)
ALL = lambda n: True

# name：输出的件名；src：哪个 glb；want：要哪些节点；kind：statue（立像，脚底中心放原点）/ relief（浮雕，背板面放在 z = 0、顺着墙弯）/ part（零件，保持和同组别的件的相对位置）；
# size：("h", 米) 按高缩放 / ("w", 米) 按宽 / ("k", 倍数) 直接给倍数；group：同一组的件用同一个倍数、同一个原点（按组里 ref 那一件定）；cell：合并顶点的格子（米，0 = 不压）
PIECES = [
    # eps：保平面减面允许偏离多少米（见 simplify）。带 eps 的件在这里就减到位了，导入时不再让引擎减（2026-10-10 用户：双子、伊莉丝、塞勒涅“棱角太多、成了碎块”，
    # 要照外立面神像那样——外立面神像之所以好，是它原有的大平面碰巧在引擎按面数硬减时保住了；这几件没保住）
    # 2026-10-11 用户：三相像、双子、瀑布后女神的棱角不要那么分明（尤其披风、衣服），像塞勒涅浮雕那样。
    # ——和塞勒涅一样加上 crease=2（护住弯的棱线：衣褶细一些、不再是几大块硬面），游戏里再把这几尊的法线调软一点（材质的 Facet，见 DysisDirectorMechPack.cpp）。
    # 三相像手里的镜子：镜面抹成平整的石头面（flat_mirror）。
    dict(name="MirrorSun",  src="三相像_上", want=ALL, kind="statue", size=("k", 1.50 / 1.90), cell=0.004, eps=0.002, crease=2.0, fix_black=True, flat_mirror=True),
    dict(name="MirrorMoon", src="三相像_中", want=ALL, kind="statue", size=("k", 1.50 / 1.90), cell=0.004, eps=0.002, crease=2.0, fix_black=True, flat_mirror=True),
    dict(name="MirrorDark", src="三相像_下", want=ALL, kind="statue", size=("k", 1.50 / 1.90), cell=0.004, eps=0.002, crease=2.0, fix_black=True, flat_mirror=True),
    # 四层的天鹅女神像（变成天鹅之前的人形，2026-10-11 美术补的《天鹅湖_女神.glb》）：原模型连两级方座高 1.9 m。
    # 方座不要（她站在天鹅那个圆台座上）：放大到“连座 2.2 m”再把 25.5 cm 的座切掉，人自己高 1.95 m（灰盒那尊就这么高）。
    dict(name="SwanGoddess", src="天鹅湖_女神", want=ALL, kind="statue", size=("h", 2.20), cell=0.004, eps=0.002, crease=2.0, fix_black=True, cut_base=0.255),
    # 瀑布后面的女神（2026-10-10 用户：怀里那面圆的内部有点碎，修成平整的圆面；底座和手里的月亮都改成正圆）：
    # 也改成保平面的减法；怀里的圆盘、脚下三级的台座另做成规整的圆（round_disc / round_base）
    # 2026-10-11 用户：下面那两层台子不要了，换成一层小一点的。——这里干脆不带台座（脚底在 0），台座在游戏里另放一个圆台（要多大、多高好调）。
    # 倍数还是按“连原来的台座一共 5.40 m”算的：人自己高 4.815 m，怀里圆盘的圆心离脚 3.368 m、在身子的轴线右边 0.638 m、前面 0.485 m，半径 0.514 m
    dict(name="Goddess",    src="瀑布后女神", want=ALL, kind="statue", size=("h", 5.40), cell=0.008, eps=0.0028, crease=2.0, fix_black=True, round_disc=True, cut_base=0.585),
    # 外立面神像：2026-10-10 用户要把脚下那块方底座撤掉（人直接站在窗台上、往墙里收），所以也改成在这里减面
    dict(name="FacadeStatue", src="外立面神像", want=ALL, kind="statue", size=("h", 3.20), cell=0.006, eps=0.003, cut_base=0.20, fix_black=True),
    # 双子：原来方的台座换成圆的（用户要的）
    dict(name="TwinCastor", src="双子_左", want=ALL, kind="statue", size=("k", 2.15 / 1.895), cell=0.004, eps=0.002, crease=2.0, fix_black=True,
         cut_base=0.27, round_base=[(0.48, 0.0, 0.10), (0.44, 0.10, 0.19), (0.40, 0.19, 0.27)]),
    dict(name="TwinPollux", src="双子_右", want=ALL, kind="statue", size=("k", 2.15 / 1.895), cell=0.004, eps=0.002, crease=2.0, fix_black=True,
         cut_base=0.23, round_base=[(0.61, 0.0, 0.11), (0.57, 0.11, 0.23)]),
    # 浮雕：背板整个不要（drop_back：贴在墙面上不到这么高的面都丢掉），墙自己当背板；上下那两道横框、两侧的边框也整个压平丢掉（frame = 两侧 / 底 / 顶各多宽，米）；
    # 背板不是绝对平的（塞勒涅的背板本身就是一块块高低差一两厘米的面），bias 是整体再往墙里沉多少，沉到背板全在墙面以下为止（用 zhist=1 查）
    # 2026-10-10 用户：这两块的边上还有凸出来的边（边框没压干净的残条）→ 边框压平的范围加宽，贴着墙面不到 1 cm 高的薄片也丢掉；
    # 塞勒涅放大到 1.393 倍（3.62 m 高）：眼睛的位置是灰盒定死的，放大以后人的脚正好落到地面上（各个尺寸都跟着乘了 1.393）
    # 2026-10-11 用户：塞勒涅左边和上边的弧太生硬、伊莉丝的彩虹也是，还缺了一小块；伊莉丝整体不如别的几件自然。
    #   · crease=2：减面的时候护住弯的棱线（见 simplify）——那几道弧原来被并成了五六段折线；面数多一倍（塞勒涅 5 千 → 1.2 万，伊莉丝 0.9 万 → 1.7 万）；
    #   · 伊莉丝：顶框一刀切（hard_top，框高 10 cm）、背板丢得浅一点（4 mm：彩虹只凸出来七八毫米，原来 8 mm 的线把它丢成一截一截的）；
    #     整块放大到 1.15 倍（人太小了，一米出头）——各个尺寸都跟着乘了 1.15。
    dict(name="Selene",     src="塞勒涅", want=ALL, kind="relief", size=("h", 2.60 * 1.393), cell=0.0055, eps=0.0028, crease=2.0, drop_back=0.010, bias=0.036, frame=(0.0975, 0.195, 0.195), edge=0.02 / 1.6, islands_after=2500),
    dict(name="IrisRelief", src="伊莉丝与彩虹浮雕", want=ALL, kind="relief", size=("w", 2.34 * 1.15), cell=0.0046, eps=0.0023, crease=2.0, drop_back=0.0046, bias=0.0035, frame=(0.0575, 0.161, 0.115), hard_top=True,
         edge=0.015 / 2.34, islands_after=120),
    dict(name="SwanRelief", src="四小天鹅浮雕", want=ALL, kind="relief", size=("w", 4.20), cell=0.004, eps=0.002, drop_back=0.0015, bias=0.006, frame=(0.05, 0.26, 0.17), edge=0.02 / 4.2, islands_after=800),
    # 神龛通体是金色的：accent_sat 调到 9 = 不把金色当“点缀”去护着（不然贴图上的深浅会挡住合并，一万多面减不下去）
    dict(name="NicheBox",   src="神龛_底部", want=ALL, kind="statue", size=("w", 0.80), cell=0.003, eps=0.0035, accent_sat=9.0),
    dict(name="NicheLid",   src="神龛_顶部", want=ALL, kind="statue", size=("w", 0.80), cell=0.003, eps=0.0035, accent_sat=9.0),
    # 拱门：拱和门各一个文件，但摆的是拼在一起的位置（门在拱洞里）——都不挪原点，装的时候两件放同一处就对上了
    # （2026-10-10 用户：拱门看着太碎，面数和殿里的柱子差不多就行 → 允许偏离放到 1–1.5 cm，拱 470 面、门 330 面）
    # （2026-10-11 用户又说：拱的边缘弄平滑一点、或者干脆就是弧面，再缩窄一点，现在看着臃肿。——美术这座拱不用了，
    #   GateArch / GateDoor 改成按尺寸现做的素石拱和门板，见 make_extra_shapes.py。下面两行留着备查，不再转。）
    # dict(name="GateArch",   src="拱门_拱", want=ALL, kind="part", origin=(0.0, 0.0, 0.0), k=1.0, cell=0.008, eps=0.010, fix_black=True),
    # dict(name="GateDoor",   src="拱门_门", want=ALL, kind="part", origin=(0.0, 0.0, 0.0), k=1.0, cell=0.008, eps=0.015, fix_black=True),
    # 金苹果（2026-10-10 用户：面数再少一些、小巧精致一些 → 高 14 cm、340 面）。原点放在正中：游戏里是按球心摆的
    dict(name="Apple",      src="golden_apple_lowpoly_leaf", want=ALL, kind="statue", size=("h", 0.14), cell=0, eps=0.0015, accent_sat=9.0, center=True),
    # ── 下面四个是要拆开的低模（没有贴图，颜色就是材质的颜色；不压面）。kind="part"：origin 是原模型里的哪一点放到原点，k 是倍数（可以三个方向不一样） ──
    # 拉杆：底座一件；会扳动的那一组（"Lever assembly" 底下）一件，原点放在转轴上（原模型 y = 0.98）
    dict(name="LeverBase", src="拉杆_初定", want=hasnt("Lever assembly"), kind="part", origin=(0.0, 0.0, 0.0), k=0.38, cell=0),
    dict(name="LeverArm",  src="拉杆_初定", want=has("Lever assembly"), kind="part", origin=(0.0, 0.98, 0.0), k=0.38, cell=0, upright=True),   # 原模型里杆是扳过去的样子：扶正，游戏里它从竖直开始扳
    # 棱镜：石膏台座一件（原点在脚底）；会转的那一组（"旋转控制" 底下）除了水晶一件、水晶单独一件，原点都放在转轴上水晶的底（原模型 y = 2.523）。倍数取 0.1665：水晶的中心正好离地 0.575 m（灰盒里色光从这个高度出来）
    dict(name="PrismBase",    src="水晶棱镜_初定", want=lambda n: "旋转控制" not in n and "展示" not in n and "参考" not in n, kind="part", origin=(0.0, 0.0, 0.0), k=0.1665, cell=0),
    dict(name="PrismSpin",    src="水晶棱镜_初定", want=lambda n: "旋转控制" in n and "光学水晶" not in n, kind="part", origin=(0.0, 2.523, 0.0), k=0.1665, cell=0),
    dict(name="PrismCrystal", src="水晶棱镜_初定", want=has("光学水晶"), kind="part", origin=(0.0, 2.523, 0.0), k=0.1665, cell=0),
    # 月门：门板一件、门框（连两只脚）一件、月牙一件。原点 = 门板底边中点、门板朝外的那个面。横着缩到门洞宽（1.285 m）、竖着缩到门洞高（2.40 m）
    # （门板原来是深紫的黑曜石。2026-10-10 用户：不要深紫色，和周围的墙一个颜色——这里把它的颜色换成中性的灰，材质会把没有颜色的石头统一成墙的颜色）
    dict(name="MoonGateSlab",     src="月门_初定", want=has("Door |"), kind="part", origin="slab", k=(1.285 / 1.78, 2.40 / 3.09, 1.285 / 1.78), cell=0, color=(0.55, 0.55, 0.55)),
    dict(name="MoonGateFrame",    src="月门_初定", want=lambda n: ("Frame |" in n or "Foot |" in n or "柱体" in n), kind="part", origin="slab", k=(1.285 / 1.78, 2.40 / 3.09, 1.285 / 1.78), cell=0),
    # （月牙也改成现做的了——美术那枚是几块大平面拼的，外弧是折线；见 make_extra_shapes.py）
    # dict(name="MoonGateCrescent", src="月门_初定", want=has("Crescent"), kind="part", origin="slab", k=(1.285 / 1.78, 2.40 / 3.09, 1.285 / 1.78), cell=0),
    # 天鹅：圆台座一件（竖着拉高到 0.5 m，和原来的台座一样高）；天鹅一件，原点在台座顶面的中心（原模型 y = 0.76）
    dict(name="SwanPlinth", src="天鹅_再定", want=has("plinth", "Laurel", "drum flute", "bead molding", "mounting disk", "Rim diamond"), kind="part", origin=(0.0, 0.0, 0.0), k=(0.42, 0.50 / 0.76, 0.42), cell=0),
    dict(name="SwanBird",   src="天鹅_再定", want=hasnt("plinth", "Laurel", "drum flute", "bead molding", "mounting disk", "Rim diamond"), kind="part", origin=(0.0, 0.76, 0.0), k=0.42, cell=0),
]

def bounds(P):
    return [min(p[k] for p in P) for k in range(3)], [max(p[k] for p in P) for k in range(3)]

def convert(pc, src_dir, out_dir, report):
    key = pc["src"]
    j, bin_ = load(os.path.join(src_dir, key + ".glb"))
    P, C, T = gather(j, bin_, os.path.join(out_dir, "tex"), key, pc["want"])
    if not P: print("  %s：没有挑到任何节点" % pc["name"]); return
    if pc.get("color"): C = [pc["color"]] * len(P)
    lo, hi = bounds(P)
    w, h, d = hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]
    if pc["kind"] == "part":
        org = pc["origin"]
        if org == "slab":      # 月门：门板底边中点、门板最靠外（+z）的那个面
            SP, _, _ = gather(j, bin_, os.path.join(out_dir, "tex"), key, has("Door |"))
            slo, shi = bounds(SP); org = ((slo[0] + shi[0]) * 0.5, slo[1], shi[2])
        kk = pc["k"] if isinstance(pc["k"], tuple) else (pc["k"],) * 3
        P = [((p[0] - org[0]) * kk[0], (p[1] - org[1]) * kk[1], (p[2] - org[2]) * kk[2]) for p in P]
        if pc.get("upright"):
            top = max(p[1] for p in P); hi_pts = [p for p in P if p[1] > 0.8 * top]; lo_pts = [p for p in P if abs(p[1]) < 0.12 * top]
            az = sum(p[2] for p in hi_pts) / len(hi_pts) - sum(p[2] for p in lo_pts) / len(lo_pts); ay = sum(p[1] for p in hi_pts) / len(hi_pts) - sum(p[1] for p in lo_pts) / len(lo_pts)
            ang = math.atan2(az, ay); ca, sa = math.cos(ang), math.sin(ang)
            P = [(p[0], p[1] * ca + p[2] * sa, -p[1] * sa + p[2] * ca) for p in P]
            print("    （%s 扶正了 %.1f°）" % (pc["name"], math.degrees(ang)))
        P, C, T = cluster(P, C, T, pc["cell"])
        if pc.get("eps"):
            n0 = len(T) // 3; P, C, T = simplify(P, C, T, pc["eps"], accent_sat=pc.get("accent_sat", 0.38)); print("    （%s 保平面减面：%d → %d 面）" % (pc["name"], n0, len(T) // 3))
        if pc.get("flat", False): OP_, N, OC_, OT_ = flatten(P, C, T)
        else: OP_, N, OC_, OT_ = P, normals(P, T), C, T
        write_glb(os.path.join(out_dir, pc.get("out", pc["name"]) + ".glb"), OP_, N, OC_, OT_, pc["name"])
        lo2, hi2 = bounds(P)
        line = "%-16s 顶点 %6d 三角 %6d  倍数 %s  x %.3f…%.3f  y %.3f…%.3f  z %.3f…%.3f  原点取原模型的 (%.3f, %.3f, %.3f)" % (pc["name"], len(P), len(T) // 3, "×".join("%.3f" % v for v in kk), lo2[0], hi2[0], lo2[1], hi2[1], lo2[2], hi2[2], org[0], org[1], org[2])
        if pc["name"] == "GateArch":
            mid = [abs(p[0]) for p in P if 0.3 < p[1] < 0.9]; cen = [p[1] for p in P if abs(p[0]) < 0.05]
            line += "  拱洞：半宽 %.3f、洞顶 %.3f" % (min(mid), min(cen))
        print("  " + line); report.append(line)
        return
    mode, val = pc["size"]
    k = val / h if mode == "h" else (val / w if mode == "w" else val)
    # 原点：立像 = 脚底中心；浮雕 = 底边中点、背板那个面
    cx = (lo[0] + hi[0]) * 0.5; cz = (lo[2] + hi[2]) * 0.5; y0 = (lo[1] + hi[1]) * 0.5 if pc.get("center") else lo[1]; back = None
    if pc["kind"] == "relief":
        # 背板的面在哪：把深度分成细档，点最多的那一档（浮雕的底面一大片都在同一个深度上）
        bins = {}
        for p in P[::3]:
            b = round(p[2] / (d / 200.0)); bins[b] = bins.get(b, 0) + 1
        best = max(bins, key=lambda b: bins[b]); back = best * (d / 200.0)
        cz = back
    P = [((p[0] - cx) * k, (p[1] - y0) * k, (p[2] - cz) * k) for p in P]
    note = ""
    P, C, T = cluster(P, C, T, pc["cell"])
    if pc.get("islands"):
        P, C, T, gone = drop_islands(P, C, T, pc["islands"])
        if gone: note += " 丢掉零碎小块 %d 个" % gone
    if pc["kind"] == "statue" and pc.get("probe_base"):      # 查脚下那块方底座有多高：离中轴远的点（人身上没有那么远的）最高到哪
        far = sorted(p[1] for p in P if max(abs(p[0]), abs(p[2])) > pc["probe_base"])
        print("    离中轴超过 %.2f m 的点 %d 个，高度 %.3f…%.3f（九成以下 %.3f）" % (pc["probe_base"], len(far), far[0] if far else 0, far[-1] if far else 0, far[int(len(far) * 0.9)] if far else 0))
    if pc["kind"] == "statue" and pc.get("cut_base"):        # 去掉脚下的底座：三个角里至少两个在这个高度以上的面才留（底座顶面连着衣摆的那几片大三角也要丢），人落回到 0
        cut = pc["cut_base"]; keep = []
        for q in range(0, len(T) - 2, 3):
            if (P[T[q]][1] > cut) + (P[T[q + 1]][1] > cut) + (P[T[q + 2]][1] > cut) >= 2: keep.extend((T[q], T[q + 1], T[q + 2]))
        note += " 去掉底座 %d 面" % ((len(T) - len(keep)) // 3)
        used = {}; NP = []; NC = []
        for q, v in enumerate(keep):
            w_ = used.get(v)
            if w_ is None: w_ = used[v] = len(NP); NP.append(P[v]); NC.append(C[v])
            keep[q] = w_
        y1 = 0.0 if pc.get("round_base") else min(p[1] for p in NP)
        P, C, T = [(p[0], p[1] - y1, p[2]) for p in NP], NC, keep
    if pc["kind"] == "statue" and pc.get("smooth"):
        P, C = taubin(P, C, T, pc["smooth"], color=pc.get("color_blur", 0.5))
        y1 = min(p[1] for p in P)
        if not pc.get("center"): P = [(p[0], p[1] - y1, p[2]) for p in P]      # 脚底还是踩在 0 上
    if pc["kind"] == "relief":
        W = w * k; H = h * k
        # 四边那一圈框、背板以后的部分：都压到背板的面上（和墙融在一起，不留一块方板凸着）；背板往后的厚度不要
        # frame：四边各有多宽的一圈整个压平（米；可以给 (两侧, 底, 顶) 三个数——原模型上下各有一道横框）；edge：再往里多宽的一段渐渐放开；
        # bias：背板不是绝对平的（有的地方比“背板面”高出几毫米），整体再往墙里沉这么多，免得墙上留下一片片高出一点点的残块
        edge = pc.get("edge", 0.035) * W; frame = pc.get("frame", 0.0); bias = pc.get("bias", 0.0)
        fs, fb, ft = frame if isinstance(frame, tuple) else (frame * W,) * 3
        Q = []; C = list(C)
        for i, (x, y, z) in enumerate(P):
            # hard_top：顶上那道横框一刀切——框以内整个压平，框以下一点不动（不做“渐渐压平”的那一段）。
            # 伊莉丝的彩虹是一路伸到顶框底下的：渐渐压平的话，彩虹快到顶的那一截被压成薄片、当成背板丢掉，看着就是断的、缺了一块
            # （2026-10-11 用户：伊莉丝的彩虹缺失了一小部分）
            hard = pc.get("hard_top", False)
            m = min(W * 0.5 - abs(x) - fs, y - fb) if hard else min(W * 0.5 - abs(x) - fs, y - fb, H - y - ft)   # 离最近的边多远（方框那一圈以内算 0）
            f = 1.0 if m >= edge else max(0.0, m / edge)
            f = f * f * (3 - 2 * f)
            if hard and H - y < ft: f = 0.0
            z = max(z - bias, 0.0) * f                           # 边上压平；背板后面的收到 0
            g = min(1.0, z / 0.02)                               # 离墙面 2 cm 以内：颜色渐渐换成标准的石头灰（STONE_GREY = 材质里的 LumRef）
            if g < 1.0: C[i] = tuple(STONE_GREY + (C[i][q] - STONE_GREY) * g for q in range(3))
            Q.append((x, y, z))
        if pc.get("smooth"):      # 抹平细褶：贴在墙面上的点（背板）不动，墙面还是平的
            Q, C = taubin(Q, C, T, pc["smooth"], pin=[q[2] <= 1e-6 for q in Q], color=pc.get("color_blur", 0.5))
        if pc.get("zhist"):       # 查背板到底在多深：朝正面的三角形，按离“背板面”多远分档，各档占多少面积
            bins = {}; tot = 0.0
            for q in range(0, len(T) - 2, 3):
                a_, b_, c_ = Q[T[q]], Q[T[q + 1]], Q[T[q + 2]]
                e1 = (b_[0] - a_[0], b_[1] - a_[1], b_[2] - a_[2]); e2 = (c_[0] - a_[0], c_[1] - a_[1], c_[2] - a_[2])
                n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]); l = math.sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2])
                if l < 1e-12 or abs(n[2]) < 0.97 * l: continue
                k_ = int((a_[2] + b_[2] + c_[2]) / 3.0 / 0.002); bins[k_] = bins.get(k_, 0.0) + l; tot += l
            print("    朝正面的面按深度（每档 2 mm）：" + "  ".join("%d–%d mm %.1f%%" % (k_ * 2, k_ * 2 + 2, 100.0 * v / tot) for k_, v in sorted(bins.items()) if v / tot > 0.004))
        if pc.get("drop_back"):   # 背板不要了：整个贴在墙面上的三角形都丢掉——墙自己就是背板，浮雕只剩凸出来的那部分，四周自然没有方框
            keep = []; cut = pc["drop_back"]
            for q in range(0, len(T) - 2, 3):
                if Q[T[q]][2] > cut or Q[T[q + 1]][2] > cut or Q[T[q + 2]][2] > cut: keep.extend((T[q], T[q + 1], T[q + 2]))
            note += " 丢掉背板 %d 面" % ((len(T) - len(keep)) // 3)
            used = {}; NQ = []; NC = []
            for q, v in enumerate(keep):
                w_ = used.get(v)
                if w_ is None: w_ = used[v] = len(NQ); NQ.append(Q[v]); NC.append(C[v])
                keep[q] = w_
            Q, C, T = NQ, NC, keep
            if pc.get("islands_after"):      # 丢了背板以后剩下的零星残片（背板上没沉下去的小块）
                Q, C, T, gone = drop_islands(Q, C, T, pc["islands_after"])
                if gone: note += " 丢掉残片 %d 块" % gone
        if pc.get("depth"):       # 浮雕凸出来的厚度乘一个倍数
            Q = [(q[0], q[1], q[2] * pc["depth"]) for q in Q]
        # 顺着墙弯：横着的距离按弧长算，z 是朝殿心凸出来多少
        Q = [((WALL_R - max(z, 0.0)) * math.sin(x / WALL_R), y, WALL_R - (WALL_R - max(z, 0.0)) * math.cos(x / WALL_R)) for (x, y, z) in Q]
        # 弯完以后：原点在墙面上（浮雕底边的中点），+z 朝殿心；墙面是轴在 (x = 0, z = WALL_R) 的圆柱面，背板就贴在这个面上
        P = Q
        note += " 背板面在原模型深度 %.3f（共 %.3f）" % (back - lo[2], d)
    if pc.get("fix_black", pc["kind"] == "relief"):
        C, nfix = fix_black(P, C, T)
        if nfix: note += " 填掉黑点 %d 个" % nfix
    mir = None
    if pc.get("flat_mirror"):
        P, C, mir = flat_mirror(P, C, T, pc["name"])
    if pc.get("eps"):
        n0 = len(T) // 3; P, C, T = simplify(P, C, T, pc["eps"], accent_sat=pc.get("accent_sat", 0.38), crease=pc.get("crease", 0.0), crease_cos=pc.get("crease_cos", 0.80)); note += " 保平面减面 %d → %d" % (n0, len(T) // 3)
    if mir:
        P, C, T = mirror_plate(P, C, T, mir); note += " 镜子换成规整的圆板（半径 %.3f、厚 %.3f）" % (mir[2] * MIRROR_PLATE_R, 2.0 * MIRROR_PLATE_HALF)
    if pc.get("round_disc"):
        P, C, T, disc = round_disc(P, C, T, pc["name"])
        if disc: note += " 圆盘正面的圆心 (%.3f, %.3f, %.3f) 半径 %.3f" % (disc[0][0], disc[0][1], disc[0][2], disc[2])
    if pc.get("round_base"):      # 规整的圆台座：一级一级的圆柱（半径、从多高到多高），颜色是标准的石头灰
        P = list(P); C = list(C); T = list(T)
        for r_, y0_, y1_ in pc["round_base"]: add_cylinder(P, C, T, (0.0, y0_, 0.0), (0.0, 1.0, 0.0), r_, y1_ - y0_, 48, (STONE_GREY, STONE_GREY, STONE_GREY), caps=(False, True))
        note += " 圆台座 %d 级" % len(pc["round_base"])
    ntri = len(T) // 3; nvert = len(P)
    if pc.get("flat", False): OP_, N, OC_, OT_ = flatten(P, C, T)
    else: OP_, N, OC_, OT_ = P, normals(P, T), C, T
    write_glb(os.path.join(out_dir, pc.get("out", pc["name"]) + ".glb"), OP_, N, OC_, OT_, pc["name"])
    lo2, hi2 = bounds(P)
    avg = [sum(c[i] for c in C) / len(C) for i in range(3)]
    blue = [P[i] for i, c in enumerate(C) if c[2] > 1.25 * c[0] and c[2] > 0.08]
    if len(blue) > 2: note += " 偏蓝的点 %d 个，中心 (%.3f, %.3f, %.3f)，范围 x %.3f…%.3f y %.3f…%.3f z %.3f…%.3f" % (len(blue), sum(p[0] for p in blue) / len(blue), sum(p[1] for p in blue) / len(blue), sum(p[2] for p in blue) / len(blue),
        min(p[0] for p in blue), max(p[0] for p in blue), min(p[1] for p in blue), max(p[1] for p in blue), min(p[2] for p in blue), max(p[2] for p in blue))
    line = "%-14s 顶点 %6d 三角 %6d  倍数 %.4f  x %.3f…%.3f  y %.3f…%.3f  z %.3f…%.3f  平均色 #%02x%02x%02x%s" % (pc["name"], len(P), len(T) // 3, k, lo2[0], hi2[0], lo2[1], hi2[1], lo2[2], hi2[2],
        int(lin_to_srgb(avg[0]) * 255), int(lin_to_srgb(avg[1]) * 255), int(lin_to_srgb(avg[2]) * 255), note)
    print("  " + line); report.append(line)

def main():
    src_dir, out_dir = sys.argv[1], sys.argv[2]; only = sys.argv[3:]
    os.makedirs(out_dir, exist_ok=True)
    report = []
    trial = {}
    for o in list(only):
        if "@" in o:      # 试一档：件名@横竖格子厘米[@进深格子厘米]，另存成 件名_格子.glb
            parts = o.split("@"); only.remove(o); trial.setdefault(parts[0], []).append([float(v) for v in parts[1:]])
    for pc in PIECES:
        for cells in trial.get(pc["name"], []):
            t = dict(pc); c = cells[0] / 100.0
            t["cell"] = (c, c, cells[1] / 100.0) if len(cells) > 1 else c
            t["out"] = "%s_%s" % (pc["name"], "x".join("%g" % v for v in cells))
            convert(t, src_dir, out_dir, [])
        if (only or trial) and pc["name"] not in only: continue
        convert(pc, src_dir, out_dir, report)
    if not only: open(os.path.join(out_dir, "report.txt"), "w", encoding="utf-8").write("\n".join(report) + "\n")

if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--textures":
        # 把各 glb 里的底色贴图原样倒出来（jpg / png），给 make_bmp.ps1 转成 BMP：python -I convert_mech_pack.py --textures <解包目录> <输出目录>
        src_dir, out_dir = sys.argv[2], os.path.join(sys.argv[3], "tex"); os.makedirs(out_dir, exist_ok=True)
        for fn in sorted(os.listdir(src_dir)):
            if not fn.lower().endswith(".glb"): continue
            j, bin_ = load(os.path.join(src_dir, fn)); key = fn[:-4]
            want = set()
            for m in j.get("materials", []):
                t = m.get("pbrMetallicRoughness", {}).get("baseColorTexture")
                if t: want.add(j["textures"][t["index"]]["source"])
            for img in sorted(want):
                im = j["images"][img]; bv = j["bufferViews"][im["bufferView"]]
                ext = "jpg" if im.get("mimeType") == "image/jpeg" else "png"
                open(os.path.join(out_dir, "%s_%d.%s" % (key, img, ext)), "wb").write(bin_[bv.get("byteOffset", 0):bv.get("byteOffset", 0) + bv["byteLength"]])
            print(key, sorted(want))
    else:
        main()
