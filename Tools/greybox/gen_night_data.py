# 由灰盒导出的 golden/greybox_night2.json 生成夜里第二段要用的数据表（C++ 头文件）：月桥中线的 73 个点、
# 厚墙和月之龛两块月石的取样点。重新导出以后再跑一次：
#   python Tools/greybox/gen_night_data.py
import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
c = json.load(open(os.path.join(HERE, "golden", "greybox_night2.json"), encoding="utf-8"))["consts"]
def vec(p): return "FVector(%.3f, %.3f, %.3f)" % (p[0], p[1], p[2])
def table(name, pts, per=3):
    rows = ["inline const FVector %s[] = {" % name]
    for i in range(0, len(pts), per): rows.append("	" + ", ".join(vec(p) for p in pts[i:i + per]) + ",")
    return rows + ["};", ""]
out = ["// 由 Tools/greybox/gen_night_data.py 从灰盒 v0.12 导出的数据生成，不要手改。单位：厘米（UE 坐标）。", "#pragma once", "",
       "/** 月桥中线上的点（灰盒 MOONBR.pts）：从 L1 西边双子脚下，绕过水亭北边，落到水庭东边。桥宽 %.0f cm。 */" % c["bridge"]["w"]]
out += table("GDysisMoonBridgePts", c["bridge"]["pts"])
out += ["/** L1 东南厚墙（月石 twinWall）的九个取样点。 */"] + table("GDysisTwinWallSamples", c["twinWall"]["samples"])
out += ["/** 月之龛（月石 moonShrine）的九个取样点。 */"] + table("GDysisMoonShrineSamples", c["MSHRINE"]["samples"])
dst = os.path.normpath(os.path.join(HERE, "..", "..", "Source", "Dysis", "Mechanisms", "DysisNightData.generated.h"))
open(dst, "wb").write(b"\xef\xbb\xbf" + "\n".join(out).encode("utf-8"))
print("wrote", dst, len(c["bridge"]["pts"]), "bridge points")
