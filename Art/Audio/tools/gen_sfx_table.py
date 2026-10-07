"""从 Art/Audio/manifest.json + sfx_events.py 生成 C++ 默认音效表 Source/Dysis/Audio/DysisSfxDefaults.cpp。

  python Art/Audio/tools/gen_sfx_table.py

换了 WAV、加了事件就重跑一次（然后编译）。生成前会检查：每个 WAV 都归到了某个事件、没有事件是空的。
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, HERE)
from sfx_events import EVENTS, DEFAULTS  # noqa: E402

GAME_DIR = "/Game/Dysis/Audio/SFX"
OUT = os.path.join(ROOT, "Source", "Dysis", "Audio", "DysisSfxDefaults.cpp")


def load_manifest():
    man = json.load(open(os.path.join(ROOT, "Art", "Audio", "manifest.json"), encoding="utf-8"))
    files = []   # (name, folder, loop)
    for it in man["items"]:
        for f in it["files"]:
            folder = f["path"].split("/")[1]
            files.append((f["name"], folder, bool(f["loop"])))
    return files


def resolve(files):
    used = {}
    out = []
    for ev in EVENTS:
        e = dict(DEFAULTS)
        e.update(ev)
        rx = re.compile(r"^" + e["files"] + r"$")
        hits = [(n, d, lp) for (n, d, lp) in files if rx.match(n)]
        if not hits:
            raise SystemExit(f"事件 {e['key']} 没匹配到任何 WAV（{e['files']}）")
        for n, d, lp in hits:
            if n in used:
                raise SystemExit(f"{n} 同时归到了 {used[n]} 和 {e['key']}")
            used[n] = e["key"]
            if lp != e["loop"]:
                raise SystemExit(f"{n}：manifest 里 loop={lp}，事件 {e['key']} loop={e['loop']}，对不上")
        e["paths"] = [f"{GAME_DIR}/{d}/{n}.{n}" for n, d, _ in hits]
        out.append(e)
    orphans = [n for n, _, _ in files if n not in used]
    if orphans:
        raise SystemExit("这些 WAV 没有归到任何事件：" + ", ".join(orphans))
    keys = [e["key"] for e in out]
    if len(keys) != len(set(keys)):
        raise SystemExit("事件名有重复")
    return out


def cstr(s):
    return 'TEXT("' + s.replace("\\", "\\\\").replace('"', '\\"') + '")'


def fnum(v):
    s = repr(float(v))
    return s + "f"


def emit(events):
    lines = [
        "﻿// 狄西斯的日落回廊 · 音效默认表（" + str(len(events)) + " 项、" + str(sum(len(e["paths"]) for e in events)) + " 个文件）",
        "// 由 Art/Audio/tools/gen_sfx_table.py 从 Art/Audio/manifest.json + sfx_events.py 生成，不要手改。",
        "// 运行时这些只是“出厂值”：Project Settings → Game → Dysis 音效 里改的音量、快慢等存在 Config/DefaultGame.ini，优先于这里。",
        '#include "Audio/DysisSfxSettings.h"',
        '#include "Sound/SoundBase.h"',
        "",
        "namespace DysisSfxDefaults",
        "{",
        "\tstatic FDysisSfxEvent Make(const TCHAR* Key, const TCHAR* Label, EDysisSfxCategory Category, bool b2D,",
        "\t\tfloat InnerCm, float FalloffCm, float Volume, float Pitch, float VolumeJitter, float PitchJitter,",
        "\t\tbool bNoRepeat, float Cooldown, int32 MaxInstances, bool bLoop, std::initializer_list<const TCHAR*> Paths)",
        "\t{",
        "\t\tFDysisSfxEvent E;",
        "\t\tE.Key = FName(Key);",
        "\t\tE.Label = Label;",
        "\t\tE.Category = Category;",
        "\t\tE.b2D = b2D;",
        "\t\tE.InnerRadiusCm = InnerCm;",
        "\t\tE.FalloffDistanceCm = FalloffCm;",
        "\t\tE.Volume = Volume;",
        "\t\tE.Pitch = Pitch;",
        "\t\tE.VolumeJitter = VolumeJitter;",
        "\t\tE.PitchJitter = PitchJitter;",
        "\t\tE.bNoRepeat = bNoRepeat;",
        "\t\tE.CooldownSeconds = Cooldown;",
        "\t\tE.MaxInstances = MaxInstances;",
        "\t\tE.bLoop = bLoop;",
        "\t\tfor (const TCHAR* P : Paths)",
        "\t\t{",
        "\t\t\tE.Sounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(P)));",
        "\t\t}",
        "\t\treturn E;",
        "\t}",
        "",
        "\tvoid Fill(TArray<FDysisSfxEvent>& Out)",
        "\t{",
        "\t\tOut.Reset();",
        "\t\tusing C = EDysisSfxCategory;",
    ]
    for e in events:
        paths = ", ".join(cstr(p) for p in e["paths"])
        lines.append(
            f"\t\tOut.Add(Make({cstr(e['key'])}, {cstr(e['label'])}, C::{e['cat']}, {'true' if e['d2'] else 'false'}, "
            f"{fnum(e['inner'])}, {fnum(e['falloff'])}, {fnum(e['vol'])}, {fnum(e['pitch'])}, {fnum(e['vj'])}, {fnum(e['pj'])}, "
            f"{'true' if e['norep'] else 'false'}, {fnum(e['cd'])}, {int(e['maxn'])}, {'true' if e['loop'] else 'false'},")
        lines.append(f"\t\t\t{{ {paths} }}));")
    lines += ["\t}", "}", ""]
    return "\n".join(lines)


def main():
    events = resolve(load_manifest())
    src = emit(events)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(src)
    print(f"写好 {os.path.relpath(OUT, ROOT)}：{len(events)} 项、{sum(len(e['paths']) for e in events)} 个文件")


if __name__ == "__main__":
    main()
