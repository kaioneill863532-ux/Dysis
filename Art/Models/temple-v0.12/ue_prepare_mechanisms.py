# 给“机关总管”（ADysisDirector）准备关卡（UE 编辑器 Python，可以重复跑；跑完存关卡）：
#   1. 每个模型部件（标签以 SM_ 开头）带上一个和标签一样的 Tag——运行时没有标签，总管靠 Tag 找部件。
#   2. L1 的两个机关合成一个：留下西南那个（A），挪到两处沿回廊走的正中间（方位 328.5°，贴外墙），B 的部件删掉。
#   3. 已经由总管接手的旧玩法 Actor 删掉（水闸拉杆、机关 A / B、两块石板的滑动器、瀑布驱动器、屋顶踏步、接光、金苹果、
#      三相像的镜光源和日之龛、虹那条支线的对齐判定 / 棱镜 / 窗台和铜柱的驱动器 / 虹之龛），免得两套逻辑打架。
import unreal, math
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = list(sub.get_all_level_actors())
by_label = {a.get_actor_label(): a for a in actors}

# 1 Tag
n_tag = 0
for a in actors:
    lab = a.get_actor_label()
    if not lab.startswith("SM_"): continue
    if lab not in [str(t) for t in a.tags]:
        a.modify(); tags = list(a.tags); tags.append(lab); a.set_editor_property("tags", tags); n_tag += 1
print("新打 Tag：", n_tag)

# 2 机关合并
NEW_AZ = 328.5
moved = 0
for lab in ("SM_Mech_LeverA_Base", "SM_Mech_LeverA_Arm", "SM_Mech_LeverA_Chain"):
    a = by_label.get(lab)
    if not a: print("找不到", lab); continue
    l = a.get_actor_location(); az = math.degrees(math.atan2(l.y, l.x)) % 360
    d = NEW_AZ - az
    if abs(d) < 0.01: continue
    a.modify()
    a.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD)
    r = math.hypot(l.x, l.y); rot = a.get_actor_rotation()
    a.set_actor_location(unreal.Vector(r * math.cos(math.radians(NEW_AZ)), r * math.sin(math.radians(NEW_AZ)), l.z), False, False)
    a.set_actor_rotation(unreal.Rotator(roll=rot.roll, pitch=rot.pitch, yaw=rot.yaw + d), False)
    moved += 1
print("机关 A 的部件挪了：", moved)

# 3 删旧的
gone = []
for lab in ("SM_Mech_LeverB_Base", "SM_Mech_LeverB_Arm", "SM_Mech_LeverB_Chain",
            "DysisLeverA", "DysisLeverB", "DysisLeverSluice", "DysisSlider_b2", "DysisSlider_iris", "Mech_Waterfall",
            "DysisRoofSteps", "DysisCatchLight", "DysisGoldenApple",
            "DysisMirrorSource", "Niche_Sun", "Mech_SunNicheLid",
            "DysisRainbowAlign", "DysisPrism", "Mech_PrismColumn", "Mech_SillLedge", "Niche_Rainbow",
            "Mech_SwanDoor", "Mech_SwanGoddess", "Mech_SwanRelief", "Mech_MoonRelief", "Mech_StairSeal_TS", "Mech_StairSeal_TR",
            "Mech_Win_TS1", "Mech_Win_TS2", "Mech_Win_TS3", "Mech_Win_TR1", "Mech_Win_TR2"):
    a = by_label.get(lab)
    if not a: continue
    for ch in a.get_attached_actors():      # 挂在它下面的模型部件留着
        ch.modify(); ch.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD)
    sub.destroy_actor(a); gone.append(lab)
print("删掉：", gone)
print("存关卡：", unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
