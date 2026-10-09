# 把场景美术 2026-10-09 的外景摆放清单（ReferenceMatch_20261009/final_scene_configuration.json）
# 变成代码里用的一张表：Source/Dysis/Mechanisms/DysisSceneryData.generated.h。
# 只取山、石、草坡、云这 62 件配景；清单里的相机、灯、天空球、地形壳不要（原因见 DysisDirectorScenery.cpp 开头）。
# 用法：python Art/Scenery/gen_scenery_data.py
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "ReferenceMatch_20261009", "final_scene_configuration.json")
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "Source", "Dysis", "Mechanisms", "DysisSceneryData.generated.h"))

KINDS = [  # 清单里名字的开头 → 哪一类（长的在前）
    ("MATCH_EdgeGrassBank", "EdgeGrassBank"), ("MATCH_GrassBank", "GrassBank"), ("MATCH_FarMountain", "FarMountain"), ("MATCH_MidMountain", "MidMountain"),
    ("MATCH_RightMonolith", "Monolith"), ("MATCH_ForeRock", "ForeRock"), ("MATCH_NearRock", "NearRock"), ("MATCH_Cloud", "Cloud"),
]
MESHES = ["RockA", "RockB", "Mountain", "Cloud"]

def short(path): return path.split(".")[-1]

items = []
for p in json.load(open(SRC, encoding="utf-8"))["actor_placements"]:
    kind = next((k for prefix, k in KINDS if p["label"].startswith(prefix)), None)
    if kind is None: continue
    mesh = short(p["mesh"]).replace("SM_MATCH_", "")
    assert mesh in MESHES, p["label"]
    roll, pitch, yaw = p["rotation_rotator_roll_pitch_yaw"]
    assert abs(roll) < 1e-6 and abs(pitch) < 1e-6, p["label"]          # 都只绕竖轴转过
    mats = [short(m) for m in p["materials"]]
    if kind == "Cloud":
        assert mats == ["MI_MATCH_CloudScattering_%d" % i for i in range(3)], p["label"]
        palette = "Cloud"
    else:
        palette = mats[0].replace("MI_MATCH_", "")
        assert mats == ["MI_MATCH_" + palette, "MI_MATCH_%s_1" % palette, "MI_MATCH_%s_2" % palette], p["label"]   # 三个面组各一档深浅
    items.append((p["label"], kind, mesh, p["location_cm"], yaw, p["scale"], palette))

L = ["// 由 Art/Scenery/gen_scenery_data.py 从场景美术 2026-10-09 的外景摆放清单生成，不要手改。单位：厘米（UE 坐标）。",
     "#pragma once", "",
     "enum class EDysisSceneryKind : uint8 { %s };" % ", ".join(k for _, k in reversed(KINDS)),
     "enum class EDysisSceneryMesh : uint8 { %s };" % ", ".join(MESHES), "",
     "/** 一件配景：美术摆的位置、绕竖轴转的角度、缩放；Palette = 用哪一种颜色（材质 MI_MATCH_<Palette>、_1、_2 三档深浅；云另算）。 */",
     "struct FDysisSceneryItem { const TCHAR* Label; EDysisSceneryKind Kind; EDysisSceneryMesh Mesh; FVector Loc; float Yaw; FVector Scale; const TCHAR* Palette; };", "",
     "inline const FDysisSceneryItem GDysisScenery[] = {"]
for label, kind, mesh, loc, yaw, scale, palette in items:
    L.append('\t{ TEXT("%s"), EDysisSceneryKind::%s, EDysisSceneryMesh::%s, FVector(%.1f, %.1f, %.1f), %.1ff, FVector(%.2f, %.2f, %.2f), TEXT("%s") },'
             % (label, kind, mesh, loc[0], loc[1], loc[2], yaw, scale[0], scale[1], scale[2], palette))
L.append("};")
open(OUT, "w", encoding="utf-8-sig", newline="\n").write("\n".join(L) + "\n")   # 带 BOM：里面有中文注释，编译器要靠它认编码
print("写了 %d 件配景 → %s" % (len(items), OUT))
