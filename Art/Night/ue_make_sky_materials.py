# 建夜空、星星、月虹、光圈叶片要用的材质（UE 编辑器 Python，可以重复跑）：
#   /Game/Dysis/Sky/M_DysisNightDome    夜空：罩在外面的大球朝里的一面。地平线附近深蓝、头顶近乎黑，月亮周围一圈淡光（颜色取自灰盒 nightDome、moonHalo）。
#                                       落日以后先是灰蒙蒙的暮色（Twilight），再换成夜空。
#                                       参数 Opacity、Glow（亮度换算）、NightW / TwilightW（夜空、暮色各占多少）、Twilight（暮色的颜色）、Halo（月亮圆盘显出来多少）、MoonUp、MoonDir。
#   /Game/Dysis/Sky/M_DysisStar         星星：一张小片，中间亮、边上淡；每颗星自己的颜色和亮度从“每个实例的数据”里来。
#                                       参数 Opacity、Glow、Line（1 = 画成一条细线，结局星座的连线用）。
#   /Game/Dysis/Night/M_DysisMoonbow    月虹：立在瀑布前面的一片透明片上画一道弧，白里带一点七色（外红内紫）。
#                                       参数 W、H（片子的宽高，厘米）、Radius、Band（弧的半径、半宽）、Opacity、Glow。
#   /Game/Dysis/Night/M_DysisIrisBlade  光圈叶片：照着叶片现在用的材质复制一份，再加一条“离殿心不到 Aperture 厘米的地方不画”。
# 前三个都是不受光、双面、不吃雾。现在只是占位的样子，等美术的。
import unreal
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

def new_mat(path, name, blend):
    full = path + "/" + name
    mat = unreal.load_asset(full) if eal.does_asset_exist(full) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    for prop, val in (("use_translucency_vertex_fog", False), ("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)):
        try: mat.set_editor_property(prop, val)
        except Exception as e: unreal.log_warning("%s: %s 设不上（%s）" % (name, prop, e))
    mel.delete_all_material_expressions(mat)
    return mat, full

def scalar(mat, name, dv, x, y):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name); e.set_editor_property("default_value", dv)
    return e

def custom(mat, code, inputs, out_type, x=-700, y=0):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", out_type)
    ins = []
    for n, _, _ in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    for n, e, out in inputs: mel.connect_material_expressions(e, out, c, n)
    return c

def channel(mat, src, comps, x, y):
    m = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, x, y)
    for k in "rgba": m.set_editor_property(k, k in comps)
    mel.connect_material_expressions(src, "", m, "")
    return m

def local_pos(mat):
    # 模型自己坐标里的位置（世界坐标是大坐标，自定义节点里不好直接用）
    wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1500, 0)
    lp = mel.create_material_expression(mat, unreal.MaterialExpressionTransformPosition, -1250, 0)
    lp.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    lp.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(wp, "", lp, "")
    return lp

def finish(mat, full):
    mel.recompile_material(mat)
    print(full.split("/")[-1], "saved:", eal.save_asset(full, only_if_is_dirty=False))

F3, F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT3, unreal.CustomMaterialOutputType.CMOT_FLOAT4

# ───────── 夜空 ─────────
mat, full = new_mat("/Game/Dysis/Sky", "M_DysisNightDome", unreal.BlendMode.BLEND_TRANSLUCENT)
cv = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1250, 0)
moon = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1250, 160)
moon.set_editor_property("parameter_name", "MoonDir"); moon.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
moon3 = channel(mat, moon, "rgb", -1000, 160)
halo, moonup = scalar(mat, "Halo", 0.0, -1250, 400), scalar(mat, "MoonUp", 1.0, -1250, 500)
glow, opac = scalar(mat, "Glow", 3.0, -1250, 600), scalar(mat, "Opacity", 0.0, -1250, 700)
nightw, twiw = scalar(mat, "NightW", 1.0, -1250, 800), scalar(mat, "TwilightW", 0.0, -1250, 900)
twi = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1250, 1000)
twi.set_editor_property("parameter_name", "Twilight"); twi.set_editor_property("default_value", unreal.LinearColor(0.055, 0.049, 0.043, 0.0))
twi3 = channel(mat, twi, "rgb", -1000, 1000)
c = custom(mat, """
float3 d = -normalize(CV);
float3 m = normalize(Moon);
float3 c = lerp(float3(0.03, 0.045, 0.085), float3(0.006, 0.01, 0.026), smoothstep(-0.02, 0.55, d.z));
float md = clamp(dot(d, m), 0.0, 1.0);
c += float3(0.05, 0.06, 0.085) * pow(md, 10.0) * MoonUp;
float h = saturate(1.0 - degrees(acos(md)) / 3.72);
c += float3(0.55, 0.62, 0.8) * (h * h * h * 0.5 * Halo);
float3 t = Twi * lerp(1.15, 0.7, smoothstep(0.0, 0.6, d.z));
return c * NightW + t * TwilightW;
""", [("CV", cv, ""), ("Moon", moon3, ""), ("Halo", halo, ""), ("MoonUp", moonup, ""), ("NightW", nightw, ""), ("TwilightW", twiw, ""), ("Twi", twi3, "")], F3)
em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 0)
mel.connect_material_expressions(c, "", em, "A"); mel.connect_material_expressions(glow, "", em, "B")
mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(opac, "", unreal.MaterialProperty.MP_OPACITY)
finish(mat, full)

# ───────── 星星 ─────────
mat, full = new_mat("/Game/Dysis/Sky", "M_DysisStar", unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("used_with_instanced_static_meshes", True)
uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1250, -160)
cv = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -1250, 0)
data = mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -1500, 160)
data.set_editor_property("data_index", 0); data.set_editor_property("const_default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
vi = mel.create_material_expression(mat, unreal.MaterialExpressionVertexInterpolator, -1250, 160)   # 每颗星的数据只在顶点上有，带到像素里去
mel.connect_material_expressions(data, "", vi, "")
line, glow, opac = scalar(mat, "Line", 0.0, -1250, 400), scalar(mat, "Glow", 3.0, -1250, 500), scalar(mat, "Opacity", 1.0, -1250, 600)
c = custom(mat, """
float2 q = UV * 2.0 - 1.0;
float dotA = saturate((1.0 - length(q)) * 2.2); dotA = dotA * dotA * (3.0 - 2.0 * dotA);   // 中间一小块是满亮的，边上淡出去（太小的亮点在画面上会被抹淡）
float lineA = saturate(1.0 - abs(q.y)); lineA *= lineA * saturate((1.0 - abs(q.x)) * 14.0);
float a = lerp(dotA, lineA, Line);
a *= saturate(-CV.z * 30.0);
return Data * (a * Glow * Opacity);
""", [("UV", uv, ""), ("CV", cv, ""), ("Data", vi, ""), ("Line", line, ""), ("Glow", glow, ""), ("Opacity", opac, "")], F3)
mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(mat, full)

# ───────── 月虹 ─────────
BOW7 = [0xff3a2a, 0xff8a1c, 0xffe03a, 0x46d86a, 0x3aa8ff, 0x4a4cff, 0xa24aff]
def lin(c):   # sRGB → 线性
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
COLS = ", ".join("float3(%.4f, %.4f, %.4f)" % (lin(h >> 16 & 255), lin(h >> 8 & 255), lin(h & 255)) for h in BOW7)
mat, full = new_mat("/Game/Dysis/Night", "M_DysisMoonbow", unreal.BlendMode.BLEND_ADDITIVE)
lp = local_pos(mat)
w, h = scalar(mat, "W", 1400.0, -1250, 200), scalar(mat, "H", 760.0, -1250, 300)
rad, band = scalar(mat, "Radius", 640.0, -1250, 400), scalar(mat, "Band", 42.0, -1250, 500)
glow, opac = scalar(mat, "Glow", 0.3, -1250, 600), scalar(mat, "Opacity", 0.0, -1250, 700)
c = custom(mat, """
float2 p = float2(LP.x * W / 100.0, H * 0.5 - LP.y * H / 100.0);
float t = (length(p) - (Radius - Band)) / (2.0 * Band);
float a = pow(saturate(t) * saturate(1.0 - t) * 4.0, 0.8);
a *= smoothstep(0.0, 170.0, p.y);
float3 cols[7] = { %s };
float xs = clamp((1.0 - t) * 7.0 - 0.5, 0.0, 6.0);
int k0 = (int)floor(xs); int k1 = min(k0 + 1, 6);
float3 sp = lerp(cols[k0], cols[k1], xs - (float)k0);
float3 c = lerp(float3(0.80, 0.87, 1.0), sp, 0.30);
return c * (a * Glow * Opacity);
""" % COLS, [("LP", lp, ""), ("W", w, ""), ("H", h, ""), ("Radius", rad, ""), ("Band", band, ""), ("Glow", glow, ""), ("Opacity", opac, "")], F3)
mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(mat, full)

# ───────── 光圈叶片 ─────────
def blade_base_material():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor):
        if any(str(t).startswith("SM_Mech_IrisBlades_") for t in a.tags):
            m = a.static_mesh_component.get_material(0)
            if m: return m, m.get_base_material()
    return None, None
orig, base = blade_base_material()
if not base:
    unreal.log_error("M_DysisIrisBlade：关卡里找不到光圈叶片（Tag 以 SM_Mech_IrisBlades_ 开头），没建")
else:
    full = "/Game/Dysis/Night/M_DysisIrisBlade"
    if eal.does_asset_exist(full): eal.delete_asset(full)
    mat = eal.duplicate_asset(base.get_path_name().split(".")[0], full)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("opacity_mask_clip_value", 0.3333)
    lp = local_pos(mat)   # 叶片的轴心就在殿心：自己坐标里离原点多远 = 离殿心多远
    ap = scalar(mat, "Aperture", 40.0, -1250, 200)
    c = custom(mat, "return saturate(length(LP.xy) - Aperture);", [("LP", lp, ""), ("Aperture", ap, "")], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -900, 100)
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat, full)
    print("M_DysisIrisBlade 照着", base.get_path_name(), "做的；叶片原来用的是", orig.get_path_name())
