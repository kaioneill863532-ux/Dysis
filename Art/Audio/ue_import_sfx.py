# 狄西斯的日落回廊 · 音效一键导入 + 接入
#
# 做四件事（可以重跑，重跑安全）：
#   1) 把 Art/Audio/SFX/<编号_名字>/*.wav（265 个）导入成 /Game/Dysis/Audio/SFX/<编号_名字>/ 下的 SoundWave，
#      循环素材（环境、掉落中、捧着金苹果……）设成 Looping。已经导入、WAV 也没改过的跳过。
#   2) 殿内混响：用 Art/Audio/ir/IR_Temple_Rotunda.wav（和预览网页同一份圆殿冲激响应）生成
#      /Game/Dysis/Audio/Reverb 下的冲激响应、卷积混响效果、混响 Submix（要 Synthesis 插件，Dysis.uproject 里已经打开）。
#   3) 打开 Dysis_Temple，摆一个音效总调度 DysisSfxDirector（大纲：Dysis_Audio/DysisSfx），
#      按大纲名字自动绑定：瀑布/水池/桥门/塞勒涅浮雕……这些位置，和 21 个机关驱动/石板各自的音效。
#   4) 保存，并检查：每一项音效的文件都在、Looping 对、混响生成了。
#
# 用法 A（编辑器里）：先编译（C++ 里有新的音效代码），打开工程 → 工具 → 执行 Python 脚本…，选这个文件。
# 用法 B（无头命令行）：
#   UnrealEditor-Cmd Dysis.uproject -run=pythonscript -script="<本文件的绝对路径>"
#   （命令行会自己触发编译；脚本自己加载并保存 Dysis_Temple。）
#
# 重跑 ue_import_gameplay.py（它会删掉重摆机关 Actor）以后，再跑一次本脚本，把绑定指到新的机关上；
# 忘了也没关系：DysisSfxDirector 开局发现绑定的机关不在了，会自己按名字重新找一遍。
#
# 调音量、快慢、混响：Project Settings → Game → Dysis 音效（改动存在 Config/DefaultGame.ini）。
import json
import os

import unreal

MAP = "/Game/Dysis/Maps/Dysis_Temple"
GAME_DIR = "/Game/Dysis/Audio/SFX"
LABEL = "DysisSfx"
TAG = "DysisSfx"
FOLDER = "Dysis_Audio"
REIMPORT_ALL = False   # True = 不管有没有改过，全部重新导入
REVERB_DIR = "/Game/Dysis/Audio/Reverb"
REVERB_ASSETS = ("IR_Temple_Rotunda", "SubmixFX_TempleReverb", "Submix_TempleReverb")
REVERB_SUBMIX = REVERB_DIR + "/Submix_TempleReverb"


def say(msg):
    # Warning 级才会出现在无头命令行的 stdout 里（和 verify_gameplay.py 一样）。
    unreal.log_warning("[DysisSfx] " + str(msg))


def project_dir():
    return unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())


def load_manifest():
    path = os.path.join(project_dir(), "Art", "Audio", "manifest.json")
    if not os.path.exists(path):
        raise RuntimeError("找不到 %s（Art/Audio 没拉下来？LFS 有没有 pull？）" % path)
    with open(path, encoding="utf-8") as fh:
        man = json.load(fh)
    files = []
    for item in man["items"]:
        for f in item["files"]:
            folder = f["path"].split("/")[1]
            src = os.path.join(project_dir(), "Art", "Audio", "SFX", folder, f["name"] + ".wav")
            files.append(dict(name=f["name"], folder=folder, src=src, loop=bool(f["loop"]), num=item["num"]))
    return files


def uasset_path(folder, name):
    return os.path.join(project_dir(), "Content", "Dysis", "Audio", "SFX", folder, name + ".uasset")


def is_lfs_pointer(path):
    try:
        with open(path, "rb") as fh:
            return fh.read(40).startswith(b"version https://git-lfs")
    except OSError:
        return False


# ───────────────────────── 1) 导入 ─────────────────────────

def import_sounds(files):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    tasks, todo, skipped, bad = [], [], 0, []
    for f in files:
        if not os.path.exists(f["src"]):
            bad.append("%s：WAV 不存在（%s）" % (f["name"], f["src"]))
            continue
        if is_lfs_pointer(f["src"]):
            bad.append("%s：是 Git LFS 指针，不是声音（先 git lfs pull）" % f["name"])
            continue
        dest = uasset_path(f["folder"], f["name"])
        if not REIMPORT_ALL and os.path.exists(dest) and os.path.getmtime(dest) >= os.path.getmtime(f["src"]):
            skipped += 1
            continue
        t = unreal.AssetImportTask()
        t.filename = f["src"]
        t.destination_path = GAME_DIR + "/" + f["folder"]
        t.destination_name = f["name"]
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
        todo.append(f)
    if tasks:
        say("导入 %d 个 WAV（跳过 %d 个没改过的）……" % (len(tasks), skipped))
        tools.import_asset_tasks(tasks)
    else:
        say("WAV 都导入过了（%d 个没改过，跳过）" % skipped)
    return todo, skipped, bad


def fix_looping_and_save(files, imported):
    """按 manifest 设 Looping 并保存。

    这次新导入的：一定保存。没重新导入的（WAV 没改过）：Looping 和 manifest 不一样才改、才存
    （有人误改了，或 manifest 改了哪些是循环），其余不动，免得每次重跑都把 265 个 .uasset 重存一遍。
    返回 (保存了几个, 其中修好的旧资产几个, 问题列表)。
    """
    new = {(f["folder"], f["name"]) for f in imported}
    saved, fixed, problems = 0, 0, []
    for f in files:
        path = "%s/%s/%s" % (GAME_DIR, f["folder"], f["name"])
        is_new = (f["folder"], f["name"]) in new
        asset = unreal.load_asset(path)
        if asset is None:
            if is_new:
                problems.append("%s：导入以后还是加载不到" % path)
            continue   # 没导入过的由 check() 报“缺”
        changed = False
        try:
            if bool(asset.get_editor_property("looping")) != f["loop"]:
                asset.set_editor_property("looping", f["loop"])
                changed = True
        except Exception as e:  # noqa: BLE001
            problems.append("%s：设 Looping 失败（%s）" % (path, e))
        if not (is_new or changed):
            continue
        if unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
            saved += 1
            fixed += 0 if is_new else 1
        else:
            problems.append("%s：保存失败" % path)
    return saved, fixed, problems


# ───────────────────────── 2) 殿内混响 ─────────────────────────

def build_reverb():
    """生成殿内卷积混响（冲激响应 → 卷积混响效果 → Submix）并保存。返回 None = 成功，否则是原因。"""
    ir = os.path.join(project_dir(), "Art", "Audio", "ir", "IR_Temple_Rotunda.wav")
    if not os.path.exists(ir):
        return "找不到 %s" % ir
    if is_lfs_pointer(ir):
        return "%s 是 Git LFS 指针，不是声音（先 git lfs pull）" % ir
    lib = getattr(unreal, "DysisSfxLibrary", None)
    if lib is None or not hasattr(lib, "build_temple_reverb"):
        return "找不到 DysisSfxLibrary.build_temple_reverb——先编译工程（Source/Dysis/Audio/DysisSfxReverb.cpp）"
    result = str(lib.build_temple_reverb(ir))
    say("混响：" + result)
    if not result.startswith("成功"):
        return result
    for name in REVERB_ASSETS:
        path = REVERB_DIR + "/" + name
        if not unreal.EditorAssetLibrary.save_asset(path, False):
            return "保存 %s 失败" % path
    return None


# ───────────────────────── 3) 关卡里的总调度 ─────────────────────────

def all_actors():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return sub.get_all_level_actors() if sub else unreal.EditorLevelLibrary.get_all_level_actors()


def load_class(name):
    cls = getattr(unreal, name, None)
    if cls is None:
        cls = unreal.load_object(None, "/Script/Dysis." + name)
    if cls is None:
        raise RuntimeError("找不到 C++ 类 Dysis.%s——先编译工程（音效代码在 Source/Dysis/Audio）" % name)
    return cls


def place_director():
    world = None
    try:
        world = unreal.EditorLevelLibrary.get_editor_world()
    except Exception:  # noqa: BLE001
        world = None
    if world is None or not str(world.get_path_name()).startswith(MAP + "."):
        unreal.EditorLoadingAndSavingUtils.load_map(MAP)
        world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("打不开关卡 %s" % MAP)

    # 旧的删掉（重跑安全）。
    removed = 0
    for a in list(all_actors()):
        try:
            if TAG in [str(t) for t in a.tags] or a.get_actor_label() == LABEL:
                a.destroy_actor()
                removed += 1
        except Exception:  # noqa: BLE001
            continue
    if removed:
        say("删掉旧的 DysisSfx %d 个" % removed)

    cls = load_class("DysisSfxDirector")
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    loc, rot = unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0)
    director = sub.spawn_actor_from_class(cls, loc, rot) if sub else unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    director.set_actor_label(LABEL)
    try:
        director.set_folder_path(unreal.Name(FOLDER))
    except Exception:  # noqa: BLE001
        pass
    director.tags = list(director.tags) + [unreal.Name(TAG)]

    director.auto_bind()
    report = director.describe_bindings()
    for line in str(report).splitlines():
        say(line)

    saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    say("保存关卡：%s" % ("成功" if saved else "失败"))
    return director


# ───────────────────────── 4) 检查 ─────────────────────────

def check(files):
    missing, wrong_loop = [], []
    for f in files:
        path = "%s/%s/%s" % (GAME_DIR, f["folder"], f["name"])
        asset = unreal.load_asset(path)
        if asset is None:
            missing.append(path)
            continue
        try:
            if bool(asset.get_editor_property("looping")) != f["loop"]:
                wrong_loop.append(path)
        except Exception:  # noqa: BLE001
            wrong_loop.append(path)
    return missing, wrong_loop


def run():
    files = load_manifest()
    say("manifest：%d 个音效文件" % len(files))
    todo, skipped, bad = import_sounds(files)
    for b in bad:
        say("  ✗ " + b)
    saved, fixed, problems = fix_looping_and_save(files, todo)
    if fixed:
        say("Looping 和 manifest 不一样的旧资产：改好并保存了 %d 个" % fixed)
    for p in problems:
        say("  ✗ " + p)
    reverb_problem = build_reverb()
    if reverb_problem:
        say("  ✗ 混响没生成：%s（音效本身不受影响；见 Art/Audio/README.md 的“混响”一节）" % reverb_problem)
    place_director()
    missing, wrong_loop = check(files)
    reverb_ok = unreal.load_asset(REVERB_SUBMIX) is not None
    say("检查：%d 个文件，缺 %d 个，Looping 不对 %d 个；殿内混响%s" % (len(files), len(missing), len(wrong_loop),
                                                         "已生成" if reverb_ok else "没有"))
    for m in missing[:20]:
        say("  缺 " + m)
    for w in wrong_loop[:20]:
        say("  Looping 不对 " + w)
    ok = not bad and not problems and not missing and not wrong_loop and reverb_ok
    say("完成：%s。下一步：PIE 里按 ~ 输入 Dysis.Sfx.Debug 1，走一走、拉一拉机关，屏幕左上角会显示每次播了哪一项；"
        "调音量/快慢/混响在 Project Settings → Game → Dysis 音效（Dysis.Sfx.Reverb 看混响状态）。" % ("全部通过" if ok else "有问题，见上面"))
    return ok


run()
