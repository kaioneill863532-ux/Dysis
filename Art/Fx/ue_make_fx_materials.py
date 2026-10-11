# 建“光和水”这一批效果的材质（UE 编辑器 Python，可以重复跑；想只建其中几个：把名字写在 项目/Saved/fx_only.txt，逗号隔开）。
# 方向：往《光遇》那种柔和、会发光、空气里有光粒的感觉做。不用任何外来素材：形状和噪声都是材质里现算的。
#
#   /Game/Dysis/Beams/M_DysisBeam        光柱（体积光）：不是一块发光的板，而是“视线穿过光柱多长就多亮”——
#                                         侧着看薄、顺着看浓，边上是软的，里面有慢慢飘的雾气；人走进去也不会被罩成一片白。
#                                         参数 Color、Intensity（代码写）、Brightness、FullDepth（穿过多厚算全亮，厘米）、Soft（边上软多宽）、
#                                         StartSoft / EndSoft（两头软多长）、NoiseAmt（雾气的起伏）。
#   /Game/Dysis/Fx/M_DysisMote           光粒：一组小片，位置、飘动、明灭全在材质里算（不用粒子系统）。挂在谁身上就充满谁的盒子。
#                                         参数 Color、Glow、Size（厘米）、Flow（沿盒子三个方向飘多快，每秒飘过盒子的几分之几）、Wobble（左右晃多少）、Opacity。
#
#   /Game/Dysis/Rainbow/M_DysisSprayBow 水沫里的虹（浮雕前的水帘、殿顶虹门的喷泉）：背对太阳 42° 才看得见，人挪一步虹跟着挪。
#   /Game/Dysis/Fx/M_DysisGlowPool       光做的桥落在石头上的那一汪光。
#   其余（虹桥、浮雕上的七色圈、月虹、影桥、月桥、瀑布、海、水庭的水）各段开头有说明。
#
# 用法：python Tools/ue_run.py Art/Fx/ue_make_fx_materials.py
import unreal, os
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
ONLY = [x.strip() for x in open(SAVED + "fx_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "fx_only.txt") else []
def want(name): return not ONLY or name in ONLY

F1, F3, F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3, unreal.CustomMaterialOutputType.CMOT_FLOAT4
POS = unreal.MaterialPositionTransformSource
VEC_SRC, VEC_DST = unreal.MaterialVectorCoordTransformSource, unreal.MaterialVectorCoordTransform

def new_mat(path, name, blend, unlit=True, two_sided=True, **props):
    full = path + "/" + name
    mat = unreal.load_asset(full) if eal.does_asset_exist(full) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", blend)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT if unlit else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property("two_sided", two_sided)
    props.setdefault("use_translucency_vertex_fog", False)
    for prop, val in props.items():
        try: mat.set_editor_property(prop, val)
        except Exception as e: unreal.log_warning("%s: %s 设不上（%s）" % (name, prop, e))
    # 引擎的 delete_all_material_expressions 一次只删掉一半（它边遍历边删），留下来的旧参数节点会顶掉新写的默认值——删到一个不剩为止
    for _ in range(24):
        if mel.get_num_material_expressions(mat) == 0: break
        mel.delete_all_material_expressions(mat)
    if mel.get_num_material_expressions(mat) != 0: unreal.log_warning("%s: 旧节点没删干净（还剩 %d 个）" % (name, mel.get_num_material_expressions(mat)))
    return mat, full

def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): e.set_editor_property(k, v)
    return e
def scalar(mat, name, dv, x, y): return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=dv)
def vector(mat, name, rgb, x, y): return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
def rgb_of(mat, src, x, y):
    m = node(mat, unreal.MaterialExpressionComponentMask, x, y, r=True, g=True, b=True, a=False)
    mel.connect_material_expressions(src, "", m, ""); return m
def custom(mat, code, inputs, out_type, x=-600, y=0):
    c = node(mat, unreal.MaterialExpressionCustom, x, y, code=code, output_type=out_type)
    ins = []
    for n, _ in inputs:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    for n, e in inputs: mel.connect_material_expressions(e, "", c, n)
    return c
def local_of(mat, src, x, y):
    # 世界里的一个位置 → 模型自己坐标里的位置（世界坐标是大坐标，自定义节点里不好直接用）
    t = node(mat, unreal.MaterialExpressionTransformPosition, x, y, transform_source_type=POS.TRANSFORMPOSSOURCE_WORLD, transform_type=POS.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(src, "", t, ""); return t
def axis_world(mat, v, x, y):
    # 模型自己的一根坐标轴在世界里是哪个方向、多长（长度 = 这个方向上的缩放）
    c = node(mat, unreal.MaterialExpressionConstant3Vector, x - 220, y, constant=unreal.LinearColor(v[0], v[1], v[2], 0.0))
    t = node(mat, unreal.MaterialExpressionTransform, x, y, transform_source_type=VEC_SRC.TRANSFORMSOURCE_LOCAL, transform_type=VEC_DST.TRANSFORM_WORLD)
    mel.connect_material_expressions(c, "", t, ""); return t
def emissive(mat, src):
    # 发光的东西按“屏幕上看着多亮”来写：接到发光口之前先除掉当时的曝光倍数。
    # （白天殿里是自动曝光，会把画面提亮几十倍；夜里是另一个固定的倍数。不除的话同一个数白天亮成一片白、夜里又不够。
    #   除了以后：写 1 就是屏幕上的白，写 0.2 就是淡淡的一层，白天夜里一个样。）
    ea = node(mat, unreal.MaterialExpressionEyeAdaptation, -220, 260)
    d = node(mat, unreal.MaterialExpressionDivide, -80, 200)
    mel.connect_material_expressions(src, "", d, "A"); mel.connect_material_expressions(ea, "", d, "B")
    mel.connect_material_property(d, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
def finish(mat, full):
    mel.recompile_material(mat)
    print(full.split("/")[-1], "saved:", eal.save_asset(full, only_if_is_dirty=False))

# 材质里现算的噪声（自定义节点里不能直接写函数，包在一个结构体里）：h = 把一个格点变成 0–1 的随机数，n = 平滑的三维噪声 0–1，
# fbm = 灰盒那个两层的雾，sp = 一格一颗、位置和明灭都各不相同的圆光点（uv 的一格里最多一颗）
NOISE = """
struct FDysisNoise {
  float h(float3 p) { p = frac(p * 0.3183099 + 0.1); p *= 17.0; return frac(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float n(float3 x) {
    float3 i = floor(x), f = frac(x); f = f * f * (3.0 - 2.0 * f);
    return lerp(lerp(lerp(h(i), h(i + float3(1, 0, 0)), f.x), lerp(h(i + float3(0, 1, 0)), h(i + float3(1, 1, 0)), f.x), f.y),
                lerp(lerp(h(i + float3(0, 0, 1)), h(i + float3(1, 0, 1)), f.x), lerp(h(i + float3(0, 1, 1)), h(i + float3(1, 1, 1)), f.x), f.y), f.z);
  }
  float fbm(float3 p) { return 0.62 * n(p) + 0.38 * n(p * 2.17 + 7.3); }
  float sp(float2 uv, float t) {
    float2 i = floor(uv), f = frac(uv);
    float a = h(float3(i, 3.7)), b = h(float3(i, 11.1)), c = h(float3(i, 23.9));
    float d = length(f - (0.25 + 0.5 * float2(a, b)));
    float tw = pow(0.5 + 0.5 * sin(t * (0.7 + 1.9 * c) + a * 43.0), 5.0);
    return smoothstep(0.09 + 0.13 * c, 0.0, d) * tw * step(0.4, b);
  }
};
FDysisNoise NZ;
"""

# 灰盒的虹（合成那一遍里的 spectral / bow）：视线离“光往前走的方向”多少度就是什么颜色——
# 40.6°–42.3° 是主虹（里紫外红），50.3°–53.4° 是淡淡的副虹（颜色反过来），主虹里面再亮一点点
BOW = """
struct FDysisBow {
  float3 spectral(float x) {
    float3 c = float3(smoothstep(0.42, 0.78, x) + 0.4 * smoothstep(0.0, 0.1, x) * (1.0 - smoothstep(0.1, 0.28, x)),
                      smoothstep(0.18, 0.45, x) * (1.0 - smoothstep(0.62, 0.88, x)),
                      1.0 - smoothstep(0.22, 0.5, x));
    return c * smoothstep(0.0, 0.1, x) * (1.0 - smoothstep(0.9, 1.0, x));
  }
  float3 bow(float ang) {
    float x1 = (ang - 40.6) / 1.7, x2 = (ang - 50.3) / 3.1;
    float3 c = spectral(saturate(x1)) * step(0.0, x1) * step(x1, 1.0);
    c += 0.3 * spectral(1.0 - saturate(x2)) * step(0.0, x2) * step(x2, 1.0);
    c += 0.06 * smoothstep(28.0, 40.6, ang) * step(ang, 40.6);
    return c;
  }
  float3 bowW(float ang, float wd) {                                   // 主虹放宽 wd 倍（中心还在 41.45°），别的不变
    float lo = 41.45 - 0.85 * wd, x1 = (ang - lo) / (1.7 * wd), x2 = (ang - 50.3) / 3.1;
    float3 c = spectral(saturate(x1)) * step(0.0, x1) * step(x1, 1.0);
    c += 0.3 * spectral(1.0 - saturate(x2)) * step(0.0, x2) * step(x2, 1.0);
    c += 0.06 * smoothstep(28.0, lo, ang) * step(ang, lo);
    return c;
  }
};
FDysisBow BW;
"""

# 视线穿过一个盒子（模型自己的坐标里是 ±50）的哪一段：只在盒子的背面上画一次（正面那一遍不画），这样镜头在盒子外面、里面都对；
# 关了深度测试，被前面的东西挡住的那一段用场景深度截掉。给出 O、D（镜头、这一个像素的方向）、tEnter、tExit、dir（世界里的视线方向）、jit（每个像素错开一点取样）
RAYBOX = """
float3 O = CL, D = LP - CL;
float3 inv = 1.0 / (abs(D) + 1e-4) * (step(0.0, D) * 2.0 - 1.0);
float3 t0 = (-50.0 - O) * inv, t1 = (50.0 - O) * inv;
float3 tmin = min(t0, t1), tmax = max(t0, t1);
float tEnter = max(max(tmin.x, tmin.y), max(tmin.z, 0.0));
float tExit = min(min(tmax.x, tmax.y), min(tmax.z, SD / max(PD, 1.0)));
if (Face > 0.0 || tExit <= tEnter) return 0.0;
float lx = length(AX), ly = length(AY), lz = length(AZ);
float3 dirW = AX * D.x + AY * D.y + AZ * D.z;
float rayLen = length(dirW);
float3 dir = dirW / max(rayLen, 1e-4);
float segLen = rayLen * (tExit - tEnter);                                          // 视线穿过盒子多少厘米
float2 px = Parameters.SvPosition.xy + 5.588238 * float(ResolvedView.StateFrameIndexMod8);
float jit = frac(52.9829189 * frac(dot(px, float2(0.06711056, 0.00583715))));
"""
def raybox_inputs(mat):
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1700, -200)
    cam = node(mat, unreal.MaterialExpressionCameraPositionWS, -1700, 0)
    lp, cl = local_of(mat, wp, -1450, -200), local_of(mat, cam, -1450, 0)
    ax, ay, az = axis_world(mat, (1, 0, 0), -1450, 200), axis_world(mat, (0, 1, 0), -1450, 340), axis_world(mat, (0, 0, 1), -1450, 480)
    sd = node(mat, unreal.MaterialExpressionSceneDepth, -1450, 620)
    pd = node(mat, unreal.MaterialExpressionPixelDepth, -1450, 720)
    face = node(mat, unreal.MaterialExpressionTwoSidedSign, -1450, 820)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 920)
    return [("LP", lp), ("CL", cl), ("AX", ax), ("AY", ay), ("AZ", az), ("SD", sd), ("PD", pd), ("Face", face), ("Time", time)]
def named(P): return [(p.get_editor_property("parameter_name"), p) for p in P]

# ───────────────────────── 光柱（体积光） ─────────────────────────
if want("M_DysisBeam"):
    # 亮度跟着看的方向变（灰盒的做法）：迎着光看过去亮一些，背着光看淡一些——所以不再是一根一样白的棒子。
    # 里面的雾气有两层：慢慢飘的大块浓淡 + 顺着光拉长的一丝一丝。
    # 日光穿过瀑布的水沫（瀑布前面、脚下，还有顺着水面飘到第一束光光脚的那一片）：背对太阳 42° 的方向上立着一段虹（SprayK > 0 才算；月光没有）。
    mat, full = new_mat("/Game/Dysis/Beams", "M_DysisBeam", unreal.BlendMode.BLEND_ADDITIVE, disable_depth_test=True)
    ins = raybox_inputs(mat)
    col = rgb_of(mat, vector(mat, "Color", (1.0, 0.84, 0.58), -1700, 1040), -1450, 1040)
    cen = rgb_of(mat, vector(mat, "Center", (0.0, 0.0, 0.0), -1700, 1200), -1450, 1200)
    P = [scalar(mat, n, v, -1450, 1360 + 100 * i) for i, (n, v) in enumerate([("Intensity", 1.0), ("Brightness", 3.0), ("FullDepth", 380.0), ("Soft", 55.0), ("StartSoft", 25.0), ("EndSoft", 90.0), ("NoiseAmt", 0.6), ("Phase", 1.0), ("SprayK", 0.0), ("RainK", 2.4), ("Round", 0.0)])]
    c = custom(mat, NOISE + BOW + RAYBOX + """
float3 L = AX / max(lx, 1e-4);                                                     // 光往前走的方向
float soft = min(Soft, 22.5 * min(ly, lz));                                        // 边上软多宽（不超过半宽的一小半）
float sum = 0.0, spr = 0.0;
for (int i = 0; i < 8; i++) {
  float3 p = O + D * lerp(tEnter, tExit, (float(i) + jit) / 8.0);
  float edge = smoothstep(0.0, soft, min((50.0 - abs(p.y)) * ly, (50.0 - abs(p.z)) * lz));
  float ends = smoothstep(0.0, StartSoft, (p.x + 50.0) * lx) * smoothstep(0.0, EndSoft, (50.0 - p.x) * lx);
  float3 w = AX * p.x + AY * p.y + AZ * p.z;                                       // 相对光柱中心的世界位置（厘米）
  float n = NZ.n(w * 0.0045 + float3(0.0, 0.0, -Time * 0.035)) * 0.55 + NZ.n(float3(p.x * lx * 0.0011 - Time * 0.03, p.y * ly * 0.021, p.z * lz * 0.021)) * 0.45;
  float shape = lerp(edge * ends, pow(saturate(1.0 - dot(p, p) / 2500.0), 1.6), Round);   // Round = 1：一团（中间浓、四周化开），不是一根柱子
  sum += shape * lerp(1.0, n * 1.7, NoiseAmt);
  if (SprayK > 0.001) {
    float3 Pw = Center + w;
    float r = length(Pw.xy) * 0.01, y = Pw.z * 0.01;                               // 离殿心多远、多高（米）
    float a = degrees(atan2(Pw.y, Pw.x)); a += a < 0.0 ? 360.0 : 0.0;
    float inA = smoothstep(114.75, 117.25, a) * (1.0 - smoothstep(138.75, 141.25, a));
    float front = (1.0 - smoothstep(0.0, 1.4, 12.45 - r)) * step(r, 12.85);
    float foot = (1.0 - smoothstep(0.0, 3.2, 12.45 - r)) * (1.0 - smoothstep(0.5, 4.5, y)) * step(-0.8, y);
    float dA = smoothstep(86.25, 96.25, a) * (1.0 - smoothstep(115.25, 118.25, a));
    float drift = dA * smoothstep(8.0, 9.5, r) * (1.0 - smoothstep(12.0, 13.0, r)) * (1.0 - smoothstep(1.5, 4.0, y)) * step(-0.6, y);
    spr += (inA * (0.55 * front + 1.2 * foot) + 0.7 * drift) * (0.6 + 0.8 * n) * shape;
  }
}
float thick = segLen * sum / 8.0;
float g = 0.4, ct = dot(L, -dir);
float hg = (1.0 - g * g) / (12.566371 * pow(1.0 + g * g - 2.0 * g * ct, 1.5));
float ph = lerp(1.0, 0.45 + 3.0 * hg, Phase * 0.6);                                // 背着光 0.71 … 侧着 0.77 … 迎着光 1.23（灰盒是 0.52 … 1.38，UE 里光柱是路，差别收一些）
float3 outc = Color * ((1.0 - exp(-thick / max(FullDepth, 1.0))) * Intensity * Brightness * ph);
if (SprayK > 0.001) outc += BW.bow(degrees(acos(clamp(dot(dir, L), -1.0, 1.0)))) * (spr * segLen * 0.01 / 8.0 * RainK * SprayK);
return outc;
""", ins + [("Color", col), ("Center", cen)] + named(P), F3, -900, 0)
    emissive(mat, c)
    finish(mat, full)

# ───────────────────────── 光粒 ─────────────────────────
if want("M_DysisMote"):
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisMote", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1700, -300)
    data = node(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -1700, -150, data_index=0, const_default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    time = node(mat, unreal.MaterialExpressionTime, -1700, 0)
    ax, ay, az = axis_world(mat, (1, 0, 0), -1450, 150), axis_world(mat, (0, 1, 0), -1450, 290), axis_world(mat, (0, 0, 1), -1450, 430)
    # 镜头的右、上两个方向（小片永远正对镜头）
    def view_axis(v, y):
        c = node(mat, unreal.MaterialExpressionConstant3Vector, -1700, y, constant=unreal.LinearColor(v[0], v[1], v[2], 0.0))
        t = node(mat, unreal.MaterialExpressionTransform, -1450, y, transform_source_type=VEC_SRC.TRANSFORMSOURCE_VIEW, transform_type=VEC_DST.TRANSFORM_WORLD)
        mel.connect_material_expressions(c, "", t, ""); return t
    cr, cu = view_axis((1, 0, 0), 570), view_axis((0, 1, 0), 710)
    flow = rgb_of(mat, vector(mat, "Flow", (0.02, 0.0, 0.0), -1700, 860), -1450, 860)
    size, wob = scalar(mat, "Size", 5.0, -1450, 1020), scalar(mat, "Wobble", 0.03, -1450, 1120)
    # 顶点往哪挪：这一粒在盒子里的位置（随机起点 + 慢慢飘 + 轻轻晃） − 这个顶点原来的位置 + 正对镜头的一小片
    lpv = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 1240), -1450, 1240)
    wpo = custom(mat, """
float3 r = Data;                                                   // 这一粒自己的三个随机数
float3 q = frac(r + Flow * Time + Wobble * float3(sin(Time * (0.31 + r.y) + r.z * 40.0), sin(Time * (0.23 + r.z) + r.x * 40.0), sin(Time * (0.27 + r.x) + r.y * 40.0))) - 0.5;
float3 want = q * 100.0;                                           // 盒子是 ±50
float3 d = want - LP;                                              // 这个顶点原来在哪（模型自己的坐标）→ 挪到这一粒的位置
float s = Size * (0.55 + 0.9 * frac(r.x * 7.31 + r.y * 3.17));     // 每一粒大小不一样
return AX * d.x + AY * d.y + AZ * d.z + (CR * (UV.x - 0.5) + CU * (0.5 - UV.y)) * s;
""", [("Data", data), ("Flow", flow), ("Time", time), ("Wobble", wob), ("LP", lpv), ("AX", ax), ("AY", ay), ("AZ", az), ("CR", cr), ("CU", cu), ("UV", uv), ("Size", size)], F3, -900, 300)
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    vi = node(mat, unreal.MaterialExpressionVertexInterpolator, -1450, -150)
    mel.connect_material_expressions(data, "", vi, "")
    col = rgb_of(mat, vector(mat, "Color", (1.0, 0.9, 0.7), -1700, -520), -1450, -520)
    glow, opac = scalar(mat, "Glow", 2.0, -1450, -420), scalar(mat, "Opacity", 1.0, -1450, -330)
    em = custom(mat, """
float2 q = UV * 2.0 - 1.0;
float a = saturate(1.0 - length(q)); a = a * a * a;                // 中间亮、边上很快淡掉的一个小光点
float tw = 0.55 + 0.45 * sin(Time * (0.9 + 1.7 * Data.z) + Data.x * 60.0);   // 慢慢明灭
float3 r = Data;
float3 bq = frac(r + Flow * Time + Wobble * float3(sin(Time * (0.31 + r.y) + r.z * 40.0), sin(Time * (0.23 + r.z) + r.x * 40.0), sin(Time * (0.27 + r.x) + r.y * 40.0))) - 0.5;   // 这一粒在盒子里的位置
float3 f3 = lerp(float3(1.0, 1.0, 1.0), smoothstep(0.0, 0.12, 0.5 - abs(bq)), saturate(abs(Flow) * 1000.0));
float fade = f3.x * f3.y * f3.z;                                   // 往哪个方向飘，就在那个方向的两头淡出去，不会“啪”地跳回来
return Color * (a * tw * fade * Glow * Opacity);
""", [("UV", uv), ("Data", vi), ("Time", time), ("Flow", flow), ("Wobble", wob), ("Color", col), ("Glow", glow), ("Opacity", opac)], F3, -900, -300)
    emissive(mat, em)
    finish(mat, full)

# ───────────────────────── 虹：虹桥、浮雕上的那一圈 ─────────────────────────
import json
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
G = json.load(open(PROJ + "Tools/greybox/golden/greybox_rainbow.json", encoding="utf-8"))["consts"]
BOW7 = [0xff3a2a, 0xff8a1c, 0xffe03a, 0x46d86a, 0x3aa8ff, 0x4a4cff, 0xa24aff]
def lin(c):   # sRGB → 线性
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
COLS = ", ".join("float3(%.4f, %.4f, %.4f)" % (lin(h >> 16 & 255), lin(h >> 8 & 255), lin(h & 255)) for h in BOW7)
def v3(v): return "float3(%.4f, %.4f, %.4f)" % (v[0], v[1], v[2])
# 七色之间平滑过渡：x = 0–7 → 颜色 sp
SPECTRUM = """
float3 cols[7] = { %s };
float xs = clamp(x - 0.5, 0.0, 6.0);
int k0 = (int)floor(xs); int k1 = min(k0 + 1, 6);
float3 sp = lerp(cols[k0], cols[k1], smoothstep(0.0, 1.0, xs - (float)k0));
""" % COLS

def lit_alpha(mat, c, glow, opac):
    # 自定义节点给出 (颜色, 透明度) → 接到发光和不透明度上
    rgb = rgb_of(mat, c, -350, -60)
    al = node(mat, unreal.MaterialExpressionComponentMask, -350, 120, r=False, g=False, b=False, a=True)
    mel.connect_material_expressions(c, "", al, "")
    em = node(mat, unreal.MaterialExpressionMultiply, -150, -60); op = node(mat, unreal.MaterialExpressionMultiply, -150, 120)
    mel.connect_material_expressions(rgb, "", em, "A"); mel.connect_material_expressions(glow, "", em, "B")
    mel.connect_material_expressions(al, "", op, "A"); mel.connect_material_expressions(opac, "", op, "B")
    emissive(mat, em)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)

if want("M_DysisRainbowBridge"):
    # 虹桥：一条会发光的软带子——七色横着平滑过渡，两边是软的，顺着桥有光在流，上面有一颗一颗明灭的光点。
    # 挨着石头的地方（桥头落在走廊地面上、桥尾伸进窗台里）自己化开：不留一道硬的交线，也不会在离石头还有一截的地方就没了。
    # 模型的原点就在桥头（BRIDGE.A），在关卡里转了 90°，所以“世界里的偏移” = (−y, x, z)。参数 Reveal（长出来多少）、Opacity、Glow。
    B = G["BRIDGE"]
    mat, full = new_mat("/Game/Dysis/Rainbow", "M_DysisRainbowBridge", unreal.BlendMode.BLEND_TRANSLUCENT)
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 150)
    sd, pd = node(mat, unreal.MaterialExpressionSceneDepth, -1450, 560), node(mat, unreal.MaterialExpressionPixelDepth, -1450, 660)
    reveal, glow, opac = scalar(mat, "Reveal", 1.0, -1450, 260), scalar(mat, "Glow", 1.0, -1450, 360), scalar(mat, "Opacity", 0.7, -1450, 460)
    c = custom(mat, NOISE + """
float3 d = float3(-LP.y, LP.x, LP.z);
float t = dot(d, %s) / %.3f;                                 // 沿桥 0–1
float l = dot(d, %s) / %.1f + 0.5;                           // 横着 0–1
float x = l * 7.0;
%s
float edge = smoothstep(0.0, 0.13, l) * smoothstep(1.0, 0.87, l);            // 两边软
float ends = smoothstep(0.0, 0.010, t) * smoothstep(1.0, 0.988, t);          // 桥头桥尾那一道边
float touch = saturate((SD - PD) / 24.0);                                    // 挨着石头的地方化开
float grow = smoothstep(Reveal + 0.001, Reveal - 0.07, t);                    // 正在长出来的那一头也是软的
float flow = 0.5 + 0.5 * sin(t * 70.0 - Time * 1.6 + l * 3.0);                // 顺着桥流的光
float2 suv = float2(t * %.3f, l * %.1f);                                      // 桥面上的位置（厘米）
float spark = NZ.sp(suv / 9.0, Time * 2.2) * 1.5 + NZ.sp(suv / 21.0 + 5.3, Time * 1.3) * 0.9;
float3 col = lerp(sp, float3(1.0, 1.0, 1.0), 0.03 + 0.10 * flow) * (0.74 + 0.22 * flow) + spark * (0.55 + 0.45 * sp);
float core = 0.82 + 0.18 * (1.0 - abs(l * 2.0 - 1.0));                        // 中间略实、两边略透
return float4(col, edge * ends * touch * grow * core);
""" % (v3(B["ax"]), B["len"], v3(B["side"]), B["W"], SPECTRUM, B["len"], B["W"]), [("LP", lp), ("Time", time), ("Reveal", reveal), ("SD", sd), ("PD", pd)], F4, -900, 0)
    lit_alpha(mat, c, glow, opac)
    finish(mat, full)

if want("M_DysisBowBands"):
    # 浮雕上的七色圈：贴在浮雕前面的一块透明片（代码里摆在浮雕面前 8.5 cm、放大成 2.2 m × 2.7 m：横着沿墙、竖着朝下），
    # 只在“从对的站位看过去 40°–43°”的那一圈上有颜色；外面再罩一圈淡淡的光晕，对上了以后亮得清清楚楚。参数 Opacity、Glow。
    I = G["IRISREL"]; R = G["RELIEF"]
    Cc = [I["face"][i] + I["n"][i] * 8.5 for i in range(3)]; Cc[2] += 5.0 + R["h"] / 2
    mat, full = new_mat("/Game/Dysis/Rainbow", "M_DysisBowBands", unreal.BlendMode.BLEND_TRANSLUCENT)
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 150)
    glow, opac = scalar(mat, "Glow", 1.3, -1450, 360), scalar(mat, "Opacity", 0.0, -1450, 460)
    c = custom(mat, """
float3 wp = %s + %s * (LP.x * %.4f) + float3(0.0, 0.0, -1.0) * (LP.y * %.4f);
float3 dir = normalize(wp - %s);
float ang = degrees(acos(clamp(dot(dir, %s), -1.0, 1.0)));
float x = (43.25 - ang) / 0.5;                               // 0–7 = 七色（外红内紫）
%s
float band = smoothstep(-0.2, 0.9, x) * smoothstep(7.2, 6.1, x);
float halo = exp(-pow((x - 3.5) / 5.5, 2.0)) * 0.22;                           // 圈外淡淡的一层光
float shimmer = 0.9 + 0.1 * sin(Time * 1.3 + ang * 9.0);
float inb = (wp.z > %.1f) ? 1.0 : 0.0;
float a = saturate(band * 0.95 + halo) * inb;
return float4(lerp(sp, float3(1.0, 1.0, 1.0), 0.12 + halo) * shimmer, a);
""" % (v3(Cc), v3(I["T"]), R["w"] / 100.0, R["h"] / 100.0, v3(I["E"]), v3([-q for q in I["sun"]]), SPECTRUM, I["face"][2] + 3.0), [("LP", lp), ("Time", time)], F4, -900, 0)
    lit_alpha(mat, c, glow, opac)
    finish(mat, full)

# ───────────────────────── 水沫里的虹：浮雕前的水帘、殿顶虹门的喷泉 ─────────────────────────
if want("M_DysisSprayBow"):
    # 一个装着水沫的盒子（看不见盒子本身）：阳光照着它，从背对太阳 42° 的方向看过去，水沫里就立着一段虹——
    # 人挪一步虹就跟着挪，和真的一样（灰盒合成那一遍里的 uGate / uRel）。盒子：x 横着、y 是厚度、z 朝上。
    # 参数 LightDir（光往前走的方向，代码写）、SunColor、Gain（虹多亮）、SunGain（水沫本身被照亮多少）、
    # EdgeLo / EdgeHi（盒子三个方向上两头各软掉几成）、NoiseScale / NoiseSpeed（水沫的起伏、往上还是往下流）、ShadowZ（这个高度以下照不到太阳）、Center（盒子中心，代码写）。
    # Anchor = 1：虹落在哪按“人的眼睛”算，不按镜头算（灰盒是按镜头算的——镜头贴着人的时候两样一样；UE 里镜头在人身后几米，
    #   按镜头算的话，人站对了位置虹也落不到刻着的那一圈上、填不满拱门）。做法：视线和盒子里的一个面（y = BowPlane）相交的那一点，
    #   从 Eye（人的头，代码写）看过去离“背对太阳的方向”多少度，就是什么颜色。Anchor = 0 就是灰盒原样。
    # 画法：半透明地盖上去（亮墙前面要是“加上去”只会更白，看不出颜色）；Widen：主虹放宽几倍（镜头离得远，按真实的 1.7° 太细）。
    mat, full = new_mat("/Game/Dysis/Rainbow", "M_DysisSprayBow", unreal.BlendMode.BLEND_TRANSLUCENT, disable_depth_test=True)
    ins = raybox_inputs(mat)
    ldir = rgb_of(mat, vector(mat, "LightDir", (-0.75, -0.48, -0.45), -1700, 1040), -1450, 1040)
    sun = rgb_of(mat, vector(mat, "SunColor", (1.0, 0.93, 0.82), -1700, 1200), -1450, 1200)
    elo = rgb_of(mat, vector(mat, "EdgeLo", (0.12, 0.3, 0.08), -1700, 1360), -1450, 1360)
    ehi = rgb_of(mat, vector(mat, "EdgeHi", (0.12, 0.3, 0.2), -1700, 1520), -1450, 1520)
    cen = rgb_of(mat, vector(mat, "Center", (0.0, 0.0, 0.0), -1700, 1680), -1450, 1680)
    eye = rgb_of(mat, vector(mat, "Eye", (0.0, 0.0, 0.0), -1700, 1840), -1450, 1840)
    P = [scalar(mat, n, v, -1450, 2000 + 100 * i) for i, (n, v) in enumerate([("Gain", 0.0), ("SunGain", 0.0), ("NoiseScale", 1.3), ("NoiseSpeed", -0.6), ("ShadowZ", -1.0e6), ("Anchor", 1.0), ("BowPlane", 0.0), ("Widen", 2.0)])]
    c = custom(mat, NOISE + BOW + RAYBOX + """
float spr = 0.0;
for (int i = 0; i < 12; i++) {
  float3 p = O + D * lerp(tEnter, tExit, (float(i) + jit) / 12.0);
  float3 q = p * 0.01 + 0.5;                                                       // 在盒子里的位置 0–1
  float3 e = smoothstep(float3(0.0, 0.0, 0.0), max(EdgeLo, 1e-4), q) * smoothstep(float3(0.0, 0.0, 0.0), max(EdgeHi, 1e-4), 1.0 - q);
  float3 w = AX * p.x + AY * p.y + AZ * p.z;
  float n = NZ.fbm(w * 0.01 * NoiseScale + float3(0.0, 0.0, Time * NoiseSpeed));
  spr += (0.35 + 0.9 * n) * e.x * e.y * e.z * step(ShadowZ, Center.z + w.z);
}
spr *= segLen * 0.01 / 12.0;                                                       // 灰盒里是按米算的
float3 bdir = dir;
if (Anchor > 0.5) {
  float3 pl = O + D * clamp((BowPlane - O.y) / (abs(D.y) > 1e-5 ? D.y : 1e-5), tEnter, tExit);
  bdir = normalize(Center + AX * pl.x + AY * pl.y + AZ * pl.z - Eye);
}
float ang = degrees(acos(clamp(dot(bdir, normalize(LightDir)), -1.0, 1.0)));
float3 bc = BW.bowW(ang, max(Widen, 1.0)) * (Gain * spr);
return float4(bc + SunColor * (SunGain * spr), saturate(max(bc.r, max(bc.g, bc.b)) * 1.1));
""", ins + [("LightDir", ldir), ("SunColor", sun), ("EdgeLo", elo), ("EdgeHi", ehi), ("Center", cen), ("Eye", eye)] + named(P), F4, -900, 0)
    lit_alpha(mat, c, node(mat, unreal.MaterialExpressionConstant, -1450, 2800, r=1.0), node(mat, unreal.MaterialExpressionConstant, -1450, 2900, r=1.0))
    finish(mat, full)

if want("M_DysisGlowPool"):
    # 光做的桥落在石头上的地方：地上一汪淡淡的光（一块平放的片，中间亮、往外化开，慢慢地一明一暗）。参数 Color、Opacity。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisGlowPool", unreal.BlendMode.BLEND_ADDITIVE)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1700, -300)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 150)
    col = rgb_of(mat, vector(mat, "Color", (1.0, 0.95, 0.85), -1700, 260), -1450, 260)
    opac = scalar(mat, "Opacity", 0.0, -1450, 420)
    c = custom(mat, NOISE + """
float2 q = UV * 2.0 - 1.0;
float r = length(q);
float a = pow(saturate(1.0 - r), 2.2);
float breathe = 0.82 + 0.18 * sin(Time * 1.1);
float wisp = 0.75 + 0.5 * NZ.n(float3(q * 2.6, Time * 0.2));
return Color * (a * breathe * wisp * Opacity);
""", [("UV", uv), ("Time", time), ("Color", col), ("Opacity", opac)], F3, -900, 0)
    emissive(mat, c)
    finish(mat, full)

# ───────────────────────── 月虹 ─────────────────────────
if want("M_DysisMoonbow"):
    # 立在瀑布前面的一片透明片上画一道弧：白里带一点七色（外红内紫）。
    # 2026-10-10 重做（用户：光效突兀、虹本身不好看）：以前是一道边缘清楚的弧，里外各晕开一大片亮光，像贴上去的一个发光的圈。
    # 现在：弧是软的、没有硬边；只在“有水汽的地方”显出来——两层慢慢飘的雾，一段浓一段淡；两只脚慢慢淡进水面上的水汽里；
    # 那一大片亮光去掉了，只留贴着弧的一点点；上面偶尔有水珠闪一下。
    # 参数 W、H（片子的宽高，厘米）、Radius、Band（弧的半径、半宽）、Opacity、Glow。
    # 2026-10-11 用户（附了照片，二层靠瀑布那头斜着往下看）：月虹的边在一些角度很明显，有一道竖着的分界。两处：
    #   · 弧外面那一层淡淡的光、弧自己的外半边，到片子的侧边还没淡完，被片子的边齐齐切断——现在片子两侧 70 cm、顶上 30 cm 以内都渐渐淡到 0；
    #     （弧的半径也从 6.4 m 收到 6 m，离片子的边远一点：见 DysisDirectorSky.cpp 的 BowRadius）
    #   · 它是一张平的片子，贴着片子的方向看过去，整道弧会压成一根竖着的细亮线——视线和片子的夹角小到 9° 以内开始淡、3° 以内就不画了。
    mat, full = new_mat("/Game/Dysis/Night", "M_DysisMoonbow", unreal.BlendMode.BLEND_ADDITIVE)
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 150)
    camv = node(mat, unreal.MaterialExpressionCameraVectorWS, -1450, 900)
    nrm = axis_world(mat, (0, 0, 1), -1450, 1000)
    P = [scalar(mat, n, v, -1450, 260 + 100 * i) for i, (n, v) in enumerate([("W", 1400.0), ("H", 760.0), ("Radius", 640.0), ("Band", 42.0), ("Glow", 0.38), ("Opacity", 0.0)])]
    c = custom(mat, NOISE + """
float2 p = float2(LP.x * W / 100.0, H * 0.5 - LP.y * H / 100.0);             // 横向离中线多远、离底边多高（厘米）
float r = length(p);
float d = (r - Radius) / Band;                                               // 离弧的中线多远（以半宽计）
float t = saturate(0.5 + d * 0.5);                                           // 0 = 内缘，1 = 外缘
float x = (1.0 - t) * 7.0;
%s
float band = exp(-d * d * 1.5);                                              // 软的一道，没有硬边
float m1 = NZ.n(float3(p * 0.0042 + float2(Time * 0.035, -Time * 0.05), Time * 0.03));          // 大团的水汽，慢慢飘
float m2 = NZ.n(float3(p * 0.013 + float2(-Time * 0.05, -Time * 0.11), 3.0 + Time * 0.06));     // 细一点的，往下落
float mist = smoothstep(0.22, 0.78, 0.15 + 0.95 * m1 + 0.45 * (m2 - 0.5));   // 有水汽的地方虹才显出来
float ang = atan2(max(p.y, 0.0), abs(p.x) + 1.0);                            // 0 = 贴着水面，π/2 = 弧顶
float foot = smoothstep(0.04, 0.40, ang);                                    // 两只脚慢慢淡进水面上的水汽里
float3 col = lerp(float3(0.82, 0.88, 1.0), sp, 0.30);                        // 月虹颜色很淡，近乎白
float halo = exp(-abs(d) * 0.6) * 0.06;                                      // 贴着弧的一点点光
float glint = NZ.sp(p * 0.03, Time * 1.3) * band * 1.4;                      // 水珠偶尔闪一下
float edge = smoothstep(0.0, 5.0, 50.0 - abs(LP.x)) * smoothstep(0.0, 4.0, LP.y + 50.0);   // 片子的两侧、顶边：淡到 0，不留一道直边
float graze = smoothstep(0.05, 0.16, abs(dot(normalize(CamV), normalize(Nrm))));          // 贴着片子看：淡掉，不压成一根线
return (col * (band * (0.22 + 0.78 * mist) + glint) + float3(0.70, 0.80, 1.0) * (halo * mist)) * (foot * edge * graze * Glow * Opacity);
""" % SPECTRUM, [("LP", lp), ("Time", time), ("CamV", camv), ("Nrm", nrm)] + [(p.get_editor_property("parameter_name"), p) for p in P], F3, -900, 0)
    emissive(mat, c)
    finish(mat, full)

# ───────────────────────── 影桥：水面上的一片月影 ─────────────────────────
if want("M_DysisShadowBridge"):
    # 一块平放的透明片（引擎 Plane，本地 x 沿着桥、y 横着）。没接上时是一片淡淡的暗影；接上以后亮成月白色，边上是软的、有水波一样的起伏和细碎的光。
    # 两头各淡出去一小段，不硬生生压在半桥和水亭上。参数 Color、Glow、Opacity（代码写）。
    mat, full = new_mat("/Game/Dysis/Night", "M_DysisShadowBridge", unreal.BlendMode.BLEND_TRANSLUCENT)
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    ax, ay = axis_world(mat, (1, 0, 0), -1450, 150), axis_world(mat, (0, 1, 0), -1450, 290)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 430)
    col = rgb_of(mat, vector(mat, "Color", (0.03, 0.04, 0.07), -1700, 540), -1450, 540)
    glow, opac = scalar(mat, "Glow", 0.0, -1450, 700), scalar(mat, "Opacity", 0.55, -1450, 800)
    c = custom(mat, NOISE + """
float lx = length(AX), ly = length(AY);
float2 w = float2(LP.x * lx, LP.y * ly);                                     // 片子上的位置（厘米）：x 沿桥、y 横着
float halfL = 50.0 * lx, halfW = 50.0 * ly;
float rip = NZ.n(float3(w * 0.02, Time * 0.35)) - 0.5;                        // 水波：边沿跟着轻轻晃
float side = smoothstep(0.0, 26.0, halfW - abs(w.y) + rip * 14.0);
float ends = smoothstep(0.0, 26.0, halfL - abs(w.x));
float spark = (NZ.sp(w / 10.0, Time * 2.0) * 1.3 + NZ.sp(w / 23.0 + 3.1, Time * 1.3) * 0.8) * Glow;   // 接上以后上面一颗一颗明灭的光点
float flow = 0.85 + 0.15 * sin(w.x * 0.02 - Time * 0.9);
return float4(Color * (0.2 + Glow) * flow + spark, side * ends);
""", [("LP", lp), ("AX", ax), ("AY", ay), ("Time", time), ("Color", col), ("Glow", glow)], F4, -900, 0)
    one = node(mat, unreal.MaterialExpressionConstant, -1450, 900, r=1.0)
    lit_alpha(mat, c, one, opac)
    finish(mat, full)

def noise_code(tag):
    # 一个材质里有两个自定义节点都要用噪声时，结构体的名字得不一样
    return NOISE.replace("FDysisNoise", "FDysisNoise" + tag)

# ───────────────────────── 月桥：月光凝成的桥面 ─────────────────────────
if want("M_DysisMoonBridge"):
    # 半透明、会发光的月白色桥面：迎着看是透的、斜着看亮（像一层薄薄的光），两边各有一道亮边，往外是软的；
    # 上面有慢慢流动的明暗和一颗一颗明灭的光点。显形时月光从一层那一头顺着桥淌下来（Reveal 0→1）。
    # 桥在平面上是一段圆弧（灰盒 MOONBR：圆心 ArcC、半径 ArcR、从 Ang0 转过 AngSpan 弧度），所以“离中线多远、沿桥走了多远”在材质里直接算。
    # 两头各软掉一小段，挨着石头的地方（落到水庭地面、接上一层走廊）自己化开。参数 Reveal、Glow、Opacity、Pivot（模型原点，代码写）、ArcC、ArcR、Ang0、AngSpan、HalfW。
    mat, full = new_mat("/Game/Dysis/Night", "M_DysisMoonBridge", unreal.BlendMode.BLEND_TRANSLUCENT, two_sided=True)
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    ax, ay, az = axis_world(mat, (1, 0, 0), -1450, 150), axis_world(mat, (0, 1, 0), -1450, 290), axis_world(mat, (0, 0, 1), -1450, 430)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 560)
    fres = node(mat, unreal.MaterialExpressionFresnel, -1450, 660, exponent=2.5, base_reflect_fraction=0.06)
    sd, pd = node(mat, unreal.MaterialExpressionSceneDepth, -1450, 760), node(mat, unreal.MaterialExpressionPixelDepth, -1450, 860)
    pivot = rgb_of(mat, vector(mat, "Pivot", (0.0, 0.0, 0.0), -1700, 960), -1450, 960)
    arcc = rgb_of(mat, vector(mat, "ArcC", (0.0, 0.0, 0.0), -1700, 1120), -1450, 1120)
    P = [scalar(mat, n, v, -1450, 1280 + 100 * i) for i, (n, v) in enumerate([("Reveal", 1.0), ("ArcR", 1000.0), ("Ang0", 0.0), ("AngSpan", 3.0), ("HalfW", 75.0), ("EndSoft0", 34.0), ("EndSoft1", 120.0)])]
    glow, opac = scalar(mat, "Glow", 1.0, -1450, 1800), scalar(mat, "Opacity", 0.85, -1450, 1900)
    c = custom(mat, NOISE + """
float3 W = Pivot + AX * LP.x + AY * LP.y + AZ * LP.z;                         // 世界位置（厘米）
float2 rel = W.xy - ArcC.xy;
float v = (length(rel) - ArcR) / HalfW;                                       // 横着：−1 … 1
float sgn = AngSpan < 0.0 ? -1.0 : 1.0, span = abs(AngSpan);
float margin = (6.2831853 - span) * 0.5;
float da = fmod(sgn * (atan2(rel.y, rel.x) - Ang0) + margin + 62.831853, 6.2831853) - margin;
float s = da * ArcR, len = span * ArcR, u = da / span;                        // 沿桥走了多少厘米、全长、0–1
float tip = lerp(0.5, 1.0, smoothstep(0.0, 260.0, len - s));                  // 落到水庭的那一头收窄成一个尖（旁边就是去水闸的石阶，整幅宽伸过去会被石阶切出缺口）
float side = smoothstep(1.0, 0.70, abs(v) / tip);                             // 两边软
float rim = smoothstep(0.42, 0.86, abs(v) / tip) * side;                      // 两边各一道亮边
float ends = smoothstep(0.0, EndSoft0, s) * smoothstep(0.0, EndSoft1, len - s);
float touch = saturate((SD - PD) / 20.0);                                     // 挨着石头的地方化开
float n = NZ.n(float3(s * 0.011, v * 1.4, Time * 0.2));
float edgeU = Reveal * 1.18;
float grow = smoothstep(edgeU + 0.001, edgeU - 0.16, u + (n - 0.5) * 0.10);   // 显形：月光从一层那头淌下来，前沿是散的
float flow = 0.72 + 0.56 * NZ.n(float3(s * 0.0065 - Time * 0.13, v * 0.9, Time * 0.07));
float2 suv = float2(s, v * HalfW);
float spark = NZ.sp(suv / 10.0, Time * 2.0) * 1.4 + NZ.sp(suv / 24.0 + 3.1, Time * 1.3) * 0.9;
float3 col = float3(0.56, 0.71, 1.0) * (0.38 + 0.95 * Fres) * flow + float3(0.78, 0.88, 1.0) * rim * 0.55 + spark * float3(0.85, 0.92, 1.0);
return float4(col, saturate(0.46 + 0.42 * Fres + 0.30 * rim) * side * ends * touch * grow);
""", [("LP", lp), ("AX", ax), ("AY", ay), ("AZ", az), ("Time", time), ("Fres", fres), ("SD", sd), ("PD", pd), ("Pivot", pivot), ("ArcC", arcc)] + named(P), F4, -900, 0)
    lit_alpha(mat, c, glow, opac)
    finish(mat, full)

# ───────────────────────── 瀑布 ─────────────────────────
# 瀑布这几个材质共用的开头：世界位置（殿心是原点）、离殿心多远、方位
FALL_HEAD = """
float3 W = Pivot + AX * LP.x + AY * LP.y + AZ * LP.z;
float rr = length(W.xy);
float ang = degrees(atan2(W.y, W.x)); ang += ang < 0.0 ? 360.0 : 0.0;
"""
def fall_inputs(mat):
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    ax, ay, az = axis_world(mat, (1, 0, 0), -1450, 150), axis_world(mat, (0, 1, 0), -1450, 290), axis_world(mat, (0, 0, 1), -1450, 430)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 560)
    pivot = rgb_of(mat, vector(mat, "Pivot", (0.0, 0.0, 0.0), -1700, 660), -1450, 660)
    tint = rgb_of(mat, vector(mat, "Tint", (0.45, 0.50, 0.55), -1700, 820), -1450, 820)
    return [("LP", lp), ("AX", ax), ("AY", ay), ("AZ", az), ("Time", time), ("Pivot", pivot), ("Tint", tint)]

if want("M_DysisWaterfall"):
    # 水帘（2026-10-10 第二版；用户：原来是平着下来的一片，边也硬，要自然一些）：
    #   · 上沿刚漫出来是薄而整的一片；往下越落越快（花纹沿“落了多久”铺，越往下拉得越长），散成一股一股，股和股之间是透的；
    #   · 两边不是一条直线：水股到了边上一股一股地散掉，越往下散得越宽（EdgeW = 上沿那里边有多宽，度）；
    #   · 落到水面的那一米多泛白。
    # 夜里镜子反射的月光打到的那一块水帘透开（灰盒 FALLHOLE）：中间几乎全透，边上一圈被月光照亮的水——透过它看得见后面的女神。
    # 自己不受光（半透明受光在引擎里很吃力也不好看），亮度和冷暖由代码按时刻给（Tint）。参数 Tint、Opacity、Pivot（模型原点在世界里的位置，代码写）、
    # TopZ / BottomZ（上沿、水面的高度）、A0 / A1（两边的方位，度）、Hole（透开那一块的中心）、HoleR（半径）、HoleK（透开多少）、RimGain。
    # 同一个材质用在好几层上：Phase 每一层不一样（水股错开、快慢差一点，叠着看有前后的错动），Density 是这一层有多浓，
    # Strand = 散成一股一股的程度（0 = 主水帘，还连着；1 = 最前面那一层，全是散开的水股），
    # Flat = 1 是两头封口的侧片（顺着半径铺；靠里的那一边从 InnerR 起软着淡进来）。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisWaterfall", unreal.BlendMode.BLEND_TRANSLUCENT)
    ins = fall_inputs(mat)
    hole = rgb_of(mat, vector(mat, "Hole", (0.0, 0.0, -9900.0), -1700, 980), -1450, 980)
    P = [scalar(mat, n, v, -1450, 1140 + 100 * i) for i, (n, v) in enumerate([("TopZ", 2950.0), ("BottomZ", -45.0), ("A0", 116.25), ("A1", 139.75), ("HoleR", 135.0), ("HoleK", 0.0), ("RimGain", 0.5),
        ("Flat", 0.0), ("Phase", 0.0), ("Density", 1.0), ("Strand", 0.0), ("EdgeW", 2.0), ("InnerR", 1243.0)])]
    one, opac = node(mat, unreal.MaterialExpressionConstant, -1450, 2500, r=1.0), scalar(mat, "Opacity", 1.0, -1450, 2600)
    c = custom(mat, NOISE + FALL_HEAD + """
float u = lerp(radians(ang) * rr, rr, Flat) + Phase * 137.0, v = W.z;         // 沿水帘横着多远、多高（厘米）；侧片顺着半径铺
float fallD = max(TopZ - v, 0.0);                                             // 落下来多远
float q = sqrt(fallD + 60.0) * 22.0;                                          // “落了多久”：越往下水越快，花纹跟着拉长
float tk = Time * (1.0 - 0.1 * Phase);
float s1 = NZ.n(float3(u * 0.045, (q - tk * 300.0) * 0.012, 1.7 + Phase));    // 粗的水股
float s2 = NZ.n(float3(u * 0.17, (q - tk * 420.0) * 0.030, 5.1 + Phase));     // 细的水丝
float s3 = NZ.n(float3(u * 0.018, (q - tk * 170.0) * 0.006, 9.3));            // 一阵一阵的浓淡
float lumps = NZ.n(float3(u * 0.03, (q - tk * 360.0) * 0.020, 3.3 + Phase));  // 一团一团往下掉的水
float lip = exp(-fallD / 240.0);                                              // 上沿：薄而整的一片
float breakup = smoothstep(120.0, 1500.0, fallD) * (0.5 + 0.5 * Strand);      // 往下散开多少
float rope = smoothstep(0.34 + 0.16 * breakup, 0.58 + 0.06 * breakup, s1 * 0.62 + s3 * 0.38);   // 水股（1）和股之间的空（0）
float thread = pow(s2, 2.4);
float body = lerp(0.50 + 0.18 * s1, rope * (0.30 + 0.55 * lumps), breakup);
float edgeN = NZ.n(float3((q - tk * 240.0) * 0.02, Phase * 7.0 + 2.0, u * 0.004));   // 边缘随高度、时间进进出出
float edge = saturate(min(ang - A0, A1 - ang) / max(EdgeW * (0.7 + 1.3 * breakup), 0.01));
float sideArc = smoothstep(0.0, 1.0, edge * (0.45 + 1.1 * edgeN)) * lerp(1.0, 0.35 + 0.65 * rope, (1.0 - edge) * breakup);
float sideFlat = smoothstep(0.0, 1.0, saturate((rr - InnerR) / 70.0) * (0.45 + 1.1 * edgeN));
float side = lerp(sideArc, sideFlat, Flat);
float foam = exp(-max(v - BottomZ, 0.0) / 170.0);                             // 落到水面的地方泛白
float a = saturate(body + thread * (0.5 + 0.45 * breakup) + lip * 0.22 + foam * 0.75) * side * saturate(fallD / 24.0) * Density;
float hd = length(W - Hole) / max(HoleR, 1.0);
float open = HoleK * (1.0 - smoothstep(0.55, 1.0, hd));                       // 月光打到的那一块透开
float rim = HoleK * smoothstep(0.5, 0.85, hd) * (1.0 - smoothstep(0.85, 1.25, hd));   // 边上一圈被月光照亮的水
float3 col = Tint * (0.40 + 0.95 * thread + 0.30 * rope * lumps + lip * 0.30 + foam * 0.65) + float3(0.5, 0.6, 0.9) * (rim * 0.8 * RimGain);
return float4(col, a * 0.9 * (1.0 - 0.95 * open) + 0.25 * rim);
""", ins + [("Hole", hole)] + named(P), F4, -900, 0)
    lit_alpha(mat, c, one, opac)
    finish(mat, full)

if want("M_DysisFallSplash"):
    # 瀑布脚下的水花：贴着水帘、从水面往上蹿的一团团白水，越高越稀。铺在一段矮的水帘形状上（瀑布那个模型压扁了用）。
    # 参数 Tint、Opacity、Pivot、BottomZ（水面）、Height（最高蹿多高，厘米）、A0 / A1、Phase。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisFallSplash", unreal.BlendMode.BLEND_TRANSLUCENT)
    ins = fall_inputs(mat)
    P = [scalar(mat, n, v, -1450, 1140 + 100 * i) for i, (n, v) in enumerate([("BottomZ", -45.0), ("Height", 230.0), ("A0", 116.25), ("A1", 139.75), ("Phase", 0.0)])]
    one, opac = node(mat, unreal.MaterialExpressionConstant, -1450, 1900, r=1.0), scalar(mat, "Opacity", 1.0, -1450, 2000)
    c = custom(mat, NOISE + FALL_HEAD + """
float u = radians(ang) * rr + Phase * 91.0, v = W.z;
float h = saturate((v - BottomZ) / max(Height, 1.0));                         // 0 = 水面，1 = 水花最高的地方
float n1 = NZ.n(float3(u * 0.028, (v - Time * 150.0) * 0.016, Time * 0.7 + Phase));           // 往上蹿的一团团
float n2 = NZ.n(float3(u * 0.075, (v - Time * 230.0) * 0.035, Time * 1.3 + Phase * 3.0));     // 碎的
float th = lerp(0.16, 0.80, pow(h, 0.7));                                     // 越高，够得着的越少
float a = smoothstep(th, th + 0.16, n1 * 0.65 + n2 * 0.35) * (1.0 - h * h);
a *= smoothstep(0.0, 1.0, saturate(min(ang - A0, A1 - ang) / 2.5));           // 两头软着收
return float4(Tint * (0.75 + 0.5 * n2), a * 0.85);
""", ins + named(P), F4, -900, 0)
    lit_alpha(mat, c, one, opac)
    finish(mat, full)

if want("M_DysisFallFoam"):
    # 瀑布入水的地方：贴在水面上的一片。水帘落点跟前是翻着的白沫，往中庭那边一圈一圈的波纹推出去、越远越淡；水帘后面（到石台）也是白的。
    # 参数 Tint、Opacity、Pivot、R0（水帘落在离殿心多远）、Reach（白沫和波纹往外推多远，厘米）、A0 / A1。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisFallFoam", unreal.BlendMode.BLEND_TRANSLUCENT)
    ins = fall_inputs(mat)
    P = [scalar(mat, n, v, -1450, 1140 + 100 * i) for i, (n, v) in enumerate([("R0", 1245.0), ("Reach", 330.0), ("A0", 116.25), ("A1", 139.75)])]
    one, opac = node(mat, unreal.MaterialExpressionConstant, -1450, 1900, r=1.0), scalar(mat, "Opacity", 1.0, -1450, 2000)
    c = custom(mat, NOISE + FALL_HEAD + """
float d = R0 - rr;                                                            // 离落点多远：正 = 朝中庭这边，负 = 水帘后面
float din = max(d, 0.0), dback = max(-d, 0.0);
float u = radians(ang) * rr;
float nearK = exp(-din / 70.0) * exp(-dback / 150.0);                         // 落点跟前一片白（变量名别用 line / near / far：HLSL 的保留字，材质会编不过、变成灰格子）
float churn = NZ.n(float3(u * 0.02, (din - Time * 60.0) * 0.016, Time * 0.5));
float fine = NZ.n(float3(u * 0.07, (din - Time * 95.0) * 0.05, Time * 0.9));
float blobs = smoothstep(0.42, 0.75, churn);                                  // 一团一团翻着的白沫
float ring = 0.5 + 0.5 * sin((din - Time * 85.0) * 0.07 + churn * 3.0);       // 一圈一圈往外推的波
float crest = smoothstep(0.80, 0.98, ring);                                   // 只留波峰那一条细线
float farK = exp(-din / max(Reach, 1.0));
float a = saturate(nearK * (0.35 + 0.55 * blobs) + farK * (0.16 * crest * (0.4 + 0.6 * fine) + 0.14 * blobs * smoothstep(0.5, 0.8, fine)));
a *= smoothstep(0.0, 1.0, saturate(min(ang - A0 + 3.0, A1 + 3.0 - ang) / 5.0)) * smoothstep(Reach * 1.9, Reach * 0.8, din);
return float4(Tint * (0.8 + 0.4 * fine), a);
""", ins + named(P), F4, -900, 0)
    lit_alpha(mat, c, one, opac)
    finish(mat, full)

if want("M_DysisDroplet"):
    # 溅起来的水珠（和光粒 M_DysisMote 一样是一组小片，位置全在材质里算，挂在 UDysisMoteField 上）：
    # 每一滴从盒子靠里的那条底边（水帘落点）蹦出来——往外（盒子的 +Y）、往上、再带一点左右——抛物线落回去，落完了从头再来。
    # 参数 Color、Glow、Opacity、Size（厘米）、Life（一滴飞多久，秒）、Burst（左右、往外、往上最快多少，厘米/秒）、SizeCm（盒子三边多长，代码写）。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisDroplet", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1700, -300)
    data = node(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -1700, -150, data_index=0, const_default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    time = node(mat, unreal.MaterialExpressionTime, -1700, 0)
    ax, ay, az = axis_world(mat, (1, 0, 0), -1450, 150), axis_world(mat, (0, 1, 0), -1450, 290), axis_world(mat, (0, 0, 1), -1450, 430)
    def view_axis(v, y):
        c = node(mat, unreal.MaterialExpressionConstant3Vector, -1700, y, constant=unreal.LinearColor(v[0], v[1], v[2], 0.0))
        t = node(mat, unreal.MaterialExpressionTransform, -1450, y, transform_source_type=VEC_SRC.TRANSFORMSOURCE_VIEW, transform_type=VEC_DST.TRANSFORM_WORLD)
        mel.connect_material_expressions(c, "", t, ""); return t
    cr, cu = view_axis((1, 0, 0), 570), view_axis((0, 1, 0), 710)
    burst = rgb_of(mat, vector(mat, "Burst", (70.0, 200.0, 380.0), -1700, 860), -1450, 860)
    box = rgb_of(mat, vector(mat, "SizeCm", (500.0, 300.0, 240.0), -1700, 1020), -1450, 1020)
    size, life = scalar(mat, "Size", 5.0, -1450, 1180), scalar(mat, "Life", 0.9, -1450, 1280)
    lpv = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 1400), -1450, 1400)
    FLY = """
float3 r = Data;                                                   // 这一滴自己的三个随机数
float life = Life * (0.7 + 0.6 * frac(r.z * 7.13));
float tn = frac(Time / life + r.x), tt = tn * life;                // 飞到第几成、飞了几秒
float vup = Burst.z * (0.45 + 0.55 * frac(r.z * 4.41 + r.y * 2.0));
float zz = vup * tt - 490.0 * tt * tt;                             // 现在比起点高多少
"""
    wpo = custom(mat, FLY + """
float lat = (frac(r.x * 5.71 + r.z * 3.3) - 0.5) * 2.0 * Burst.x;
float vin = Burst.y * (0.25 + 0.75 * frac(r.y * 9.17 + r.x));
float3 pc = float3((r.y - 0.5) * SizeCm.x + lat * tt, -0.5 * SizeCm.y + vin * tt, -0.5 * SizeCm.z + zz);   // 盒子里的位置（厘米）
float3 d = pc / SizeCm * 100.0 - LP;                               // 这个顶点原来在哪（模型自己的坐标，盒子是 ±50）→ 挪到这一滴的位置
float s = Size * (0.5 + 0.9 * frac(r.x * 7.31 + r.y * 3.17)) * (1.0 - 0.5 * tn);
return AX * d.x + AY * d.y + AZ * d.z + (CR * (UV.x - 0.5) + CU * (0.5 - UV.y)) * s;
""", [("Data", data), ("Time", time), ("Life", life), ("Burst", burst), ("SizeCm", box), ("LP", lpv), ("AX", ax), ("AY", ay), ("AZ", az), ("CR", cr), ("CU", cu), ("UV", uv), ("Size", size)], F3, -900, 300)
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    vi = node(mat, unreal.MaterialExpressionVertexInterpolator, -1450, -150)
    mel.connect_material_expressions(data, "", vi, "")
    col = rgb_of(mat, vector(mat, "Color", (0.85, 0.93, 1.0), -1700, -520), -1450, -520)
    glow, opac = scalar(mat, "Glow", 2.0, -1450, -420), scalar(mat, "Opacity", 1.0, -1450, -330)
    em = custom(mat, FLY + """
float2 q = UV * 2.0 - 1.0;
float a = saturate(1.0 - length(q)); a = a * a;                    // 一个小圆点，边上软
float fade = smoothstep(0.0, 0.08, tn) * (1.0 - tn) * smoothstep(-30.0, 5.0, zz);   // 刚蹦出来渐显、越飞越淡，落回水面以下就没了
return Color * (a * fade * Glow * Opacity);
""", [("UV", uv), ("Data", vi), ("Time", time), ("Life", life), ("Burst", burst), ("Color", col), ("Glow", glow), ("Opacity", opac)], F3, -900, -300)
    emissive(mat, em)
    finish(mat, full)

# ───────────────────────── 海、水庭的水 ─────────────────────────
def make_water(name, wave_cm, wave_h, speed, deep, shallow, rough, spec, big, rings):
    # 受光、不透明的水面：波纹是材质里现算的（细的、中的两层来回错开，海上再加一层大的涌），所以太阳、月亮照上去自己会有碎光。
    # 水本身是带一点青的蓝（迎着看深、斜着看浅）；贴着水面看过去映出天色（SkyTint，代码按时刻给——夜里引擎自己的天是黑的，映不出我们画的夜空）；
    # 波峰上有一颗一颗明灭的亮点。rings = (r0, r1)：水挨着石头的那两圈（水亭的座子外沿、水池边）各有一道轻轻起伏的白沫。
    # 参数 SkyTint、DeepColor、ShallowColor、WaveCm、WaveHeight、WaveSpeed、Reflect、Glint、Pivot（模型原点，代码写）。
    mat, full = new_mat("/Game/Dysis/Fx", name, unreal.BlendMode.BLEND_OPAQUE, unlit=False, two_sided=False)
    lp = local_of(mat, node(mat, unreal.MaterialExpressionWorldPosition, -1700, 0), -1450, 0)
    ax, ay = axis_world(mat, (1, 0, 0), -1450, 150), axis_world(mat, (0, 1, 0), -1450, 290)
    time = node(mat, unreal.MaterialExpressionTime, -1450, 430)
    depth = node(mat, unreal.MaterialExpressionPixelDepth, -1450, 520)
    fres = node(mat, unreal.MaterialExpressionFresnel, -1450, 620, exponent=4.0, base_reflect_fraction=0.03)
    sky = rgb_of(mat, vector(mat, "SkyTint", (0.45, 0.62, 0.85), -1700, 740), -1450, 740)
    deepc = rgb_of(mat, vector(mat, "DeepColor", deep, -1700, 900), -1450, 900)
    shal = rgb_of(mat, vector(mat, "ShallowColor", shallow, -1700, 1060), -1450, 1060)
    pivot = rgb_of(mat, vector(mat, "Pivot", (0.0, 0.0, 0.0), -1700, 1220), -1450, 1220)
    P = [scalar(mat, n, v, -1450, 1380 + 100 * i) for i, (n, v) in enumerate([("WaveCm", wave_cm), ("WaveHeight", wave_h), ("WaveSpeed", speed), ("Reflect", 0.6), ("Glint", 1.0)])]
    common = [("LP", lp), ("AX", ax), ("AY", ay), ("Time", time), ("PD", depth), ("Pivot", pivot)] + named(P[:3])
    height = """
float2 w = Pivot.xy + AX.xy * LP.x + AY.xy * LP.y;                            // 水面上的位置（厘米，殿心是原点）
float far = saturate(1.0 - PD / (WaveCm * 220.0));                            // 远处把波纹压平（不然一片花）
float2 e = float2(WaveCm * 0.06, 0.0);
float t = Time * WaveSpeed;
#define DYSIS_H(q) (NZ.n(float3((q) / WaveCm + float2(t * 0.11, t * 0.07), t * 0.13)) * 0.6 + NZ.n(float3((q) / (WaveCm * 0.37) - float2(t * 0.17, -t * 0.09), t * 0.21 + 3.0)) * 0.28 + NZ.n(float3((q) / (WaveCm * 3.1) + float2(-t * 0.03, t * 0.05), 7.0)) * %.2f)
float h0 = DYSIS_H(w), hx = DYSIS_H(w + e.xy), hy = DYSIS_H(w + e.yx);
float rr = length(w);
float lap = 6.0 * sin(Time * 0.7 + rr * 0.01) + 10.0 * (h0 - 0.5);            // 水线一进一退
float foam = max(exp(-max(rr - %.1f + lap, 0.0) / 7.0) * step(%.1f - 2.0, rr), exp(-max(%.1f - rr + lap, 0.0) / 9.0)) * %.1f;
foam *= 0.25 + 0.5 * NZ.n(float3(w * 0.045, Time * 0.4));
""" % (big, rings[0], rings[0], rings[1], 1.0 if rings[1] > 0 else 0.0)
    normal = custom(mat, noise_code("N") + height + """
float k = WaveHeight * far / e.x;
return normalize(float3(-(hx - h0) * k, -(hy - h0) * k, 1.0));
""", common, F3, -900, -200)
    emis = custom(mat, noise_code("E") + height + """
// 贴着水面看过去映出天色；波峰上再提一点亮，上面有一颗一颗明灭的亮点；挨着石头的地方一道白沫
float crest = smoothstep(0.62, 0.95, h0) * far;
float glint = NZ.sp(w / (WaveCm * 0.11), Time * 1.7) * smoothstep(0.45, 0.8, h0) * far * Glint;
return Sky * (Fres * Reflect + crest * 0.05 + glint * 1.6 + foam * 0.25) + lerp(Deep, Shallow, crest * 0.5) * 0.02;
""", common + [("Fres", fres), ("Sky", sky), ("Deep", deepc), ("Shallow", shal), ("Reflect", P[3]), ("Glint", P[4])], F3, -900, 300)
    foamv = custom(mat, noise_code("F") + height + "return foam;", common, F1, -900, 700)
    base0 = node(mat, unreal.MaterialExpressionLinearInterpolate, -550, -400)
    mel.connect_material_expressions(deepc, "", base0, "A"); mel.connect_material_expressions(shal, "", base0, "B"); mel.connect_material_expressions(fres, "", base0, "Alpha")
    white = node(mat, unreal.MaterialExpressionConstant3Vector, -550, -560, constant=unreal.LinearColor(0.75, 0.80, 0.82, 1.0))
    base = node(mat, unreal.MaterialExpressionLinearInterpolate, -350, -400)
    mel.connect_material_expressions(base0, "", base, "A"); mel.connect_material_expressions(white, "", base, "B"); mel.connect_material_expressions(foamv, "", base, "Alpha")
    rgh = node(mat, unreal.MaterialExpressionLinearInterpolate, -350, 520, const_a=rough, const_b=0.8)
    mel.connect_material_expressions(foamv, "", rgh, "Alpha")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    emissive(mat, emis)
    mel.connect_material_property(rgh, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(mat, unreal.MaterialExpressionConstant, -350, 640, r=spec), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(node(mat, unreal.MaterialExpressionConstant, -350, 720, r=0.0), "", unreal.MaterialProperty.MP_METALLIC)
    finish(mat, full)

if want("M_DysisSea"):
    make_water("M_DysisSea", 420.0, 60.0, 1.0, (0.012, 0.070, 0.115), (0.04, 0.20, 0.25), 0.10, 0.5, 0.5, (0.0, 0.0))
if want("M_DysisPool"):
    # 水庭里四面都是亮的墙，镜子一样的水面会映成一片白：糙一点、反光弱一点，让水自己的青蓝透出来；波纹只留细的两层
    make_water("M_DysisPool", 70.0, 5.0, 0.7, (0.022, 0.115, 0.160), (0.075, 0.27, 0.31), 0.22, 0.08, 0.0, (300.0, 1020.0))

# ───────────────────────── 水庭的水：场景美术的动画水面 ─────────────────────────
# 场景美术 2026-10-09 的风格化水面（原名 M_DYSIS_AnimeWater）：雾蓝、青绿、蓝紫的大色块，稀疏的断线，岸边断续的波纹，水亭旁一片桃金色的反光。
# 着色的那段代码和三张数据贴图照搬他的源文件（Art/Fx/AnimeWater_20261009/water_shader.hlsl、T_DYSIS_*.png，贴图由同目录的 build_data_textures.py 从水面的真实轮廓算出来）：
#   PoolShore   R = 离水边多远（÷200 cm）、G = 浅水区（手画的，不是真的水深）、B = 水亭旁那片暖色反光的范围
#   PoolFlow    RG = 水往哪边流（大体朝一个方向，贴着岸 90 cm 以内顺着岸弯）
#   WaterShapes R = 三档深浅的大色块、G = 一道一道的断线、B = 扭动用的
# 和他那份不一样的地方（我们的光是另一套：自动曝光、勒克斯数很小、会走到夜里）：
#   · 他用“颜色 ×180”的自发光给水垫一个底色，那是配他固定曝光的数；这里改成“屏幕上看着多亮”（Lift，代码按白天 / 夜里给），
#     受光的那一份只留三成多（LitShare）——所以太阳照进来的那几块水面会亮一些、夜里跟着暗下去，但颜色不会被殿里的暖光照灰；
#   · 他的“流动 / 起波纹”两个状态是从一张全局参数表读的，这里是两个普通参数 FlowState、RippleState，代码按瀑布开了多少写。
# 参数（名字和他的一样）：RepeatCm、MainSpeedCm、SecondSpeedCm、CycleSeconds、ShoreWidthCm、LineDensity、WarpStrength、WarmReflectionStrength、LocalFlowStrength、
#   MainColor、ShallowColor、DeepColor、LineColor、WarmColor、FlowDirection；数值是他最后定的那一组。
ANIME_DIR = PROJ + "Art/Fx/AnimeWater_20261009/"
def import_data_texture(name, wrap):
    # 数据贴图：不按颜色解码（sRGB 关）、按“遮罩”压缩；大色块那张要能平铺，另外两张是整池一张、边上夹住
    path = "/Game/Dysis/Fx/Textures/" + name
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", ANIME_DIR + name.replace("T_Dysis", "T_DYSIS_") + ".png")
    task.set_editor_property("destination_path", "/Game/Dysis/Fx/Textures"); task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True); task.set_editor_property("replace_existing", True); task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(path)
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    addr = unreal.TextureAddress.TA_WRAP if wrap else unreal.TextureAddress.TA_CLAMP
    tex.set_editor_property("address_x", addr); tex.set_editor_property("address_y", addr)
    print(name, "saved:", eal.save_asset(path, only_if_is_dirty=False))
    return tex

if want("M_DysisAnimeWater"):
    T_SHORE, T_FLOW, T_SHAPES = import_data_texture("T_DysisPoolShore", False), import_data_texture("T_DysisPoolFlow", False), import_data_texture("T_DysisWaterShapes", True)
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisAnimeWater", unreal.BlendMode.BLEND_OPAQUE, unlit=False, two_sided=False)
    def texobj(tex, x, y): return node(mat, unreal.MaterialExpressionTextureObject, x, y, texture=tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    wp, time = node(mat, unreal.MaterialExpressionWorldPosition, -1500, -300), node(mat, unreal.MaterialExpressionTime, -1500, -200)
    S = [scalar(mat, n, v, -1500, 300 + 90 * i) for i, (n, v) in enumerate([("RepeatCm", 400.0), ("MainSpeedCm", 5.0), ("SecondSpeedCm", 2.5), ("CycleSeconds", 8.0), ("ShoreWidthCm", 24.0),
        ("LineDensity", 0.52), ("WarpStrength", 0.012), ("WarmReflectionStrength", 0.65), ("LocalFlowStrength", 0.65), ("FlowState", 0.0), ("RippleState", 0.0)])]
    V = [vector(mat, n, v, -1800, 300 + 160 * i) for i, (n, v) in enumerate([("MainColor", (0.25, 0.45, 0.52)), ("ShallowColor", (0.39, 0.66, 0.54)), ("DeepColor", (0.20, 0.26, 0.40)),
        ("LineColor", (0.73, 0.90, 0.81)), ("WarmColor", (0.95, 0.67, 0.35)), ("FlowDirection", (0.85, 0.52, 0.0))])]
    code = open(ANIME_DIR + "water_shader.hlsl", encoding="utf-8").read()
    # 瀑布底下那一段水槽（方位 116.4–139.6°）接在水池的外沿上（DysisDirectorFx.cpp 在那里补了一片水面，用的也是这个材质）：
    # 这一段池边不是岸，往外是开着的水——不画岸边那圈波纹；水槽里（贴图管不到的地方）按离岸远的深水算。两侧挨着槽壁的 20 cm 还留着波纹。
    INLET = """float inletAz=degrees(atan2(P.y,P.x)); inletAz+=inletAz<0?360:0;
float inlet=smoothstep(116.4,117.6,inletAz)*(1-smoothstep(138.4,139.6,inletAz))*smoothstep(900,985,length(P.xy));
shore.r=lerp(shore.r,1,inlet); shore.g=lerp(shore.g,.25,inlet); shore.b=lerp(shore.b,0,inlet);
"""
    assert "float2 flow=Texture2DSample" in code
    code = code.replace("float2 flow=Texture2DSample", INLET + "float2 flow=Texture2DSample", 1)
    color = custom(mat, code, [("P", wp), ("T", time), ("Shore", texobj(T_SHORE, -1500, -100)), ("Flow", texobj(T_FLOW, -1500, 0)), ("Shapes", texobj(T_SHAPES, -1500, 100))] + named(S) + named(V), F3, -900, 0)
    lift = node(mat, unreal.MaterialExpressionMultiply, -500, 200)
    mel.connect_material_expressions(color, "", lift, "A"); mel.connect_material_expressions(scalar(mat, "Lift", 0.10, -700, 320), "", lift, "B")
    # 受光的那一份只留一部分（LitShare）：殿里的光是暖的，整份受光的话青绿和蓝紫会被照成发灰的米色；水自己的颜色主要靠上面垫的那一层
    lit = node(mat, unreal.MaterialExpressionMultiply, -500, -100)
    mel.connect_material_expressions(color, "", lit, "A"); mel.connect_material_expressions(scalar(mat, "LitShare", 0.35, -700, -40), "", lit, "B")
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emissive(mat, lift)
    for prop, name, val, y in ((unreal.MaterialProperty.MP_ROUGHNESS, "Roughness", 0.70, 480), (unreal.MaterialProperty.MP_SPECULAR, "Specular", 0.16, 570), (unreal.MaterialProperty.MP_METALLIC, "Metallic", 0.0, 660)):
        mel.connect_material_property(scalar(mat, name, val, -350, y), "", prop)
    # FxMaterial 会给每个换上去的材质写一个 Pivot；这个材质用不上，留一个同名的参数接着（不接出去）
    vector(mat, "Pivot", (0.0, 0.0, 0.0), -1800, 1300)
    finish(mat, full)

# ───────────────────────── 海：“天空之镜”那一版 ─────────────────────────
if want("M_DysisSeaMirror"):
    # 2026-10-10 用户想试试把海面做成镜湖（平得像镜子，天、云、岛整个映在里面）；10-11 定下来用它（M 键、Dysis.SeaMirror 0 还能换回原来的 M_DysisSea）。
    # 做法：一面镜子——金属度 1、很光滑，映什么全靠引擎的反射（映的是画面里真的天、云、岛和神殿）。
    # 2026-10-11 用户（人出生在水面上以后）：加一点像水的质感——磨砂，或者一层层轻微的起伏，不然一看就是金属镜子。
    #   只在近处加（离镜头 NearCm 以内，越近越明显）；远处还是一整片平的镜子，从殿上看下去和原来一样：
    #   · 两层缓缓推着走的长波（Swell = 坡度有多大；波长 5.2 m 和 2.3 m，方向不同，波峰用噪声错开一点）；
    #   · 人脚边一圈圈往外荡的水纹（Ring；圆心 Player、强弱 PlayerK 由代码按人在不在水面上、走没走来给）；
    #   · 近处糙一点（Rough → Rough + RoughNear），倒影边上是软的；
    #   · 低头看的地方镜面暗一点、偏深蓝（Deep；水是越斜着看越亮的，金属镜子从哪个角度看都一样亮），DeepK = 加多少。
    #   Tint = 镜面的颜色；Lift = 垫的一点底色 × SkyTint（代码按时刻给的天色，和原来的海用的是同一个）。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisSeaMirror", unreal.BlendMode.BLEND_OPAQUE, unlit=False, two_sided=False)
    mat.set_editor_property("tangent_space_normal", False)      # 法线直接按世界里的方向给（海面是平的，朝上）
    wp, time, depth = node(mat, unreal.MaterialExpressionWorldPosition, -1500, 100), node(mat, unreal.MaterialExpressionTime, -1500, 200), node(mat, unreal.MaterialExpressionPixelDepth, -1500, 300)
    cam = node(mat, unreal.MaterialExpressionCameraVectorWS, -1500, -500)
    near = custom(mat, "float n = saturate(1.0 - PD / NearCm); return n * n;", [("PD", depth), ("NearCm", scalar(mat, "NearCm", 5000.0, -1500, 420))], F1, -1150, 360)
    tint = rgb_of(mat, vector(mat, "Tint", (0.80, 0.86, 0.92), -1500, -400), -1300, -400)
    deep = rgb_of(mat, vector(mat, "Deep", (0.36, 0.50, 0.66), -1500, -250), -1300, -250)
    player = rgb_of(mat, vector(mat, "Player", (0.0, 0.0, -100000.0), -1500, 560), -1300, 560)
    # 人脚边的水纹（−1…1）：一圈圈往外走，几米以外就平了。法线用它（倒影跟着晃），颜色也用它（一圈亮一圈暗，俯看也看得出来：RingShade）
    ringv = custom(mat, "float pd = length(WP.xy - Player.xy) + 1.0; return cos(pd / RingCm * 6.2832 - Time * RingSpeed) * exp(-pd / RingFadeCm) * saturate(pd / 45.0) * PlayerK;",
                   [("WP", wp), ("Time", time), ("Player", player), ("PlayerK", scalar(mat, "PlayerK", 0.0, -1500, 660)), ("RingCm", scalar(mat, "RingCm", 85.0, -1500, 900)),
                    ("RingSpeed", scalar(mat, "RingSpeed", 2.4, -1500, 980)), ("RingFadeCm", scalar(mat, "RingFadeCm", 320.0, -1500, 1060))], F1, -1150, 700)
    base = custom(mat, "float f = pow(1.0 - saturate(CV.z), FresnelPow); return lerp(Tint, lerp(Deep, Tint, f), saturate(Near * DeepK)) * (1.0 + RingV * RingShade);",
                  [("CV", cam), ("Tint", tint), ("Deep", deep), ("Near", near), ("DeepK", scalar(mat, "DeepK", 1.0, -1500, -120)), ("FresnelPow", scalar(mat, "FresnelPow", 2.0, -1500, -40)),
                   ("RingV", ringv), ("RingShade", scalar(mat, "RingShade", 0.10, -1500, 1220))], F3, -900, -400)
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, name, val, y in ((unreal.MaterialProperty.MP_METALLIC, "Metallic", 1.0, -250), (unreal.MaterialProperty.MP_SPECULAR, "Specular", 1.0, -70)):
        mel.connect_material_property(scalar(mat, name, val, -700, y), "", prop)
    rough = custom(mat, "return Rough + RoughNear * Near;", [("Rough", scalar(mat, "Rough", 0.04, -1150, -180)), ("RoughNear", scalar(mat, "RoughNear", 0.06, -1150, -100)), ("Near", near)], F1, -900, -160)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    normal = custom(mat, noise_code("M") + """
float2 w = WP.xy; float t = Time;
// 两层长波：高度 = sin(相位)，这里直接算它对位置的导数（坡度的方向 × cos）
float2 d1 = normalize(float2(0.8, 0.6)), d2 = normalize(float2(-0.35, 0.94));
float nz = NZ.n(float3(w / 700.0, t * 0.08));
float p1 = dot(w, d1) / 520.0 * 6.2832 - t * 0.55 + nz * 2.0, p2 = dot(w, d2) / 230.0 * 6.2832 - t * 0.8 - nz * 1.5;
float2 slope = (d1 * cos(p1) + d2 * cos(p2) * 0.55) * Swell * Near;
// 人脚边的水纹
float2 pv = w - Player.xy;
slope += pv / (length(pv) + 1.0) * RingV * Ring;
return normalize(float3(-slope * Ripple, 1.0));
""", [("WP", wp), ("Time", time), ("Near", near), ("Player", player), ("RingV", ringv), ("Swell", scalar(mat, "Swell", 0.012, -1500, 740)),
      ("Ring", scalar(mat, "Ring", 0.035, -1500, 820)), ("Ripple", scalar(mat, "Ripple", 1.0, -1500, 1140))], F3, -900, 150)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    lift = node(mat, unreal.MaterialExpressionMultiply, -500, 600)
    mel.connect_material_expressions(rgb_of(mat, vector(mat, "SkyTint", (0.45, 0.62, 0.85), -900, 600), -700, 600), "", lift, "A"); mel.connect_material_expressions(scalar(mat, "Lift", 0.0, -700, 720), "", lift, "B")
    emissive(mat, lift)
    vector(mat, "Pivot", (0.0, 0.0, 0.0), -1800, 1300)      # FxMaterial 会写这个参数；这个材质用不上
    finish(mat, full)

# ───────────────────────── 金苹果：镜面一样的金、人走近时周围一圈淡淡的虹光 ─────────────────────────
if want("M_DysisAppleGold"):
    # 2026-10-10 用户：金苹果的金色表面想要更好看，试试镜面。金属度 1、很光滑：一块一块的小平面各映各的，像切过面的金器。
    # 叶子（顶点色偏绿的那几片）不是金属，照模型自己的颜色。Lift = 垫的一点金色（屏幕上看着多亮）：殿里暗的地方镜面映不到什么，不至于发黑。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisAppleGold", unreal.BlendMode.BLEND_OPAQUE, unlit=False, two_sided=False)
    mat.set_editor_property("tangent_space_normal", False)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -1500, -300)
    gold = rgb_of(mat, vector(mat, "Gold", (1.0, 0.74, 0.32), -1500, -150), -1300, -150)
    leaf = custom(mat, "return step(VC.r * 1.05, VC.g);", [("VC", vc)], F1, -1100, -300)
    base = node(mat, unreal.MaterialExpressionLinearInterpolate, -800, -250)
    mel.connect_material_expressions(gold, "", base, "A"); mel.connect_material_expressions(rgb_of(mat, vc, -1300, -300), "", base, "B"); mel.connect_material_expressions(leaf, "", base, "Alpha")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    met = node(mat, unreal.MaterialExpressionLinearInterpolate, -800, -50, const_b=0.0)
    mel.connect_material_expressions(scalar(mat, "Metallic", 1.0, -1100, -50), "", met, "A"); mel.connect_material_expressions(leaf, "", met, "Alpha")
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    rgh = node(mat, unreal.MaterialExpressionLinearInterpolate, -800, 100, const_b=0.7)
    mel.connect_material_expressions(scalar(mat, "Roughness", 0.16, -1100, 100), "", rgh, "A"); mel.connect_material_expressions(leaf, "", rgh, "Alpha")
    mel.connect_material_property(rgh, "", unreal.MaterialProperty.MP_ROUGHNESS)
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1500, 500)
    nrm = custom(mat, "return normalize(cross(ddx(WP), ddy(WP)));", [("WP", wp)], F3, -1100, 500)      # 按面：每个小平面一个法线（顺序反了整件发黑）
    mel.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    lift = custom(mat, "return Gold * Lift * (1.0 - Leaf);", [("Gold", gold), ("Lift", scalar(mat, "Lift", 0.06, -1100, 300)), ("Leaf", leaf)], F3, -800, 300)
    emissive(mat, lift)
    finish(mat, full)
if want("M_DysisAppleHalo"):
    # 金苹果周围的一圈虹光（套在苹果外面的一个球；只看它的边缘：正对着的地方是空的，越到边上越亮，颜色顺着边转一圈虹）。Opacity 由代码按“人离它多近”给。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisAppleHalo", unreal.BlendMode.BLEND_ADDITIVE, unlit=True, two_sided=False)
    nws, cam, time = node(mat, unreal.MaterialExpressionVertexNormalWS, -1500, 0), node(mat, unreal.MaterialExpressionCameraVectorWS, -1500, 100), node(mat, unreal.MaterialExpressionTime, -1500, 200)
    glow = custom(mat, """
float f = 1.0 - abs(dot(normalize(N), normalize(V)));          // 0 = 正对着，1 = 边缘
float ring = smoothstep(0.08, 0.62, f) * (1.0 - smoothstep(0.78, 1.0, f));
float h = f * 1.5 + Time * 0.07;
float3 bow = 0.5 + 0.5 * cos(6.2831853 * (h + float3(0.0, 0.33, 0.67)));
return bow * ring * Opacity;
""", [("N", nws), ("V", cam), ("Time", time), ("Opacity", scalar(mat, "Opacity", 0.0, -1500, 300))], F3, -900, 0)
    emissive(mat, glow)
    finish(mat, full)

if want("M_DysisAppleTrail"):
    # 金苹果的拖尾（2026-10-11 用户：金苹果能有一点光效拖尾吗）：跟着苹果的一条淡金色的光带。
    # 带子是代码每帧现做的（DysisDirectorSky.cpp 的 UpdateAppleTrail）：顶点色的 A = 这一段有多亮（头亮尾淡），贴图坐标的 V = 横着的位置（中间亮、两边软）。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisAppleTrail", unreal.BlendMode.BLEND_ADDITIVE, unlit=True, two_sided=True)
    vc, uv = node(mat, unreal.MaterialExpressionVertexColor, -1500, 0), node(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 200)
    glow = custom(mat, "float e = saturate(1.0 - abs(UV.y * 2.0 - 1.0)); return Color * (e * e * A * Glow);",
                  [("A", vc), ("UV", uv), ("Color", rgb_of(mat, vector(mat, "Color", (1.0, 0.70, 0.28), -1500, 400), -1300, 400)), ("Glow", scalar(mat, "Glow", 0.5, -1500, 520))], F3, -900, 0)
    mel.connect_material_expressions(vc, "A", glow, "A")      # 顶点色只要 A 那一路
    emissive(mat, glow)
    finish(mat, full)

# ───────────────────────── 发光的小东西：棱镜分出来的七道色光、它们落在墙上的光斑、塞勒涅亮起来的眼睛 ─────────────────────────
if want("M_DysisGlow"):
    # 一根细圆柱 / 一块小圆片自己发光：正对着看的地方亮，转到边上淡下去（Edge 越大边越软；0 = 整块一样亮）。
    # 光柱的材质现在是“视线穿过多厚就多亮”的体积光，这么细的东西用它就看不见了（2026-10-10 用户发现棱镜的彩虹光束不见了），所以单做一个。
    # 参数 Color、Intensity（屏幕上的亮度，1 ≈ 白）、Edge。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisGlow", unreal.BlendMode.BLEND_ADDITIVE)
    col = rgb_of(mat, vector(mat, "Color", (1.0, 1.0, 1.0), -1100, -100), -850, -100)
    c = custom(mat, "float f = abs(dot(normalize(N), normalize(CV)));\nreturn Color * (Intensity * pow(max(f, 0.001), Edge));",
        [("N", node(mat, unreal.MaterialExpressionPixelNormalWS, -850, 60)), ("CV", node(mat, unreal.MaterialExpressionCameraVectorWS, -850, 160)),
         ("Color", col), ("Intensity", scalar(mat, "Intensity", 1.0, -850, 260)), ("Edge", scalar(mat, "Edge", 1.3, -850, 360))], F3, -500, 0)
    emissive(mat, c)
    finish(mat, full)

# ───────────────────────── 人脚下的一团影子 ─────────────────────────
if want("M_DysisBlobShadow"):
    # 朝下投的一块贴花：中间把地面压暗、往外淡出去。殿里大部分地方只有环境光，人没有影子，看着像飘在地上。参数 Opacity。
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisBlobShadow", unreal.BlendMode.BLEND_TRANSLUCENT, unlit=False, two_sided=False, material_domain=unreal.MaterialDomain.MD_DEFERRED_DECAL)
    op = custom(mat, "float2 q = UV * 2.0 - 1.0;\nreturn pow(saturate(1.0 - length(q)), 1.3) * Opacity;",
        [("UV", node(mat, unreal.MaterialExpressionTextureCoordinate, -850, 0)), ("Opacity", scalar(mat, "Opacity", 0.5, -850, 120))], F1, -500, 0)
    mel.connect_material_property(node(mat, unreal.MaterialExpressionConstant3Vector, -500, -160, constant=unreal.LinearColor(0.0, 0.0, 0.0, 1.0)), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    finish(mat, full)

# ───────────────────────── 能挖一个方洞的墙 ─────────────────────────
if want("M_DysisWallCut"):
    # 和殿里的墙一个样（底色、粗糙度两个参数，照 M_MATCH_Palette），只是多一个“洞”：世界里一个方盒子范围以内的墙面不画。
    # 虹之龛用：墙是实心的一整个网格，没法真去挖；把龛口那一块墙面不画，后面另摆一个五面的小龛（DysisDirectorMechPack.cpp）
    # （2026-10-11 用户：虹之龛嵌进墙体了，但对应的墙体没有挖空）。
    # 参数：HoleCenter（洞口的中心）、HoleOut（从殿心朝外的方向，单位向量）、HoleHalf（顺着墙的半宽、进深的一半、半高，厘米；全 0 = 没有洞）
    mat, full = new_mat("/Game/Dysis/Fx", "M_DysisWallCut", unreal.BlendMode.BLEND_MASKED, unlit=False, two_sided=False)
    mel.connect_material_property(rgb_of(mat, vector(mat, "BaseColor", (0.72, 0.60, 0.44), -900, -300), -650, -300), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(mat, "Roughness", 0.92, -900, -120), "", unreal.MaterialProperty.MP_ROUGHNESS)
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -1100, 100)
    hc = rgb_of(mat, vector(mat, "HoleCenter", (0.0, 0.0, -1000000.0), -1100, 250), -850, 250)
    ho = rgb_of(mat, vector(mat, "HoleOut", (1.0, 0.0, 0.0), -1100, 450), -850, 450)
    hh = rgb_of(mat, vector(mat, "HoleHalf", (0.0, 0.0, 0.0), -1100, 650), -850, 650)
    # 第二个洞（Hole2…）：波吕丢刻斯的龛。龛是做在厚墙那个部件里的，可它有一大半在圆殿的墙面后面——墙面不挖开，像就只露出前面一小截
    hc2 = rgb_of(mat, vector(mat, "Hole2Center", (0.0, 0.0, -1000000.0), -1100, 850), -850, 850)
    ho2 = rgb_of(mat, vector(mat, "Hole2Out", (1.0, 0.0, 0.0), -1100, 1050), -850, 1050)
    hh2 = rgb_of(mat, vector(mat, "Hole2Half", (0.0, 0.0, 0.0), -1100, 1250), -850, 1250)
    c = custom(mat, """
float3 d = WP - HoleCenter;
float3 o = normalize(float3(HoleOut.x, HoleOut.y, 0.0));
float3 t = float3(-o.y, o.x, 0.0);
if (abs(dot(d, t)) < HoleHalf.x && abs(dot(d, o)) < HoleHalf.y && abs(d.z) < HoleHalf.z) return 0.0;
d = WP - Hole2Center;
o = normalize(float3(Hole2Out.x, Hole2Out.y, 0.0));
t = float3(-o.y, o.x, 0.0);
return (abs(dot(d, t)) < Hole2Half.x && abs(dot(d, o)) < Hole2Half.y && abs(d.z) < Hole2Half.z) ? 0.0 : 1.0;
""", [("WP", wp), ("HoleCenter", hc), ("HoleOut", ho), ("HoleHalf", hh), ("Hole2Center", hc2), ("Hole2Out", ho2), ("Hole2Half", hh2)], F1, -500, 300)
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    finish(mat, full)
