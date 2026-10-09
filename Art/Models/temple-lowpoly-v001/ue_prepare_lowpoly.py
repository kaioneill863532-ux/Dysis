# 低模资产拷进工程以后跑一遍（UE 编辑器 Python，可以重复跑）：
# 场景美术 2026-10-08 的低模（/Game/Dysis/LowPoly_v001/Meshes/LP_*，215 个）在他那边的工程里，每个网格的“复杂碰撞”
# 指着另一套高模目录下的碰撞代理（/Game/Dysis/Highpoly_v001/CollisionProxies/UC_*）。我们这边低模只管看、不管碰撞
# （碰撞还是原模型的，见 Source/Dysis/Mechanisms/DysisDirectorSkin.cpp），那套代理没拷过来，所以把这个指向清掉，
# 免得每次打开都报“依赖的包找不到”。顺便按我们的引擎版本重新存一遍。
# 用法：python Tools/ue_run.py Art/Models/temple-lowpoly-v001/ue_prepare_lowpoly.py   结果写在 Saved/prepare_lowpoly.txt
import unreal
SAVED = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
eal = unreal.EditorAssetLibrary
n = cleared = 0
for p in eal.list_assets("/Game/Dysis/LowPoly_v001/Meshes", recursive=False):
    path = p.split(".")[0]
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh): continue
    n += 1
    if mesh.get_editor_property("complex_collision_mesh") is not None: cleared += 1
    mesh.set_editor_property("complex_collision_mesh", None)
    eal.save_asset(path, only_if_is_dirty=False)
for p in eal.list_assets("/Game/Dysis/LowPoly_v001/Materials", recursive=False):
    eal.save_asset(p.split(".")[0], only_if_is_dirty=False)
open(SAVED + "prepare_lowpoly.txt", "w", encoding="utf-8").write("低模网格 %d 个，清掉了 %d 个指向别处的复杂碰撞，已重新保存\nDONE" % (n, cleared))
