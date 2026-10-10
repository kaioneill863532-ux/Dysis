# 主角的贴图和材质（UE 编辑器 Python，可以重复跑；ue_import_hero.py 最后会跑它，只改材质时也可以单独跑）。
#   · 美术的四张贴图各接各的口；
#   · 头发的摆动：模型没有头发的骨头，所以在材质里挪顶点——按一张遮罩（哪些面是头发、越往发梢越白，
#     由 Art/Characters/make_hair_mask.py 从模型和底色贴图里认出来）把头发挪一点：
#       HairLag    发梢被甩出去多少（世界坐标，厘米；人物代码里的一根弹簧每帧给，见 ADysisCharacter::UpdateHair）
#       HairSwayA  左右飘的方向和幅度      HairSwayB  前后、上下飘的方向和幅度（同上，每帧给）
#     飘是两个不同快慢的波叠在一起，从发根往发梢传；三个量都是 0 时头发就是美术做的原样。
# 用法：python Tools/ue_run.py Art/Characters/ue_make_hero_material.py   结果写在 Saved/hero_material.txt
import unreal, os
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
SRC = PROJ + "Art/Characters/Hero_20261009/"
DST = "/Game/Dysis/Characters/Hero"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
_out = []
def _say(s): _out.append(str(s)); open(SAVED + "hero_material.txt", "w", encoding="utf-8").write("\n".join(_out))
def _import(filename, dest, name):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename); t.set_editor_property("destination_path", dest); t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", True)
    tools.import_asset_tasks([t])
def _node(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): e.set_editor_property(k, v)
    return e

try:
    # ── 贴图 ──
    TC = unreal.TextureCompressionSettings
    TEX = {}
    for kind, path, srgb, comp in (("BaseColor", SRC + "T_Character_BaseColor.png", True, TC.TC_DEFAULT), ("Normal", SRC + "T_Character_Normal.png", False, TC.TC_NORMALMAP),
                                   ("Roughness", SRC + "T_Character_Roughness.png", False, TC.TC_MASKS), ("Metallic", SRC + "T_Character_Metallic.png", False, TC.TC_MASKS),
                                   ("HairMask", PROJ + "Art/Characters/T_DysisHero_HairMask.png", False, TC.TC_MASKS)):
        name = "T_DysisHero_HairMask" if kind == "HairMask" else "T_Character_" + kind
        # 美术的四张贴图（4096 的大图）已经在工程里就不重导——重导一遍内容一样，却会让仓库里多出几十兆的改动。
        # 美术换了贴图：先删掉工程里那一张再跑，或者跑 ue_import_hero.py 之前把 Saved/hero_reimport_textures.flag 建出来。头发遮罩是我们自己生成的小图，每次都重导
        fresh = kind == "HairMask" or not eal.does_asset_exist(DST + "/Textures/" + name) or os.path.exists(SAVED + "hero_reimport_textures.flag")
        if fresh: _import(path, DST + "/Textures", name)
        tex = unreal.load_asset(DST + "/Textures/" + name)
        if tex is None: _say("贴图 %s 没导进来" % name); continue
        if fresh:
            tex.set_editor_property("srgb", srgb); tex.set_editor_property("compression_settings", comp)
            eal.save_asset(DST + "/Textures/" + name, only_if_is_dirty=False)
        TEX[kind] = tex
        _say("贴图 %-24s %d × %d%s" % (name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(), "" if fresh else "（已有，没重导）"))

    # ── 材质 ──
    full = DST + "/M_DysisHero"
    mat = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset("M_DysisHero", DST, unreal.Material, unreal.MaterialFactoryNew())
    for _ in range(24):      # 引擎的 delete_all_material_expressions 一次只删一半，删到一个不剩
        if mel.get_num_material_expressions(mat) == 0: break
        mel.delete_all_material_expressions(mat)
    mat.set_editor_property("two_sided", True)        # 头发、裙摆是单层的面，背面也要画
    mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    ST = unreal.MaterialSamplerType
    def sample(kind, sampler, y, **props): return _node(mat, unreal.MaterialExpressionTextureSample, -420, y, texture=TEX[kind], sampler_type=sampler, **props)
    if "BaseColor" in TEX: mel.connect_material_property(sample("BaseColor", ST.SAMPLERTYPE_COLOR, -300), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if "Normal" in TEX: mel.connect_material_property(sample("Normal", ST.SAMPLERTYPE_NORMAL, -40), "RGB", unreal.MaterialProperty.MP_NORMAL)
    if "Roughness" in TEX: mel.connect_material_property(sample("Roughness", ST.SAMPLERTYPE_MASKS, 220), "R", unreal.MaterialProperty.MP_ROUGHNESS)
    if "Metallic" in TEX: mel.connect_material_property(sample("Metallic", ST.SAMPLERTYPE_MASKS, 480), "R", unreal.MaterialProperty.MP_METALLIC)
    if "HairMask" in TEX:
        # 挪顶点是在“顶点”那一步算的：贴图要指明取哪一级（第 0 级），位置用蒙皮之前的（人摆什么姿势都不影响波的相位）
        mask = sample("HairMask", ST.SAMPLERTYPE_MASKS, 760, mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL, const_mip_value=0)
        pre = _node(mat, unreal.MaterialExpressionPreSkinnedPosition, -900, 900)
        time = _node(mat, unreal.MaterialExpressionTime, -900, 1000)
        def vec(name, y): return _node(mat, unreal.MaterialExpressionVectorParameter, -900, y, parameter_name=name, default_value=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
        lag, a, b = vec("HairLag", 1100), vec("HairSwayA", 1300), vec("HairSwayB", 1500)
        c = _node(mat, unreal.MaterialExpressionCustom, -420, 1000, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, code="""
float d = max(150.0 - pre.z, 0.0);                 // 离发根多远（发根在 150 cm 高）
float ph = d * 0.075 + pre.x * 0.045;              // 波从发根往发梢传；左右两边错开一点
float s1 = sin(t * 1.9 - ph) * 0.65 + sin(t * 3.1 - ph * 1.7 + 1.3) * 0.35;
float s2 = sin(t * 1.3 - ph * 0.8 + 2.1) * 0.6 + sin(t * 2.6 - ph * 1.3) * 0.4;
return w * (lag.xyz * w + a.xyz * s1 + b.xyz * s2);
""")
        ins = []
        for n in ("w", "pre", "t", "lag", "a", "b"):
            ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
        c.set_editor_property("inputs", ins)
        mel.connect_material_expressions(mask, "R", c, "w")
        for n, e in (("pre", pre), ("t", time), ("lag", lag), ("a", a), ("b", b)): mel.connect_material_expressions(e, "", c, n)
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(mat)
    eal.save_asset(full, only_if_is_dirty=False)
    _say("材质 M_DysisHero：%d 个节点%s" % (mel.get_num_material_expressions(mat), "，带头发的摆动" if "HairMask" in TEX else "（没有头发遮罩：头发不摆）"))

    # ── 把材质挂到模型的每个槽上（美术那边第一次就是漏了这一步，人是灰的） ──
    # 注意：从数组里取出来的每一格是一份拷贝，改完要拼成新的一组再整个写回去（直接在原数组上改不会生效）
    mesh = unreal.load_asset(DST + "/SK_DysisHero")
    slots = []
    for m in mesh.get_editor_property("materials"):
        m.set_editor_property("material_interface", mat); slots.append(m)
    mesh.set_editor_property("materials", slots)
    eal.save_asset(mesh.get_path_name().split(".")[0], only_if_is_dirty=False)
    bound = [m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None for m in mesh.get_editor_property("materials")]
    _say("材质槽 %d 个，回读：%s" % (len(bound), bound))
    if any(x != "M_DysisHero" for x in bound): _say("！！材质没挂上")
except Exception:
    import traceback
    _say("ERR " + traceback.format_exc())
_say("DONE")
