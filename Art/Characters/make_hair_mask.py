# 主角头发的遮罩：哪些面是头发、越往发梢越白（材质里按它让头发摆，见 ue_import_hero.py 和 DysisCharacter 的 UpdateHair）。
# 模型是一整块（没有单独的头发、没有头发的骨头），所以从底色贴图上认：头发是灰棕色的（色相 0.03–0.085、饱和度 0.12–0.27），
# 皮肤和裙子更亮，金饰更艳；再按位置收一收（头顶不动、越往下摆得越多、垂在身前的几缕只摆一点，免得穿进身子）。
#
# 用法（普通 Python 就行，不用开 UE；只读模型和贴图，不执行里面的任何东西）：
#   python -I Art/Characters/make_hair_mask.py [预览图放哪个文件夹]
# 结果：Art/Characters/T_DysisHero_HairMask.png（512 × 512 灰度图，排法和美术的贴图一样）。重跑 ue_import_hero.py 导进工程。
# 美术换了模型或贴图以后要重跑一遍；头发的颜色变了的话改下面 is_hair 里的范围。
import struct, zlib, sys, os, math, array, colorsys, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FBX = os.path.join(HERE, "Hero_20261009", "SK_DysisHero.fbx")
BASE = os.path.join(HERE, "Hero_20261009", "T_Character_BaseColor.png")
OUT = os.path.join(HERE, "T_DysisHero_HairMask.png")
SIZE = 512
ROOT_Z, TIP_Z = 150.0, 96.0      # 这个高度以上不动（头顶、发根），到这个高度摆得最多（厘米，脚底是 0）

# ── 读二进制 FBX 里的网格 ──
def read_fbx(path):
    b = open(path, "rb").read()
    assert b[:20] == b"Kaydara FBX Binary  ", "不是二进制 FBX"
    big = struct.unpack_from("<I", b, 23)[0] >= 7500
    def node(off):
        if big: end, nprop, plen = struct.unpack_from("<QQQ", b, off); off += 24
        else: end, nprop, plen = struct.unpack_from("<III", b, off); off += 12
        nlen = b[off]; off += 1
        name = b[off:off + nlen].decode("ascii", "replace"); off += nlen
        if end == 0: return None, off
        props = []; p = off
        for _ in range(nprop):
            t = chr(b[p]); p += 1
            if t in "YCIFDL":
                fmt, sz = {"Y": ("<h", 2), "C": ("<?", 1), "I": ("<i", 4), "F": ("<f", 4), "D": ("<d", 8), "L": ("<q", 8)}[t]
                props.append(struct.unpack_from(fmt, b, p)[0]); p += sz
            elif t in "fdlib":
                n, enc, clen = struct.unpack_from("<III", b, p); p += 12
                raw = b[p:p + clen]; p += clen
                if enc == 1: raw = zlib.decompress(raw)
                a = array.array({"f": "f", "d": "d", "l": "q", "i": "i", "b": "b"}[t]); a.frombytes(raw[:n * a.itemsize]); props.append(a)
            elif t in "SR":
                n = struct.unpack_from("<I", b, p)[0]; p += 4
                props.append(b[p:p + n]); p += n
            else: raise ValueError("没见过的属性类型 " + t)
        off += plen; kids = []
        while off < end:
            k, off = node(off)
            if k is None: break
            kids.append(k)
        return (name, props, kids), end
    off = 27; top = []
    while off < len(b) - 160:
        n, off2 = node(off)
        if n is None: break
        top.append(n); off = off2
    return top
def find(nodes, name):
    for n in nodes:
        if n[0] == name: yield n
        yield from find(n[2], name)

# ── 贴图：用系统自带的画图库缩成 1024 的 BMP 再读（这台机器的 Python 没装图像库） ──
def load_image(path, size=1024):
    tmp = os.path.join(tempfile.mkdtemp(prefix="dysis_hair_"), "base.bmp")
    ps = ("Add-Type -AssemblyName System.Drawing; $i=[System.Drawing.Image]::FromFile($args[0]); $b=New-Object System.Drawing.Bitmap %d,%d,([System.Drawing.Imaging.PixelFormat]::Format24bppRgb);"
          "$g=[System.Drawing.Graphics]::FromImage($b); $g.InterpolationMode='HighQualityBicubic'; $g.DrawImage($i,0,0,%d,%d); $b.Save($args[1],[System.Drawing.Imaging.ImageFormat]::Bmp)") % (size, size, size, size)
    subprocess.run(["powershell", "-NoProfile", "-Command", "& {" + ps + "}", path, tmp], check=True)
    b = open(tmp, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]; W, H = struct.unpack_from("<ii", b, 18); bpp = struct.unpack_from("<H", b, 28)[0] // 8
    row = (W * bpp + 3) & ~3
    def tex(u, v):       # v 朝上；图的第一行在最上面
        x = min(W - 1, max(0, int(u * W))); y = min(H - 1, max(0, int((1.0 - v) * H)))
        o = off + (H - 1 - y) * row + x * bpp
        return b[o + 2], b[o + 1], b[o]
    return tex

def write_png(path, w, h, gray):
    raw = b"".join(b"\x00" + bytes(gray[y * w:(y + 1) * w]) for y in range(h))
    def chunk(tag, data): return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))

def is_hair(rgb):
    h, s, v = colorsys.rgb_to_hsv(rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0)
    return 0.03 <= h <= 0.085 and 0.12 <= s <= 0.27 and 0.38 <= v <= 0.80

def main():
    geo = None
    for g in find(read_fbx(FBX), "Geometry"):
        kids = {k[0]: k for k in g[2]}
        if "Vertices" in kids: geo = kids; break
    V = geo["Vertices"][1][0]; PVI = geo["PolygonVertexIndex"][1][0]
    le = {k[0]: k for k in geo["LayerElementUV"][2]}
    UV = le["UV"][1][0]; UVI = le["UVIndex"][1][0] if "UVIndex" in le else None
    nv = len(V) // 3
    # 文件里的顶点是米、节点上有“绕 Z 转 180°、放大 100”：换成引擎里模型自己的坐标（厘米，脸朝 +Y）
    P = [(-V[3 * i] * 100.0, -V[3 * i + 1] * 100.0, V[3 * i + 2] * 100.0) for i in range(nv)]
    tex = load_image(BASE)

    # 面、每个顶点的颜色（它所有面角取到的颜色的平均）、谁和谁相邻
    polys = []; cur = []; col = [[0, 0, 0, 0] for _ in range(nv)]; nb = [set() for _ in range(nv)]
    for k, idx in enumerate(PVI):
        vi = idx if idx >= 0 else -idx - 1
        ui = UVI[k] if UVI else k
        cur.append((vi, ui))
        r, g, b = tex(UV[2 * ui], UV[2 * ui + 1]); c = col[vi]; c[0] += r; c[1] += g; c[2] += b; c[3] += 1
        if idx < 0:
            for i in range(len(cur)):
                a, d = cur[i][0], cur[(i + 1) % len(cur)][0]; nb[a].add(d); nb[d].add(a)
            polys.append(cur); cur = []
    # 位置完全一样的顶点（贴图接缝两边）当成同一个
    same = {}
    for i, p in enumerate(P): same.setdefault((round(p[0], 2), round(p[1], 2), round(p[2], 2)), []).append(i)
    for grp in same.values():
        if len(grp) > 1:
            allnb = set().union(*(nb[i] for i in grp))
            for i in grp: nb[i] = allnb

    hair = [1.0 if c[3] and is_hair((c[0] / c[3], c[1] / c[3], c[2] / c[3])) else 0.0 for c in col]
    # 零星认错的抹掉：看邻居里多数是什么（两遍）
    for _ in range(2):
        hair = [(1.0 if (sum(hair[j] for j in nb[i]) + hair[i]) / (len(nb[i]) + 1) > 0.5 else 0.0) if nb[i] else hair[i] for i in range(nv)]
    # 摆多少：发根不动，越往下越多；垂在身前的只摆三成
    wgt = []
    for i in range(nv):
        x, y, z = P[i]
        t = min(1.0, max(0.0, (ROOT_Z - z) / (ROOT_Z - TIP_Z)))
        k = hair[i] * t ** 1.3
        if y > 6.0: k *= 0.3 + 0.7 * max(0.0, 1.0 - (y - 6.0) / 6.0)
        wgt.append(k)
    # 边缘抹软一点（三遍）
    for _ in range(3):
        wgt = [(0.5 * wgt[i] + 0.5 * sum(wgt[j] for j in nb[i]) / len(nb[i])) if nb[i] else wgt[i] for i in range(nv)]
    n_hair = sum(1 for k in wgt if k > 0.05)
    print("顶点 %d，认成头发的 %d，会摆的（> 5%%）%d" % (nv, int(sum(hair)), n_hair))

    # 画到贴图的排法里：每个面按三个角的数插值；然后往外扩几圈（接缝外面一点也有值，取样不会取到黑边）
    S = SIZE; img = [-1.0] * (S * S)
    for poly in polys:
        for t in range(1, len(poly) - 1):
            tri = (poly[0], poly[t], poly[t + 1])
            pts = [(UV[2 * ui] * S, (1.0 - UV[2 * ui + 1]) * S, wgt[vi]) for vi, ui in tri]
            x0 = max(0, int(min(p[0] for p in pts)) - 1); x1 = min(S - 1, int(max(p[0] for p in pts)) + 1)
            y0 = max(0, int(min(p[1] for p in pts)) - 1); y1 = min(S - 1, int(max(p[1] for p in pts)) + 1)
            (ax, ay, aw), (bx, by, bw), (cx, cy, cw) = pts
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9: continue
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    px, py = x + 0.5, y + 0.5
                    l0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / den
                    l1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / den
                    l2 = 1.0 - l0 - l1
                    if l0 >= -0.02 and l1 >= -0.02 and l2 >= -0.02:
                        img[y * S + x] = max(img[y * S + x], min(1.0, max(0.0, l0 * aw + l1 * bw + l2 * cw)))
    for _ in range(4):
        nxt = img[:]
        for y in range(S):
            for x in range(S):
                if img[y * S + x] >= 0.0: continue
                best = -1.0
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < S and 0 <= yy < S: best = max(best, img[yy * S + xx])
                nxt[y * S + x] = best
        img = nxt
    write_png(OUT, S, S, [int(round(max(0.0, v) * 255)) for v in img])
    print("写好了", OUT)

    # 预览：三张点图（侧面、背面、正面），会摆的地方画成红的
    if len(sys.argv) > 1:
        def bmp(path, w, h, pix):
            row = (w * 3 + 3) & ~3; data = bytearray(row * h)
            for (x, y), (r, g, b) in pix.items():
                if 0 <= x < w and 0 <= y < h:
                    o = (h - 1 - y) * row + x * 3; data[o:o + 3] = bytes((int(b), int(g), int(r)))
            open(path, "wb").write(b"BM" + struct.pack("<IHHI", 54 + len(data), 0, 0, 54) + struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0) + bytes(data))
        C = [(c[0] / max(c[3], 1), c[1] / max(c[3], 1), c[2] / max(c[3], 1)) for c in col]
        def view(name, key, depth):
            pix = {}; zb = {}
            for i in range(nv):
                u, v = key(P[i]); d = depth(P[i]); cx, cy = int(u * 4.0), int(700 - v * 4.0)
                k = wgt[i]; c = (C[i][0] * (1 - k) * 0.6 + 255 * k, C[i][1] * (1 - k) * 0.6 + 40 * k, C[i][2] * (1 - k) * 0.6 + 40 * k)
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        q = (cx + dx, cy + dy)
                        if q not in zb or d > zb[q]: zb[q] = d; pix[q] = c
            bmp(os.path.join(sys.argv[1], name), 560, 720, pix)
        view("hair_side.bmp", lambda p: (p[1] + 70, p[2]), lambda p: p[0])
        view("hair_back.bmp", lambda p: (-p[0] + 70, p[2]), lambda p: -p[1])
        view("hair_front.bmp", lambda p: (p[0] + 70, p[2]), lambda p: p[1])
        print("预览图在", sys.argv[1])

if __name__ == "__main__":
    main()
