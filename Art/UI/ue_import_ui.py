# 狄西斯的日落回廊 · 导入界面素材（UE 编辑器 Python；可重跑）
#
# 把 Art/UI/source/ 下的按钮、立绘、文本框等导入成 /Game/Dysis/UI/{Menu,InGame,Portraits} 的贴图，
# 统一设成 UI 用的设置（不生成 mip、不流送、UI 压缩）。示意图不导入（只在 Art/UI/source 里留着对照）。
# 用法：编辑器里 工具 → 执行 Python 脚本…，选这个文件；或 ue_run.py 发进编辑器。
import unreal, os

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in dir() else "."
SRC = os.path.join(HERE, "source")
ROOT = "/Game/Dysis/UI"
# (源文件, 目标文件夹, 资产名, 最大边长 0=不限)
ITEMS = [
    ("主界面与设置/Logo.png",        "Menu", "Logo", 0),
    ("主界面与设置/开始游戏.png",     "Menu", "StartGame", 0),
    ("主界面与设置/退出游戏.png",     "Menu", "QuitGame", 0),
    ("主界面与设置/主界面选择标.png", "Menu", "SelectorMain", 0),
    ("主界面与设置/设置.png",         "Menu", "Settings", 0),
    ("主界面与设置/返回游戏.png",     "Menu", "BackToGame", 0),
    ("主界面与设置/返回主界面.png",   "Menu", "BackToMenu", 0),
    ("主界面与设置/设置选择标.png",   "Menu", "SelectorSettings", 0),
    ("游戏内UI/设置.png",             "InGame", "SettingsInGame", 0),
    ("游戏内UI/收藏品树枝.png",       "InGame", "Vine", 0),
    ("游戏内UI/太阳碎片.png",         "InGame", "SunShard", 0),
    ("游戏内UI/彩虹碎片.png",         "InGame", "RainbowShard", 0),
    ("游戏内UI/月亮碎片.png",         "InGame", "MoonShard", 0),
    ("游戏内UI/反馈与提示.png",       "InGame", "Notify", 4096),
    ("游戏内UI/文本框.png",           "InGame", "TextBox", 4096),
    ("游戏内UI/Dysis.png",            "Portraits", "Dysis", 0),
    ("游戏内UI/Iris.png",             "Portraits", "Iris", 0),
    ("游戏内UI/塞勒涅.png",           "Portraits", "Selene", 0),
    ("游戏内UI/赫利俄斯.png",         "Portraits", "Helios", 0),
    # 关卡名（罗马数字）：美术给的是整屏 1920×1080 的透明图（数字在 (960, 460)），这里放的是裁出来的数字周围 256×256 那一块
    ("关卡名/I.png",                  "InGame", "LevelI", 0),
    ("关卡名/II.png",                 "InGame", "LevelII", 0),
    ("关卡名/III.png",                "InGame", "LevelIII", 0),
    ("关卡名/IV.png",                 "InGame", "LevelIV", 0),
    ("关卡名/V.png",                  "InGame", "LevelV", 0),
]
# 不再用的旧资产（示意图的副本、重复的立绘）
STALE = [ROOT + "/References", ROOT + "/InGame/Dysis", ROOT + "/InGame/Iris"]

def say(s): unreal.log("[DysisUI] " + s); print("[DysisUI] " + s)

tools = unreal.AssetToolsHelpers.get_asset_tools()
ok = bad = 0
for rel, folder, name, max_size in ITEMS:
    src = os.path.join(SRC, rel).replace("\\", "/")
    if not os.path.isfile(src): say("✗ 找不到 " + src); bad += 1; continue
    t = unreal.AssetImportTask()
    t.filename = src; t.destination_path = ROOT + "/" + folder; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t])
    tex = unreal.load_asset("%s/%s/%s" % (ROOT, folder, name))
    if not isinstance(tex, unreal.Texture2D): say("✗ 没导成贴图：" + rel); bad += 1; continue
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)   # UserInterface2D
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("never_stream", True)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("max_texture_size", int(max_size))
    try:
        tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP); tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    except Exception: pass
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
    ok += 1
for p in STALE:
    try:
        if unreal.EditorAssetLibrary.does_directory_exist(p): unreal.EditorAssetLibrary.delete_directory(p); say("删掉旧目录 " + p)
        elif unreal.EditorAssetLibrary.does_asset_exist(p): unreal.EditorAssetLibrary.delete_asset(p); say("删掉旧资产 " + p)
    except Exception as e: say("没删成 %s：%r" % (p, e))
say("导入 %d 张，失败 %d 张" % (ok, bad))
say("完成：" + ("全部通过" if bad == 0 else "有问题，见上面"))