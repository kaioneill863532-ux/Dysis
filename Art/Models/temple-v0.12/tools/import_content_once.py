# 一次性内容导入运行器（完整编辑器 -ExecutePythonScript 用，跑完自动退出保存）：
#   1. 按官方 README 跑 bgm及导入代码/DysisMusic/ue_import_music.py
#      （4 首导入 /Game/Dysis/Audio/Music、开 Looping、挂 SC_Dysis_Music、管理器填曲）
#   2. 删除旧的两个不带循环的 SoundWave（/Game/Dysis/Audio/M_Day_Sicilienne_Long、M_Night_ClairDeLune_Full）
#   3. 导入 UI 图片（使用UI/主界面与设置→Menu、游戏内UI→InGame、立绘→Portraits、演示图→References/Mockups）
#   4. 保存全部脏包（含关卡——管理器属性被官方脚本改过）
import unreal, os, runpy

ROOT = os.path.normpath(unreal.Paths.project_dir())   # 工程根（.uproject 所在目录），引擎自己给，别手算层级

def say(msg):
    unreal.log_warning("[ContentImport] " + str(msg))

# ── 0) 先加载 Dysis_Temple（commandlet 里没有已开关卡；官方脚本要找/填关卡里的音乐管理器）──
MAP = "/Game/Dysis/Maps/Dysis_Temple"
need_load = True
try:
    w = unreal.EditorLevelLibrary.get_editor_world()
    if w and str(w.get_path_name()).startswith(MAP + "."):
        need_load = False
except Exception:
    pass
if need_load:
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    say("已加载关卡 %s" % MAP)

# ── 1) 官方音乐导入脚本（runpy 保 __file__，脚本内部定位自己的 Audio/ 目录）──
music_py = os.path.join(ROOT, "bgm及导入代码", "DysisMusic", "ue_import_music.py")
try:
    runpy.run_path(music_py, run_name="__music_import__")
    say("官方 ue_import_music.py 执行完成")
except Exception as e:
    say("官方音乐脚本异常：%s" % e)

# ── 2) 删旧的不带循环的 BGM 资产 ──
eal = unreal.EditorAssetLibrary
for name in ("M_Day_Sicilienne_Long", "M_Night_ClairDeLune_Full"):
    path = "/Game/Dysis/Audio/" + name
    if eal.does_asset_exist(path):
        ok = eal.delete_asset(path)
        say("删除旧资产 %s：%s" % (path, "成功" if ok else "失败"))

# ── 3) UI 图片导入（中文文件名 → ASCII 资产名）──
def import_png(src_rel, dest_path, dest_name):
    src = os.path.join(ROOT, src_rel)
    if not os.path.isfile(src):
        say("缺文件：%s" % src_rel)
        return
    if eal.does_asset_exist(dest_path + "/" + dest_name):
        return   # 幂等：已导入跳过
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest_path
    task.destination_name = dest_name
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    say("导入 %s → %s/%s %s" % (src_rel, dest_path, dest_name,
        "✔" if eal.does_asset_exist(dest_path + "/" + dest_name) else "✘"))

MENU = "/Game/Dysis/UI/Menu"
INGAME = "/Game/Dysis/UI/InGame"
PORTRAITS = "/Game/Dysis/UI/Portraits"
MOCKUPS = "/Game/Dysis/UI/References/Mockups"
for name, dest in (("开始游戏", "StartGame"), ("退出游戏", "QuitGame"), ("设置", "Settings"),
                   ("返回游戏", "BackToGame"), ("返回主界面", "BackToMenu"),
                   ("主界面选择标", "SelectorMain"), ("设置选择标", "SelectorSettings")):
    import_png("使用UI/主界面与设置/%s.png" % name, MENU, dest)
for name, dest in (("文本框", "TextBox"), ("反馈与提示", "Notify"), ("收藏品树枝", "Vine"),
                   ("太阳碎片", "SunShard"), ("彩虹碎片", "RainbowShard"), ("月亮碎片", "MoonShard"),
                   ("Dysis", "Dysis"), ("Iris", "Iris"), ("设置", "SettingsInGame")):
    import_png("使用UI/游戏内UI/%s.png" % name, INGAME, dest)
for name, dest in (("赫利俄斯", "Helios"), ("狄西斯", "Dysis"), ("塞勒涅", "Selene"), ("伊莉丝", "Iris")):
    import_png("局内UI示意图及各元素摆放位置/%s.png" % name, PORTRAITS, dest)
for name, dest in (("赫利俄斯-局内UI示意图", "Helios"), ("狄西斯-局内UI示意图", "Dysis"),
                   ("塞勒涅-局内UI示意图", "Selene"), ("伊莉丝-局内UI示意图", "Iris")):
    import_png("局内UI示意图及各元素摆放位置/%s.png" % name, MOCKUPS, dest)
for name, dest in (("主界面示意", "MainMenu"), ("设置示意", "Settings"),
                   ("UI演示图伊莉丝款", "InGameIris"), ("UI示意图狄西斯款", "InGameDysis")):
    import_png("使用UI/演示图/%s.png" % name, MOCKUPS, dest)

# ── 4) 保存 ──
saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
say("保存脏包：%s" % ("成功" if saved else "失败"))
say("内容导入完成")
