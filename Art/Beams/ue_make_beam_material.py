# 建光束的材质 /Game/Dysis/Beams/M_DysisBeam（UE 编辑器 Python，可以重复跑）：
# 不受光、叠加发光、双面；参数 Color（颜色）、Intensity（代码按“窗被照亮多少、能不能踩”写）、Brightness（整体亮度，美术调）。
# 现在只是一根发光的长条顶着，等美术的体积光。
import unreal
PATH, NAME = "/Game/Dysis/Beams", "M_DysisBeam"
full = PATH + "/" + NAME
mel = unreal.MaterialEditingLibrary
if unreal.EditorAssetLibrary.does_asset_exist(full):
    mat = unreal.load_asset(full)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
mel.delete_all_material_expressions(mat)
col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 0)
col.set_editor_property("parameter_name", "Color"); col.set_editor_property("default_value", unreal.LinearColor(1.0, 0.82, 0.55, 1.0))
inten = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 220)
inten.set_editor_property("parameter_name", "Intensity"); inten.set_editor_property("default_value", 1.0)
bright = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 340)
bright.set_editor_property("parameter_name", "Brightness"); bright.set_editor_property("default_value", 0.11)
m1 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -420, 80)
m2 = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 160)
mel.connect_material_expressions(col, "", m1, "A"); mel.connect_material_expressions(inten, "", m1, "B")
mel.connect_material_expressions(m1, "", m2, "A"); mel.connect_material_expressions(bright, "", m2, "B")
mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
print("M_DysisBeam saved:", unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False))
