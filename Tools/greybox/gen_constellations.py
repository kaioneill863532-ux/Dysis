# 结局的两个星座（双子座、天鹅座）：把真实星表上的位置（赤经、赤纬）摊平成一张小图，
# 写成 C++ 里的表（Source/Dysis/Mechanisms/DysisConstellations.generated.h）。
#   · 每个星座各自摊平（以自己的中心为切点），东在左、北在上——就是人抬头看到的样子，不是镜像；
#   · 再转一个角度让它在画面里立着（双子：两个头朝上；天鹅：尾巴上的天津四朝上）；
#   · 缩到“最远的一颗离中心 = 1”，代码里再按光圈的大小放到天上。
# 这两个星座在真的天上隔得很远，结局里是把它们并排摆在殿顶圆眼里的，位置是摆的，形状是真的。
# 用法：python Tools/greybox/gen_constellations.py
import math, os

GEMINI = {
    "stars": {  # 名字: (赤经 时, 赤纬 度, 视星等)
        "Castor": (7.577, 31.89, 1.58), "Pollux": (7.755, 28.03, 1.14), "Alhena": (6.628, 16.40, 1.93), "Wasat": (7.335, 21.98, 3.53),
        "Mebsuta": (6.732, 25.13, 3.06), "Mekbuda": (7.068, 20.57, 3.9), "Propus": (6.248, 22.51, 3.3), "Tejat": (6.383, 22.51, 2.87),
        "Kappa": (7.740, 24.40, 3.57), "Lambda": (7.301, 16.54, 3.58), "Alzirr": (6.755, 12.90, 3.35), "Nu": (6.483, 20.21, 4.15),
        "Tau": (7.186, 30.25, 4.4), "Upsilon": (7.599, 26.90, 4.06), "Iota": (7.429, 27.80, 3.78), "Theta": (6.880, 33.96, 3.6),
    },
    # 连线的顺序就是动画里画出来的顺序：先卡斯托耳这一边，从头画到脚；再波吕丢刻斯这一边；最后两人挽着的手
    "lines": [("Castor", "Tau"), ("Tau", "Mebsuta"), ("Mebsuta", "Tejat"), ("Tejat", "Propus"), ("Mebsuta", "Nu"), ("Tau", "Theta"),
              ("Pollux", "Upsilon"), ("Upsilon", "Wasat"), ("Wasat", "Mekbuda"), ("Mekbuda", "Alhena"), ("Wasat", "Lambda"), ("Lambda", "Alzirr"), ("Pollux", "Kappa"),
              ("Tau", "Iota"), ("Iota", "Upsilon")],
    "up": (("Alhena", "Propus", "Alzirr"), ("Castor", "Pollux")),   # 从脚到头的方向朝上
}
CYGNUS = {
    "stars": {
        "Deneb": (20.690, 45.28, 1.25), "Sadr": (20.370, 40.26, 2.23), "Gienah": (20.770, 33.97, 2.48), "Fawaris": (19.750, 45.13, 2.87),
        "Albireo": (19.512, 27.96, 3.05), "Eta": (19.938, 35.08, 3.89), "Zeta": (21.215, 30.23, 3.21), "Iota": (19.495, 51.73, 3.76), "Kappa": (19.285, 53.37, 3.8),
    },
    # 先画身子（尾 → 颈 → 喙），再画两边的翅膀
    "lines": [("Deneb", "Sadr"), ("Sadr", "Eta"), ("Eta", "Albireo"), ("Sadr", "Gienah"), ("Gienah", "Zeta"), ("Sadr", "Fawaris"), ("Fawaris", "Iota"), ("Iota", "Kappa")],
    "up": (("Albireo",), ("Deneb",)),
}

def flatten(con):
    names = list(con["stars"])
    def vec(ra_h, dec):
        ra, dec = math.radians(ra_h * 15), math.radians(dec)
        return (math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec))
    vs = [vec(*con["stars"][n][:2]) for n in names]
    c = [sum(v[i] for v in vs) for i in range(3)]; L = math.sqrt(sum(x * x for x in c)); c = [x / L for x in c]
    ra0, dec0 = math.atan2(c[1], c[0]), math.asin(c[2])
    east = (-math.sin(ra0), math.cos(ra0), 0.0)
    north = (-math.sin(dec0) * math.cos(ra0), -math.sin(dec0) * math.sin(ra0), math.cos(dec0))
    pts = {}
    for n, v in zip(names, vs):
        d = sum(a * b for a, b in zip(v, c))
        e, nn = sum(a * b for a, b in zip(v, east)) / d, sum(a * b for a, b in zip(v, north)) / d   # 以星座中心为切点摊平
        pts[n] = (-e, nn)                                                                         # 抬头看：东在左
    lo, hi = con["up"]
    def mean(ns): return (sum(pts[n][0] for n in ns) / len(ns), sum(pts[n][1] for n in ns) / len(ns))
    a, b = mean(lo), mean(hi)
    rot = math.pi / 2 - math.atan2(b[1] - a[1], b[0] - a[0])
    cr, sr = math.cos(rot), math.sin(rot)
    pts = {n: (p[0] * cr - p[1] * sr, p[0] * sr + p[1] * cr) for n, p in pts.items()}
    xs, ys = [p[0] for p in pts.values()], [p[1] for p in pts.values()]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    rad = max(math.hypot(p[0] - cx, p[1] - cy) for p in pts.values())
    span_deg = math.degrees(math.atan(rad)) * 2
    out = [(n, (pts[n][0] - cx) / rad, (pts[n][1] - cy) / rad, con["stars"][n][2]) for n in names]
    return out, [(names.index(a), names.index(b)) for a, b in con["lines"]], span_deg

lines = ["// 由 Tools/greybox/gen_constellations.py 生成，不要手改。", "// 结局的两个星座：每颗星在小图上的位置（X 右、Y 上，最远的一颗离中心 = 1）和视星等；连线按画出来的顺序排。",
         "#pragma once", "", '#include "CoreMinimal.h"', "",
         "struct FDysisConStar { float X, Y, Mag; };", "struct FDysisConLine { int32 A, B; };", ""]
for key, title, con in (("Gemini", "双子座", GEMINI), ("Cygnus", "天鹅座", CYGNUS)):
    stars, segs, span = flatten(con)
    lines.append("/** %s：%d 颗星、%d 条线（真的天上横竖大约 %.0f°）。 */" % (title, len(stars), len(segs), span))
    lines.append("inline const FDysisConStar GDysis%sStars[] = {" % key)
    for n, x, y, m in stars: lines.append("\t{ %8.4ff, %8.4ff, %.2ff },   // %s" % (x, y, m, n))
    lines.append("};")
    lines.append("inline const FDysisConLine GDysis%sLines[] = { %s };" % (key, ", ".join("{ %d, %d }" % s for s in segs)))
    lines.append("")
root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
path = os.path.join(root, "Source", "Dysis", "Mechanisms", "DysisConstellations.generated.h")
open(path, "w", encoding="utf-8-sig", newline="").write("\n".join(lines))
print("wrote", path)
print("\n".join(lines))
