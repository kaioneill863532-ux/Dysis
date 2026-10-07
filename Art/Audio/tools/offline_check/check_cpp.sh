#!/bin/bash
# 没有 UE 的机器上，对音效代码做一次 clang 语法/类型检查（音效代码 + 它引用的工程真实头文件）。
#   bash Art/Audio/tools/offline_check/check_cpp.sh            # 检查音效相关的全部 .cpp
#   bash Art/Audio/tools/offline_check/check_cpp.sh Audio/DysisSfxDirector.cpp
# 做法：把 Source/Dysis 复制到临时目录，给 GENERATED_BODY() 补上 Super/ThisClass（UHT 做的事），
# 引擎头文件全部指向 FakeUE.h（按 UE 5.x 的签名写的最小假实现），再 clang++ -fsyntax-only，警告当错误
# （-Wshadow、double→float 等，和 Mac clang / MSVC 上容易出事的一致）。
# 注意：这只能证明“按这些签名能编译”，不能代替在 UE 里真正编译——引擎 API 以 UE 为准。
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../../.." && pwd)
SRC="$REPO/Source/Dysis"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cp -r "$SRC"/. "$TMP/src/" 2>/dev/null || { mkdir -p "$TMP/src"; cp -r "$SRC"/. "$TMP/src/"; }
mkdir -p "$TMP/inc" && cp "$HERE/FakeUE.h" "$TMP/inc/"
FILES=${@:-Audio/DysisSfxSettings.cpp Audio/DysisSfxDefaults.cpp Audio/DysisSfxSubsystem.cpp Audio/DysisFootstepComponent.cpp Audio/DysisSfxDirector.cpp Audio/DysisSfxTests.cpp UI/DysisHUD.cpp}
python3 - "$TMP/src" "$TMP/inc" $FILES <<'PY'
import os, re, sys
src, inc, files = sys.argv[1], sys.argv[2], sys.argv[3:]
rx = re.compile(r'(class|struct)\s+(?:DYSIS_API\s+)?(\w+)\s*(?:final\s*)?:\s*public\s+(\w+)[^{]*\{(\s*)GENERATED_BODY\(\)')
seen = set()
def walk(p):
    if p in seen: return
    seen.add(p)
    s = open(p, encoding="utf-8-sig").read()
    s2 = rx.sub(lambda m: m.group(0) + " typedef %s Super; typedef %s ThisClass;" % (m.group(3), m.group(2)), s)
    if s2 != s: open(p, "w", encoding="utf-8").write(s2)
    for i in re.findall(r'#include\s+"([^"]+)"', s):
        local = [os.path.join(src, i), os.path.join(os.path.dirname(p), i)]
        hit = [x for x in local if os.path.exists(x)]
        if hit and not i.endswith(".generated.h"):
            walk(hit[0]); continue
        stub = os.path.join(inc, i)
        os.makedirs(os.path.dirname(stub), exist_ok=True)
        if not os.path.exists(stub):
            open(stub, "w").write("#pragma once\n" + ("" if i.endswith(".generated.h") else '#include "FakeUE.h"\n'))
for f in files: walk(os.path.join(src, f))
PY
rc=0
for f in $FILES; do
  out=$(clang++ -std=c++20 -fsyntax-only -isystem "$TMP/inc" -I "$TMP/src" -Wall -Wextra -Wshadow -Wfloat-conversion \
        -Wimplicit-float-conversion -Wno-implicit-int-float-conversion -Wshorten-64-to-32 -Wsign-compare \
        -Wno-unused-parameter -Wno-unused-private-field -Wno-missing-field-initializers -Werror "$TMP/src/$f" 2>&1) || true
  if [ -n "$out" ]; then echo "✗ $f"; echo "$out" | grep -E "error|warning" | sed "s|$TMP/src/||" | head -30; rc=1; else echo "✓ $f"; fi
done
exit $rc
