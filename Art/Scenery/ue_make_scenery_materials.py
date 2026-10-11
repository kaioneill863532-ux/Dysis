# 外景“夕照”要用的材质（UE 编辑器 Python，可以重复跑），顺便把拷进来的美术资产按我们的引擎版本重新存一遍：
#   /Game/Dysis/Sky/M_DysisSunsetSky   白天到黄昏的天：罩在最外面的大球朝里的一面。朝着太阳的那半边是桃粉到橙金，背着太阳的半边是蓝到粉紫，
#                                      贴着地平线颜色最浓；太阳周围一圈亮晕，中间一个日盘。颜色取自场景美术 2026-10-09 那版外景的天（M_MATCH_Sky），
#                                      但暖的那一边跟着真的太阳走（他那版是朝一个固定的方向）。
#                                      这个材质勾了“是天空”（Is Sky）：引擎的天光会把它拍进去，所以背光的面、阴影里的颜色也跟着它变成蓝紫。
#                                      参数 SunDir、Gain（亮度，场景里的真实单位，代码按太阳光的强弱给）、Zenith / HorizonCool / WarmHigh / WarmLow（四个颜色，代码按太阳高低给）、
#                                      Glow（太阳周围亮晕的颜色）、Disc（日盘的颜色）、DiscDeg（日盘的半径，度）、Dim（整片天压暗多少，日盘和贴着它的一小圈亮晕不压；主界面用）。
#   /Game/Dysis/Sky/M_DysisCloud       低模的云：不受光的一块颜色（Color，代码按时刻给，已经乘好亮度）。
#   /Game/Dysis/ReferenceMatch_20261009 美术的山、石、云的模型和调色板材质：重新保存；两种石头的碰撞设成“直接用模型本身”（台地上的石头要挡人）。
# 用法：python Tools/ue_run.py Art/Scenery/ue_make_scenery_materials.py
import unreal
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

def new_mat(path, name, two_sided):
    full = path + "/" + name
    mat = unreal.load_asset(full) if eal.does_asset_exist(full) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", two_sided)
    # 引擎的 delete_all_material_expressions 一次只删掉一半，删到一个不剩为止（不然留下来的旧参数节点会顶掉新写的默认值）
    for _ in range(24):
        if mel.get_num_material_expressions(mat) == 0: break
        mel.delete_all_material_expressions(mat)
    return mat, full
def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): e.set_editor_property(k, v)
    return e
def scalar(mat, name, dv, x, y): return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=dv)
def vector(mat, name, rgb, x, y):
    v = node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    m = node(mat, unreal.MaterialExpressionComponentMask, x + 250, y, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(v, "", m, ""); return m
def custom(mat, code, inputs, x=-500, y=0):
    c = node(mat, unreal.MaterialExpressionCustom, x, y, code=code, output_type=F3)
    ins = []
    for n, _ in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    for n, e in inputs: mel.connect_material_expressions(e, "", c, n)
    return c
def finish(mat, full):
    mel.recompile_material(mat)
    print(full.split("/")[-1], "saved:", eal.save_asset(full, only_if_is_dirty=False))

# ───────── 白天到黄昏的天 ─────────
mat, full = new_mat("/Game/Dysis/Sky", "M_DysisSunsetSky", True)
mat.set_editor_property("is_sky", True)
cv = node(mat, unreal.MaterialExpressionCameraVectorWS, -1500, -200)
sun = vector(mat, "SunDir", (-0.8, -0.35, 0.5), -1750, -80)
cols = [vector(mat, n, v, -1750, 100 + 170 * i) for i, (n, v) in enumerate([
    ("Zenith", (0.24, 0.36, 0.80)), ("HorizonCool", (0.50, 0.40, 0.62)), ("WarmHigh", (0.95, 0.50, 0.32)), ("WarmLow", (1.90, 0.80, 0.20)),
    ("Glow", (1.60, 0.80, 0.30)), ("Disc", (3.00, 2.40, 1.00))])]
gain, disc, dim = scalar(mat, "Gain", 1.3, -1500, 1200), scalar(mat, "DiscDeg", 1.0, -1500, 1300), scalar(mat, "Dim", 1.0, -1500, 1400)
c = custom(mat, """
float3 v = -normalize(CV);                                   // 这个像素在天上的方向
float3 s = normalize(Sun);
float hz = length(v.xy);
float azd = dot(v.xy / max(hz, 1.0e-4), normalize(s.xy + float2(1.0e-4, 0.0)));   // 水平方向上朝着太阳（+1）还是背着（−1）
float low = 1.0 - smoothstep(0.0, 0.50, v.z);                // 贴着地平线 1，往上三十度左右到 0
low *= low;
float side = smoothstep(-0.55, 0.80, azd) * saturate(hz * 1.6);   // 太阳那半边；头顶附近不分哪边
float3 cool = lerp(Zenith, HorizonCool, low);
float3 warm = lerp(WarmHigh, WarmLow, low);
float3 c = lerp(cool, warm, saturate(side * lerp(0.42, 1.0, low))) * Dim;
// 太阳周围：一大圈淡的、一小圈亮的，中间是日盘。太阳落到地平线下就都没了
float sunUp = smoothstep(-0.035, 0.015, s.z);
float mu = saturate(dot(v, s));
c += Glow * (((pow(mu, 3.0) * 0.10 + pow(mu, 8.0) * 0.28) * Dim + pow(mu, 64.0) * 0.55) * sunUp);   // 大的那两圈跟着整片天一起压暗，贴着日盘的一小圈不压
float d = smoothstep(cos(radians(DiscDeg * 1.18)), cos(radians(DiscDeg)), dot(v, s)) * sunUp;
c = lerp(c, Disc, d);
c *= lerp(0.78, 1.0, smoothstep(-0.10, 0.0, v.z));           // 地平线以下（平时被海挡着）稍暗一点
return c * Gain;
""", [("CV", cv), ("Sun", sun)] + list(zip(["Zenith", "HorizonCool", "WarmHigh", "WarmLow", "Glow", "Disc"], cols)) + [("Gain", gain), ("DiscDeg", disc), ("Dim", dim)])
mel.connect_material_property(c, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(mat, full)

# ───────── 云 ─────────
mat, full = new_mat("/Game/Dysis/Sky", "M_DysisCloud", False)
mat.set_editor_property("is_sky", False)
mel.connect_material_property(vector(mat, "Color", (0.8, 0.6, 0.65), -600, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(mat, full)
# 同样的一份，标成“天”的一部分（is_sky）：每朵云里面另套一份用它（DysisDirectorScenery.cpp）。天光的那张环境图里只有“算作天”的东西，
# 镜湖的海面映不到画面以外的东西时会退回去查这张图——没有这一份的话云一出画面，水里的倒影就没了（2026-10-11 用户）。
# 云本体不能直接标成“天”：那样的模型不记远近，入夜以后会被夜空那层天幕整个盖住。
mat, full = new_mat("/Game/Dysis/Sky", "M_DysisCloudSky", False)
mat.set_editor_property("is_sky", True)
mel.connect_material_property(vector(mat, "Color", (0.8, 0.6, 0.65), -600, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
finish(mat, full)

# ───────── 美术的山、石、云和调色板：按我们的引擎版本重新保存 ─────────
n = 0
for sub in ("Meshes", "Materials"):
    for p in eal.list_assets("/Game/Dysis/ReferenceMatch_20261009/" + sub, recursive=False):
        path = p.split(".")[0]
        asset = unreal.load_asset(path)
        if not asset: continue
        # 两种石头要能挡人（台地上的那几块）：模型只有几十个面，直接拿它本身当碰撞
        if isinstance(asset, unreal.StaticMesh) and asset.get_name() in ("SM_MATCH_RockA", "SM_MATCH_RockB") and asset.get_editor_property("body_setup"):
            asset.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        if eal.save_asset(path, only_if_is_dirty=False): n += 1
print("ReferenceMatch_20261009 重新保存了 %d 个资产" % n)
