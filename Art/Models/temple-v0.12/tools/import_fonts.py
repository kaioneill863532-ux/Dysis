# 把 Content/Dysis/UI/Fonts 下的思源宋体 .otf 正式导入成 UFontFace 资产（无头可跑）
# 和 ue_import_gameplay.py 的 BGM 导入同一套路：裸 .otf 不是资产包，必须 AssetImportTask 导入。
# 用法（无头）：
#   UnrealEditor-Cmd Dysis.uproject -run=pythonscript -script=本文件 -stdout -unattended -nosplash
# 幂等：已是资产就跳过。
import unreal

FONTS_DIR = "/Game/Dysis/UI/Fonts"

def say(msg):
    unreal.log_warning("[Fonts] " + str(msg))   # Warning 级才进 stdout

def load_or_import(name):
    path = FONTS_DIR + "/" + name
    asset = unreal.load_asset(path)
    if asset:
        say("%s 已是资产：%s" % (name, asset.get_class().get_name()))
        return asset
    src = unreal.Paths.project_content_dir() + "Dysis/UI/Fonts/" + name + ".otf"
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = FONTS_DIR
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(path)
    say("%s 导入%s：%s" % (name, "成功" if asset else "失败", asset))
    return asset

ok = True
for name in ("SourceHanSerifSC-Regular", "SourceHanSerifSC-Bold"):
    if not load_or_import(name):
        ok = False

if ok:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    say("保存完成")
else:
    say("有失败项，不保存")
