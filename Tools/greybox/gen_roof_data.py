# 由灰盒导出的 golden/greybox_roof.json 生成屋顶踏步的数据表（C++ 头文件）。重新导出以后再跑一次：
#   python Tools/greybox/gen_roof_data.py
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
g = json.load(open(os.path.join(HERE, "golden", "greybox_roof.json"), encoding="utf-8"))
KIND = {"land": 0, "up": 1, "dn": 2, "fix": 3}
def f(x): return "-1.0f" if x is None else ("%.5f" % x).rstrip("0").rstrip(".") + ("f" if "." in ("%.5f" % x).rstrip("0").rstrip(".") else ".0f")
out = ["// 由 Tools/greybox/gen_roof_data.py 从灰盒 v0.12 的 PIECES 生成，不要手改。单位：度 / 厘米；−1 = 没有这一项。",
       "#pragma once", "",
       "/** 屋顶环道的一块（灰盒 crownPiece）。Kind：0 细桥落脚的那块（不动）、1 白天升起来的踏步、2 夜里降下去的楼梯、3 不动的。 */",
       "struct FDysisRoofPieceSpec",
       "{",
       "	int32 Kind, K;",
       "	float A0, A1;          // 这一块占的方位（度）",
       "	float ZEnd;            // 升到头时比环道高多少（Kind 1）",
       "	float ZNight;          // 夜里降到的高度（Kind 2）",
       "	float Inner, Outer;    // 内沿、外沿半径（Outer −1 = 环道外沿 1545）",
       "	float Gap0, Gap1;      // 内侧的墙在这一段方位让开（细桥从这里上来）",
       "	float EndAz, EndR0, EndR1;   // 这一块逆时针那头的挡头",
       "};",
       "",
       "inline const FDysisRoofPieceSpec GDysisRoofPieces[] = {"]
for p in g["pieces"]:
    gap = p["gap"] or [None, None]; e = p["endWall"] or {}
    out.append("	{ %d, %2d, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s }," % (
        KIND[p["kind"]], p["k"], f(p["a0"]), f(p["a1"]), f(p["zEnd"]), f(p["zNight"]), f(p["inner"]), f(p["outer"]),
        f(gap[0]), f(gap[1]), f(e.get("az")), f(e.get("r0")), f(e.get("r1"))))
out += ["};", ""]
dst = os.path.normpath(os.path.join(HERE, "..", "..", "Source", "Dysis", "Mechanisms", "DysisRoofData.generated.h"))
open(dst, "wb").write(b"\xef\xbb\xbf" + "\n".join(out).encode("utf-8"))
print("wrote", dst, len(g["pieces"]), "pieces")
