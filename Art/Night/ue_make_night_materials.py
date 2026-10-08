# 建夜里要用的材质（UE 编辑器 Python，可以重复跑）：
#   /Game/Dysis/Night/M_DysisShadowBridge  影桥：水面上的一片影子。不受光、半透明、双面；
#       参数 Color（没接上时是暗影的颜色，接上以后是月白色）、Opacity、Glow（接上以后亮多少）。
# 现在只是占位的样子，等美术的影桥。
import unreal
PATH, NAME = "/Game/Dysis/Night", "M_DysisShadowBridge"
full = PATH + "/" + NAME
mel = unreal.MaterialEditingLibrary
mat = unreal.load_asset(full) if unreal.EditorAssetLibrary.does_asset_exist(full) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mel.delete_all_material_expressions(mat)
col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 0)
col.set_editor_property("parameter_name", "Color"); col.set_editor_property("default_value", unreal.LinearColor(0.03, 0.04, 0.07, 1.0))
glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 220)
glow.set_editor_property("parameter_name", "Glow"); glow.set_editor_property("default_value", 0.0)
base = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -700, 330)
base.set_editor_property("r", 0.2)
add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -480, 260)
mel.connect_material_expressions(glow, "", add, "A"); mel.connect_material_expressions(base, "", add, "B")
em = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -260, 80)
mel.connect_material_expressions(col, "", em, "A"); mel.connect_material_expressions(add, "", em, "B")
op = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 460)
op.set_editor_property("parameter_name", "Opacity"); op.set_editor_property("default_value", 0.55)
mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
print(NAME, "saved:", unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False))
