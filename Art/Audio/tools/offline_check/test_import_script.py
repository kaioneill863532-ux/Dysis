# 没有 UE 的机器上测试 ue_import_sfx.py：用 unreal.py（假的 unreal 模块）把脚本跑两遍并检查结果。
#   python3 Art/Audio/tools/offline_check/test_import_script.py
import os, sys, json, re, shutil, runpy, tempfile
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import unreal
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
proj = tempfile.mkdtemp(prefix="dysis_mock_")
os.symlink(os.path.join(REPO, "Art"), os.path.join(proj, "Art"))
unreal.STATE["project"] = proj
# 场景里先放一个“旧的”调度，看重跑能不能删掉
old = unreal._Actor(unreal.DysisSfxDirector); old.label = "DysisSfx"; old.tags = ["DysisSfx"]; unreal.STATE["actors"].append(old)

script = os.path.join(REPO, "Art", "Audio", "ue_import_sfx.py")
runpy.run_path(script, run_name="__main__")
S = unreal.STATE
man = json.load(open(os.path.join(REPO, "Art/Audio/manifest.json"), encoding="utf-8"))
files = [(f["path"].split("/")[1], f["name"], f["loop"]) for it in man["items"] for f in it["files"]]
assert len(S["tasks"]) == len(files) == 265, len(S["tasks"])
for (dp, dn, fn), (folder, name, loop) in zip(S["tasks"], files):
    assert dp == "/Game/Dysis/Audio/SFX/" + folder and dn == name and fn.endswith(f"/Art/Audio/SFX/{folder}/{name}.wav"), (dp, dn, fn)
loops = {f"/Game/Dysis/Audio/SFX/{fo}/{n}" for fo, n, lp in files if lp}
for p, a in S["assets"].items():
    assert a.props["looping"] == (p in loops), p
# C++ 默认表里的路径 == 导入的资产
cpp = open(os.path.join(REPO, "Source/Dysis/Audio/DysisSfxDefaults.cpp"), encoding="utf-8").read()
soft = set(re.findall(r'TEXT\("(/Game/[^"]+)"\)', cpp))
imported = {p + "." + p.split("/")[-1] for p in S["assets"]}
assert soft == imported, (len(soft), len(imported), list(soft ^ imported)[:5])
dirs = [a for a in S["actors"] if a.label == "DysisSfx"]
assert len(dirs) == 1 and dirs[0] is not old and dirs[0].folder == "Dysis_Audio" and "DysisSfx" in dirs[0].tags, [(a.label, a.folder, a.tags) for a in S["actors"]]
assert S["autobind"] == 1 and S["saved_maps"] == 1 and S["loaded_map"] == "/Game/Dysis/Maps/Dysis_Temple"
print("RUN1 OK: tasks", len(S["tasks"]), "loops", len(loops), "saved assets", S["saved_assets"], "soft paths match", len(soft))

# 第二次：全部跳过；调度仍只有一个
S["tasks"].clear(); n_actors_before = len(S["actors"])
runpy.run_path(script, run_name="__main__")
assert len(S["tasks"]) == 0, len(S["tasks"])
dirs = [a for a in S["actors"] if a.label == "DysisSfx"]
assert len(dirs) == 1 and S["autobind"] == 2 and S["saved_maps"] == 2
print("RUN2 OK: re-run skipped all imports, one director")
assert any("全部通过" in m for m in unreal.LOG), "final line"
shutil.rmtree(proj)
