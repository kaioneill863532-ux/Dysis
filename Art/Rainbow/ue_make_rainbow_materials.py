# 建“虹”要用的两个材质（UE 编辑器 Python，可以重复跑）：
#   /Game/Dysis/Rainbow/M_DysisRainbowBridge  虹桥：横着分成七条色带；参数 Reveal（0–1，从浮雕那头长出来多少）、Opacity、Glow。
#   /Game/Dysis/Rainbow/M_DysisBowBands       浮雕上的那一圈七色：贴在浮雕前面的一块透明片，只在“从对的站位看过去 40°–43°”的那一圈上有颜色；参数 Opacity、Glow。
# 都是不受光、半透明、双面。颜色和位置的数取自灰盒（Tools/greybox/golden/greybox_rainbow.json）。
# 现在只是占位的样子，等美术的虹。
import unreal, json
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_rainbow.json", encoding="utf-8"))["consts"]
PATH = "/Game/Dysis/Rainbow"
mel = unreal.MaterialEditingLibrary
BOW7 = [0xff3a2a, 0xff8a1c, 0xffe03a, 0x46d86a, 0x3aa8ff, 0x4a4cff, 0xa24aff]
def lin(c):   # sRGB → 线性
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
COLS = ", ".join("float3(%.4f, %.4f, %.4f)" % (lin(h >> 16 & 255), lin(h >> 8 & 255), lin(h & 255)) for h in BOW7)
def v3(v): return "float3(%.4f, %.4f, %.4f)" % (v[0], v[1], v[2])

def build(name, code, params):
    full = PATH + "/" + name
    mat = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    mel.delete_all_material_expressions(mat)
    # 模型自己坐标里的位置（世界坐标是大坐标，自定义节点里不好直接用）
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, 0)
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -1050, 0)
    lp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(wp, "", lp, "")
    cust = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -750, 0)
    cust.set_editor_property("code", code)
    cust.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    ins = []
    for n in ["LP"] + [p for p, _ in params if p not in ("Opacity", "Glow")]:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    cust.set_editor_property("inputs", ins)
    mel.connect_material_expressions(lp, "", cust, "LP")
    sc = {}
    for i, (p, dv) in enumerate(params):
        e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1050, 200 + 110 * i)
        e.set_editor_property("parameter_name", p); e.set_editor_property("default_value", dv); sc[p] = e
        if p not in ("Opacity", "Glow"): mel.connect_material_expressions(e, "", cust, p)
    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -450, -60)
    for k, on in (("r", True), ("g", True), ("b", True), ("a", False)): rgb.set_editor_property(k, on)
    al = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -450, 120)
    for k, on in (("r", False), ("g", False), ("b", False), ("a", True)): al.set_editor_property(k, on)
    mel.connect_material_expressions(cust, "", rgb, ""); mel.connect_material_expressions(cust, "", al, "")
    em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, -60)
    mel.connect_material_expressions(rgb, "", em, "A"); mel.connect_material_expressions(sc["Glow"], "", em, "B")
    op = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 120)
    mel.connect_material_expressions(al, "", op, "A"); mel.connect_material_expressions(sc["Opacity"], "", op, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    print(name, "saved:", unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False))

# 虹桥：模型的原点就在桥头（BRIDGE.A），在关卡里转了 90°，所以“世界里的偏移” = (−y, x, z)
B = G["BRIDGE"]
build("M_DysisRainbowBridge", """
float3 d = float3(-LP.y, LP.x, LP.z);
float t = dot(d, %s) / %.3f;
float l = dot(d, %s) / %.1f + 0.5;
float3 cols[7] = { %s };
int k = clamp((int)floor(l * 7.0), 0, 6);
return float4(cols[k], t <= Reveal ? 1.0 : 0.0);
""" % (v3(B["ax"]), B["len"], v3(B["side"]), B["W"], COLS), [("Reveal", 1.0), ("Opacity", 0.7), ("Glow", 1.0)])

# 浮雕上的七色圈：透明片是引擎的 Plane（100×100），代码里把它摆在浮雕面前 8.5 cm、放大成 2.2 m × 2.7 m：
#   横着（x）沿墙的方向，竖着（y）朝下。算出每一点在世界里的位置，再看它和“对的站位上的眼睛 → 背着太阳的方向”夹多少度。
I = G["IRISREL"]; R = G["RELIEF"]
C = [I["face"][i] + I["n"][i] * 8.5 for i in range(3)]; C[2] += 5.0 + R["h"] / 2
build("M_DysisBowBands", """
float3 wp = %s + %s * (LP.x * %.4f) + float3(0.0, 0.0, -1.0) * (LP.y * %.4f);
float3 d = normalize(wp - %s);
float ang = degrees(acos(clamp(dot(d, %s), -1.0, 1.0)));
float x = (43.25 - ang) / 0.5;
float3 cols[7] = { %s };
int k = clamp((int)floor(x), 0, 6);
float inb = (x >= 0.0 && x < 7.0 && wp.z > %.1f) ? 1.0 : 0.0;
return float4(cols[k], inb);
""" % (v3(C), v3(I["T"]), R["w"] / 100.0, R["h"] / 100.0, v3(I["E"]), v3([-x for x in I["sun"]]), COLS, I["face"][2] + 3.0), [("Opacity", 0.0), ("Glow", 1.0)])
