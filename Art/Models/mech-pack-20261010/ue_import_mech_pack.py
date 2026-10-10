# 把 convert_mech_pack.py 转好的机关模型导进工程（UE 编辑器 Python，可以重复跑）。
# 来源：项目/Saved/pack8_out/*.glb（美术 2026-10-10 的《打包.zip》转出来的，不进仓库；重做见 convert_mech_pack.py 开头）。
# 结果：/Game/Dysis/MechPack_20261010/SM_MP_<件名>（没有碰撞、不占光照贴图）和两个材质：
#   M_DysisMechPack —— 颜色全在顶点色里。没什么颜色的（石头、石膏）一律换成墙的那种暖白，只留一点原来的明暗；有颜色的（金边、镜面、彩虹）照原样；
#   M_DysisMechGlass —— 棱镜的水晶。
# 用法：python Tools/ue_run.py Art/Models/mech-pack-20261010/ue_import_mech_pack.py   结果写在 Saved/import_mech_pack.txt
# 想只导其中几件：把件名写在 Saved/mech_only.txt（逗号隔开）。
import unreal, os, glob, traceback
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
SRC = SAVED + "pack8_out/"
DST = "/Game/Dysis/MechPack_20261010"
TMP = DST + "/_import"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
ONLY = [x.strip() for x in open(SAVED + "mech_only.txt").read().split(",") if x.strip()] if os.path.exists(SAVED + "mech_only.txt") else []
out = []
def say(s): out.append(str(s)); open(SAVED + "import_mech_pack.txt", "w", encoding="utf-8").write("\n".join(out))

def node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): e.set_editor_property(k, v)
    return e
def fresh(name, **props):
    full = DST + "/" + name
    mat = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset(name, DST, unreal.Material, unreal.MaterialFactoryNew())
    for k, v in props.items(): mat.set_editor_property(k, v)
    for _ in range(24):      # 引擎一次只删掉一半节点：删到一个不剩为止
        if mel.get_num_material_expressions(mat) == 0: break
        mel.delete_all_material_expressions(mat)
    return mat, full

def clip_below(mat, wp):
    # ClipZ：世界里这个高度以下的部分不画（默认 −100 万 = 全画）。棱镜从石沿里升起来的时候用：还在石沿面以下的那一截不画，
    # 不然石沿只有 35 cm 厚，底下会露出一截往上钻的台座（2026-10-10 用户：棱柱升起来的时候穿模了）。
    cz = node(mat, unreal.MaterialExpressionScalarParameter, -900, 1250, parameter_name="ClipZ", default_value=-1000000.0)
    c = node(mat, unreal.MaterialExpressionCustom, -500, 1250, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, code="return WP.z >= ClipZ ? 1.0 : 0.0;")
    ins = []
    for n in ("WP", "ClipZ"):
        x = unreal.CustomInput(); x.set_editor_property("input_name", n); ins.append(x)
    c.set_editor_property("inputs", ins)
    mel.connect_material_expressions(wp, "", c, "WP"); mel.connect_material_expressions(cz, "", c, "ClipZ")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_OPACITY_MASK)

def make_materials():
    F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    mat, full = fresh("M_DysisMechPack", blend_mode=unreal.BlendMode.BLEND_MASKED, shading_model=unreal.MaterialShadingModel.MSM_DEFAULT_LIT, two_sided=False)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -900, -100)
    def sp(n, v, y): return node(mat, unreal.MaterialExpressionScalarParameter, -900, y, parameter_name=n, default_value=v)
    def vp(n, c, y): return node(mat, unreal.MaterialExpressionVectorParameter, -900, y, parameter_name=n, default_value=unreal.LinearColor(c[0], c[1], c[2], 1.0))
    # Stone = 墙的暖白（MI_MATCH_WarmWhite 的底色）；SatLo–SatHi = 颜色鲜到什么程度才算“点缀”、不换成石头色；
    # Detail = 石头上留多少原来贴图的明暗（0 = 一点不留，纯色）；LumRef = 原来的石头大约多亮（比它暗的地方会压暗）；
    # AccentGain = 点缀的颜色整体乘多少；VCGamma = 顶点色要不要再做一次解码（导入时量出来定的）
    ins = [("VC", vc), ("Stone", vp("Stone", (0.72, 0.60, 0.44), 60)), ("SatLo", sp("SatLo", 0.22, 160)), ("SatHi", sp("SatHi", 0.42, 240)), ("Detail", sp("Detail", 0.2, 320)),
           ("LumRef", sp("LumRef", 0.42, 400)), ("AccentGain", sp("AccentGain", 1.0, 480)), ("VCGamma", sp("VCGamma", 1.0, 560)), ("StoneMix", sp("StoneMix", 1.0, 640)), ("AccentShow", sp("AccentShow", 1.0, 720))]
    c = node(mat, unreal.MaterialExpressionCustom, -500, 100, output_type=F3, code="""
float3 c = pow(max(VC.rgb, 0.0), VCGamma);
float mx = max(c.r, max(c.g, c.b)), mn = min(c.r, min(c.g, c.b));
float sat = (mx - mn) / max(mx, 1e-4);
float lum = dot(c, float3(0.3, 0.59, 0.11));
float isStone = (1.0 - smoothstep(SatLo, SatHi, sat)) * StoneMix;
isStone = lerp(1.0, isStone, saturate(AccentShow));      // AccentShow：点缀的颜色显出来多少（0 = 全是石头色；伊莉丝浮雕上的虹平时是石头色，影子对上了才显出颜色）
float shade = lerp(1.0, clamp(lum / max(LumRef, 1e-3), 0.55, 1.15), Detail);
return lerp(c * AccentGain, Stone.rgb * shade, isStone);
""")
    ci = []
    for n, _ in ins:
        x = unreal.CustomInput(); x.set_editor_property("input_name", n); ci.append(x)
    c.set_editor_property("inputs", ci)
    for n, e in ins: mel.connect_material_expressions(e, "", c, n)
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(sp("Roughness", 0.92, 760), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # 一块一块的平面（和场景里别的低模一个样）：法线不用模型自带的，用屏幕上相邻像素的世界位置差现算——每个三角面一个法线
    mat.set_editor_property("tangent_space_normal", False)
    wp = node(mat, unreal.MaterialExpressionWorldPosition, -900, 900)
    # Facet：棱角有多硬。1 = 纯按面（每个三角面一个法线），0 = 用模型自己的平滑法线；中间是两样掺着——面还分得出来，但不那么碎
    vn = node(mat, unreal.MaterialExpressionVertexNormalWS, -900, 1000)
    fc = sp("Facet", 1.0, 1100)
    fn = node(mat, unreal.MaterialExpressionCustom, -500, 900, output_type=F3, code="float3 f = normalize(cross(ddx(WP), ddy(WP))); return normalize(lerp(normalize(VN), f, saturate(Facet)));")      # cross 的这个顺序法线才朝外（反过来整件东西是黑的）
    fi = []
    for n in ("WP", "VN", "Facet"):
        x = unreal.CustomInput(); x.set_editor_property("input_name", n); fi.append(x)
    fn.set_editor_property("inputs", fi)
    mel.connect_material_expressions(wp, "", fn, "WP"); mel.connect_material_expressions(vn, "", fn, "VN"); mel.connect_material_expressions(fc, "", fn, "Facet")
    mel.connect_material_property(fn, "", unreal.MaterialProperty.MP_NORMAL)
    clip_below(mat, wp)
    mel.recompile_material(mat); say("M_DysisMechPack 存了：%s" % eal.save_asset(full, only_if_is_dirty=False))

    g, gfull = fresh("M_DysisMechGlass", blend_mode=unreal.BlendMode.BLEND_MASKED, shading_model=unreal.MaterialShadingModel.MSM_DEFAULT_LIT, two_sided=True)
    clip_below(g, node(g, unreal.MaterialExpressionWorldPosition, -900, 900))
    col = node(g, unreal.MaterialExpressionVectorParameter, -500, 0, parameter_name="Color", default_value=unreal.LinearColor(0.80, 0.90, 1.0, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(g, unreal.MaterialExpressionScalarParameter, -500, 200, parameter_name="Roughness", default_value=0.08), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # 自己带一点亮：按“屏幕上多亮”写（除掉当时的曝光），白天夜里一个样
    glow = node(g, unreal.MaterialExpressionScalarParameter, -700, 360, parameter_name="Glow", default_value=0.22)
    mul = node(g, unreal.MaterialExpressionMultiply, -480, 340); mel.connect_material_expressions(col, "", mul, "A"); mel.connect_material_expressions(glow, "", mul, "B")
    ea = node(g, unreal.MaterialExpressionEyeAdaptation, -480, 480); div = node(g, unreal.MaterialExpressionDivide, -280, 380)
    mel.connect_material_expressions(mul, "", div, "A"); mel.connect_material_expressions(ea, "", div, "B")
    mel.connect_material_property(div, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(g); say("M_DysisMechGlass 存了：%s" % eal.save_asset(gfull, only_if_is_dirty=False))

# 各件降到多少个三角面（引擎自带的减面：保住大块平面的造型）。没列的不减——那几个低模本来就只有几百到几千面。
# 2026-10-10 用户：几万面太细、和场景里别的东西不搭，要降下来、和场景的低模风格一致。
# 三相像、双子、三块浮雕不在这张表里：它们在 convert_mech_pack.py 里已经按“保住原有大平面”的办法减到位了（引擎按面数硬减会把棱线削成碎块，用户看了说不行）。
# （后来外立面神像、神龛、拱门也改成在转换时减了：用户嫌拱门、日之龛“面数多、碎”。）
REDUCE = {"SwanBird": 6000, "SwanPlinth": 2400}      # 瀑布后女神也改成转换时减了（怀里的圆盘、底座要做成正圆）

# 试不同的档：Saved/mech_reduce.txt 里写 “件名=面数,件名=面数”，盖过上面的表
if os.path.exists(SAVED + "mech_reduce.txt"):
    for kv in open(SAVED + "mech_reduce.txt").read().split(","):
        if "=" in kv: REDUCE[kv.split("=")[0].strip()] = int(kv.split("=")[1])

def import_one(path):
    name = os.path.basename(path)[:-4]
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path); t.set_editor_property("destination_path", TMP + "/" + name); t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", False)
    tools.import_asset_tasks([t])
    got = [str(p) for p in t.get_editor_property("imported_object_paths")]
    meshes = [p for p in eal.list_assets(TMP + "/" + name, recursive=True) if isinstance(unreal.load_asset(p.split(".")[0]), unreal.StaticMesh)]
    if not meshes: say("✘ %s：没导出网格（%s）" % (name, got)); return None
    final = DST + "/SM_MP_" + name
    if eal.does_asset_exist(final): eal.delete_asset(final)
    if not eal.rename_asset(meshes[0].split(".")[0], final): say("✘ %s：改名失败" % name); return None
    mesh = unreal.load_asset(final)
    mat = unreal.load_asset(DST + ("/M_DysisMechGlass" if name == "PrismCrystal" else "/M_DysisMechPack"))
    mesh.set_material(0, mat)
    # 不用 Nanite（导入默认给开了：开着的话“普通的那一份网格”会被自动减掉四分之三的面，不支持 Nanite 的地方画出来就是糊的；这些件每个才几万面，用不着）
    ns = mesh.get_editor_property("nanite_settings"); ns.set_editor_property("enabled", False); mesh.set_editor_property("nanite_settings", ns)
    # 只管看：不要碰撞；不生成光照贴图 UV（没有贴图 UV，生成不出来还会报警告）
    body = mesh.get_editor_property("body_setup")
    if body: body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
    try:
        bs = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_lod_build_settings(mesh, 0)
        bs.set_editor_property("generate_lightmap_u_vs", False); bs.set_editor_property("recompute_normals", False); bs.set_editor_property("recompute_tangents", True)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_lod_build_settings(mesh, 0, bs)
    except Exception as e: say("   （%s 的构建设置没改成：%s）" % (name, e))
    full = mesh.get_num_triangles(0)
    if name in REDUCE and full > REDUCE[name]:
        sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        try:
            rs = sub.get_lod_reduction_settings(mesh, 0)
            rs.set_editor_property("percent_triangles", float(REDUCE[name]) / float(full))
            sub.set_lod_reduction_settings(mesh, 0, rs)
        except Exception as e: say("   （%s 减面没做成：%s）" % (name, e))
    eal.save_asset(final, only_if_is_dirty=False)
    b = mesh.get_bounding_box()
    tri = mesh.get_num_triangles(0)
    say("✔ SM_MP_%-17s 三角 %6d  X %.1f…%.1f  Y %.1f…%.1f  Z %.1f…%.1f" % (name, tri, b.min.x, b.max.x, b.min.y, b.max.y, b.min.z, b.max.z))
    return mesh

try:
    make_materials()
    files = sorted(glob.glob(SRC + "*.glb"))
    for f in files:
        if ONLY and os.path.basename(f)[:-4] not in ONLY: continue
        import_one(f)
    if eal.does_directory_exist(TMP): eal.delete_directory(TMP)
    # 顶点色导进来以后是什么样：拿金苹果看（转换时的平均色 #a87329，线性大约是 0.39, 0.17, 0.02）
    say("DONE")
except Exception:
    say("出错: " + traceback.format_exc()); say("DONE")
