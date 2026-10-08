# 导入界面字体（UE 编辑器 Python，可以重复跑）：Art/UI/fonts/ 里的两个字重 → /Game/Dysis/UI/Fonts/ 下的 FontFace。
#   正文：阿里巴巴普惠体 3.0 45 Light      → PuHuiTi_Light
#   加粗（名牌、标题）：阿里巴巴普惠体 3.0 65 Medium → PuHuiTi_Medium
# 加载方式设成 Inline（字体数据存在资产里）。默认的 Lazy Load 在编辑器里读的是“导入的人电脑上的源文件路径”，换一台电脑就读不到字。
import unreal, os
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = PROJ + "Art/UI/fonts/"
DEST = "/Game/Dysis/UI/Fonts"
FILES = [("AlibabaPuHuiTi-3-45-Light.otf", "PuHuiTi_Light"), ("AlibabaPuHuiTi-3-65-Medium.otf", "PuHuiTi_Medium")]
tools = unreal.AssetToolsHelpers.get_asset_tools()
for fn, name in FILES:
    path = SRC + fn
    if not os.path.exists(path): print("找不到", path); continue
    t = unreal.AssetImportTask()
    t.filename = path; t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t])
    full = DEST + "/" + name
    face = unreal.load_asset(full)
    if not face or face.get_class().get_name() != "FontFace":
        print("导入没成功：", fn, face); continue
    face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
    print(name, face.get_class().get_name(), "保存：", unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False))
# 自动导入时引擎可能顺手建一个同名加 _Font 的复合字体资产，用不着，删掉
for fn, name in FILES:
    extra = DEST + "/" + name + "_Font"
    if unreal.EditorAssetLibrary.does_asset_exist(extra): unreal.EditorAssetLibrary.delete_asset(extra)
print([str(a) for a in unreal.EditorAssetLibrary.list_assets(DEST, recursive=False)])
