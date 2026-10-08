# 标出“不挡光”的部件（UE 编辑器 Python，可以重复跑；跑完存关卡）。
# 灰盒里每个部件有四个开关：能踩 w、挡人 s、挡光 l、挡镜头 c。导进 UE 以后所有带碰撞的部件默认四样全挡，
# 但灰盒里有些是不挡光的：回廊内沿的栏杆（“栏杆不算挡光，但画出来的影子是有的”）、外立面的装饰（窗框、线脚、壁柱……只挡镜头）、日之龛的小匣子。
# 光路是用“可见性”这一路射线算的，所以把这些部件对“可见性”射线设成忽略；别的（挡人、挡镜头）不变。
import unreal, collections
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
VIS = unreal.CollisionChannel.ECC_VISIBILITY
IGN = unreal.CollisionResponseType.ECR_IGNORE
def no_light(folder):
    f = str(folder)
    part = f.split("/")[-1] if "/" in f else f
    # 03 栏杆；05、05a、05b、05c…… 外立面装饰；11.05 日之龛（灰盒里只是摆在石台上的模型，没有碰撞，镜光是穿过它照到墙上的）
    return part.startswith("03 ") or part.startswith("05") or part.startswith("11.05 ")
seen = collections.Counter(); changed = 0
for a in sub.get_all_level_actors():
    if a.get_class().get_name() != "StaticMeshActor": continue
    folder = str(a.get_folder_path())
    seen[folder.split("/")[-1]] += 1
    if not no_light(folder): continue
    c = a.static_mesh_component
    # 注意：部件默认“用网格资产自带的碰撞设置”（use_default_collision），那样运行时（PIE、打包后）会把这里改的响应冲掉，
    # 所以先关掉它，再设成“全挡、只对可见性射线忽略”。
    if c.get_editor_property("use_default_collision") or c.get_collision_response_to_channel(VIS) != IGN:
        a.modify(); c.modify()
        c.set_editor_property("use_default_collision", False)
        c.set_collision_profile_name("BlockAll")
        c.set_collision_response_to_channel(VIS, IGN)
        changed += 1
for k in sorted(seen): print("%4d  %s%s" % (seen[k], k, "   ← 不挡光" if no_light(k) else ""))
print("改了", changed, "个；存关卡：", unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
