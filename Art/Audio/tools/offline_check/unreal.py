# 假的 unreal 模块：只实现 ue_import_sfx.py 用到的部分，记录调用，供 test_import_script.py 断言。
import os
LOG = []
STATE = dict(project=None, assets={}, tasks=[], actors=[], saved_assets=0, saved_maps=0, loaded_map=None, autobind=0,
             reverb_builds=0, saved_reverb=[])
REVERB = ("IR_Temple_Rotunda", "SubmixFX_TempleReverb", "Submix_TempleReverb")

def log_warning(m): LOG.append(m); print(m)
def log(m): LOG.append(m)
def Name(s): return str(s)

class Vector:
    def __init__(self, x=0.0, y=0.0, z=0.0): self.x, self.y, self.z = x, y, z
class Rotator:
    def __init__(self, a=0.0, b=0.0, c=0.0): pass

class Paths:
    @staticmethod
    def project_dir(): return STATE["project"]
    @staticmethod
    def convert_relative_path_to_full(p): return os.path.abspath(p) + "/"

class AssetImportTask:
    def __init__(self):
        self.filename = None; self.destination_path = None; self.destination_name = None
        self.automated = False; self.replace_existing = False; self.save = True

class _Asset:
    def __init__(self, path): self.path = path; self.props = {"looping": False}
    def get_editor_property(self, k): return self.props[k]
    def set_editor_property(self, k, v):
        if k not in self.props: raise Exception("no prop " + k)
        self.props[k] = v

class _Tools:
    def import_asset_tasks(self, tasks):
        for t in tasks:
            assert t.automated and t.replace_existing and os.path.exists(t.filename), t.filename
            STATE["tasks"].append((t.destination_path, t.destination_name, t.filename))
            STATE["assets"][t.destination_path + "/" + t.destination_name] = _Asset(t.destination_path + "/" + t.destination_name)

class AssetToolsHelpers:
    @staticmethod
    def get_asset_tools(): return _Tools()

def load_asset(path): return STATE["assets"].get(path)
def load_object(outer, path): return None

class DysisSfxLibrary:
    @staticmethod
    def build_temple_reverb(ir_wav_file):
        assert os.path.exists(ir_wav_file) and ir_wav_file.endswith("/Art/Audio/ir/IR_Temple_Rotunda.wav"), ir_wav_file
        with open(ir_wav_file, "rb") as fh:
            assert fh.read(4) == b"RIFF", "IR 不是 wav（LFS 指针？）"
        STATE["reverb_builds"] += 1
        for n in REVERB:
            STATE["assets"]["/Game/Dysis/Audio/Reverb/" + n] = _Asset("/Game/Dysis/Audio/Reverb/" + n)
        return "成功：/Game/Dysis/Audio/Reverb/Submix_TempleReverb.Submix_TempleReverb（mock）"

class EditorAssetLibrary:
    @staticmethod
    def save_asset(path, only_if_is_dirty=True):
        if path not in STATE["assets"]: return False
        STATE["saved_reverb"].append(path); return True
    @staticmethod
    def save_loaded_asset(asset, only_if_is_dirty=True):
        STATE["saved_assets"] += 1
        # 模拟 UE：保存后磁盘上出现 .uasset（第二次跑用它判断“没改过”）
        rel = asset.path[len("/Game/"):]
        dest = os.path.join(STATE["project"], "Content", rel + ".uasset")
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        open(dest, "w").write("x")
        return True

class _World:
    def __init__(self, path): self.path = path
    def get_path_name(self): return self.path

class EditorLevelLibrary:
    @staticmethod
    def get_editor_world(): return _World(STATE["loaded_map"] + "." + STATE["loaded_map"].split("/")[-1]) if STATE["loaded_map"] else None

class EditorLoadingAndSavingUtils:
    @staticmethod
    def load_map(m): STATE["loaded_map"] = m
    @staticmethod
    def save_dirty_packages(a, b): STATE["saved_maps"] += 1; return True

class _Actor:
    def __init__(self, cls): self.cls = cls; self.label = ""; self.folder = None; self.tags = []; self.alive = True
    def set_actor_label(self, l): self.label = l
    def get_actor_label(self): return self.label
    def set_folder_path(self, f): self.folder = f
    def destroy_actor(self): self.alive = False; STATE["actors"].remove(self)
    def auto_bind(self): STATE["autobind"] += 1
    def describe_bindings(self): return "DysisSfxDirector 绑定：锚点 15/15，机关音效 30 条\n  (mock)"

class DysisSfxDirector: pass
class EditorActorSubsystem: pass

class _ActorSub:
    def get_all_level_actors(self): return list(STATE["actors"])
    def spawn_actor_from_class(self, cls, loc, rot):
        a = _Actor(cls); STATE["actors"].append(a); return a

def get_editor_subsystem(cls): return _ActorSub()
