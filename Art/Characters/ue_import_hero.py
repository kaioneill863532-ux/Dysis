# 把主角导进工程（UE 编辑器 Python，可以重复跑）：蒙皮模型、18 段动作、四张贴图、一个材质。
# 原始文件是场景美术 2026-10-09 给的，放在 Art/Characters/Hero_20261009/（模型已缩放到身高 174 cm、24 帧/秒；
# 四张贴图扩展名是 .png、内容其实是 JPEG，引擎按内容认，不用改名）。
# 结果在 /Game/Dysis/Characters/Hero：SK_DysisHero（+ 骨架）、Animations/A_*、Textures/T_Character_*、M_DysisHero。
# 用法：python Tools/ue_run.py Art/Characters/ue_import_hero.py   结果写在 Saved/import_hero.txt
import unreal, os, glob
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
SRC = PROJ + "Art/Characters/Hero_20261009/"
DST = "/Game/Dysis/Characters/Hero"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
out = []
def say(s): out.append(str(s)); open(SAVED + "import_hero.txt", "w", encoding="utf-8").write("\n".join(out))

def run(filename, dest, name, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", filename); t.set_editor_property("destination_path", dest); t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True); t.set_editor_property("replace_existing", True); t.set_editor_property("save", True)
    if options is not None: t.set_editor_property("options", options)
    tools.import_asset_tasks([t])
    return [str(p) for p in t.get_editor_property("imported_object_paths")]

def fbx_ui():
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_materials", False); ui.set_editor_property("import_textures", False)
    ui.set_editor_property("automated_import_should_detect_type", False)
    return ui

try:
    # ── 蒙皮模型 ──
    ui = fbx_ui()
    ui.set_editor_property("import_mesh", True); ui.set_editor_property("import_as_skeletal", True); ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property("create_physics_asset", False)
    got = run(SRC + "SK_DysisHero.fbx", DST, "SK_DysisHero", ui)
    say("模型导入：%s" % got)
    mesh = unreal.load_asset(DST + "/SK_DysisHero")
    if not isinstance(mesh, unreal.SkeletalMesh):
        # 引擎的新导入器可能给资产起了别的名字：在目录里找那一个蒙皮模型
        for p in eal.list_assets(DST, recursive=False):
            a = unreal.load_asset(p)
            if isinstance(a, unreal.SkeletalMesh): mesh = a; break
    skeleton = mesh.get_editor_property("skeleton")
    say("模型 %s  骨架 %s" % (mesh.get_path_name(), skeleton.get_path_name()))
except Exception as e:
    say("模型导入出错：%r" % e); raise

# ── 动作 ──
anims = sorted(glob.glob(SRC + "A_*.fbx"))
for f in anims:
    name = os.path.splitext(os.path.basename(f))[0]
    ui = fbx_ui()
    ui.set_editor_property("import_mesh", False); ui.set_editor_property("import_as_skeletal", False); ui.set_editor_property("import_animations", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("skeleton", skeleton)
    got = run(f, DST + "/Animations", name, ui)
    seq = None
    for p in got:
        a = unreal.load_asset(p.split(".")[0])
        if isinstance(a, unreal.AnimSequence): seq = a; break
    if seq is None: say("动作 %s 没导出来：%s" % (name, got)); continue
    if seq.get_name() != name:     # 名字不是我们要的就改过来（代码按 A_xxx 找）
        eal.rename_asset(seq.get_path_name().split(".")[0], DST + "/Animations/" + name)
        seq = unreal.load_asset(DST + "/Animations/" + name)
    # 位移由人物的移动组件管，动作本身原地放（美术那边也是这么设的）
    seq.set_editor_property("enable_root_motion", False); seq.set_editor_property("force_root_lock", True)
    eal.save_asset(seq.get_path_name().split(".")[0], only_if_is_dirty=False)
    say("动作 %-26s %.3f 秒" % (name, seq.get_play_length()))

# ── 贴图 ──
TEX = {}
for kind, srgb, comp in (("BaseColor", True, unreal.TextureCompressionSettings.TC_DEFAULT), ("Normal", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
                         ("Roughness", False, unreal.TextureCompressionSettings.TC_MASKS), ("Metallic", False, unreal.TextureCompressionSettings.TC_MASKS)):
    name = "T_Character_" + kind
    run(SRC + name + ".png", DST + "/Textures", name)
    tex = unreal.load_asset(DST + "/Textures/" + name)
    if tex is None: say("贴图 %s 没导进来" % name); continue
    tex.set_editor_property("srgb", srgb); tex.set_editor_property("compression_settings", comp)
    eal.save_asset(DST + "/Textures/" + name, only_if_is_dirty=False)
    TEX[kind] = tex
    say("贴图 %-24s %d × %d" % (name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))

# ── 材质：四张贴图各接各的口 ──
full = DST + "/M_DysisHero"
mat = unreal.load_asset(full) if eal.does_asset_exist(full) else tools.create_asset("M_DysisHero", DST, unreal.Material, unreal.MaterialFactoryNew())
for _ in range(24):      # 引擎的 delete_all_material_expressions 一次只删一半，删到一个不剩
    if mel.get_num_material_expressions(mat) == 0: break
    mel.delete_all_material_expressions(mat)
mat.set_editor_property("two_sided", True)        # 头发、裙摆是单层的面，背面也要画
def sample(kind, sampler, y):
    n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, y)
    n.set_editor_property("texture", TEX[kind]); n.set_editor_property("sampler_type", sampler); return n
ST = unreal.MaterialSamplerType
if "BaseColor" in TEX: mel.connect_material_property(sample("BaseColor", ST.SAMPLERTYPE_COLOR, -300), "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
if "Normal" in TEX: mel.connect_material_property(sample("Normal", ST.SAMPLERTYPE_NORMAL, -40), "RGB", unreal.MaterialProperty.MP_NORMAL)
if "Roughness" in TEX: mel.connect_material_property(sample("Roughness", ST.SAMPLERTYPE_MASKS, 220), "R", unreal.MaterialProperty.MP_ROUGHNESS)
if "Metallic" in TEX: mel.connect_material_property(sample("Metallic", ST.SAMPLERTYPE_MASKS, 480), "R", unreal.MaterialProperty.MP_METALLIC)
mel.recompile_material(mat)
eal.save_asset(full, only_if_is_dirty=False)

# ── 把材质挂到模型的每个槽上（美术那边第一次就是漏了这一步，人是灰的） ──
# 注意：从数组里取出来的每一格是一份拷贝，改完要拼成新的一组再整个写回去（直接在原数组上改不会生效）
slots = []
for m in mesh.get_editor_property("materials"):
    m.set_editor_property("material_interface", mat); slots.append(m)
mesh.set_editor_property("materials", slots)
eal.save_asset(mesh.get_path_name().split(".")[0], only_if_is_dirty=False)
bound = [m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None for m in mesh.get_editor_property("materials")]
b = mesh.get_bounds()
say("材质槽 %d 个，回读：%s；模型高约 %.0f cm（包围盒 %.0f × %.0f × %.0f）" % (len(bound), bound, b.box_extent.z * 2, b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2))
if any(x != "M_DysisHero" for x in bound): say("！！材质没挂上")
say("DONE")
