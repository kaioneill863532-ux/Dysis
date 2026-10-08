#!/usr/bin/env bash
# 一口气跑几个 PIE 测试（编辑器要开着、别在试玩）：bash Tools/tests/run_tests.sh pie_roof pie_beams …
# 每个测试跑完打印它的结论（没过的那几条、最后一句）；一个测试 9 分钟还没跑完就算卡住，接着跑下一个。
cd "$(dirname "$0")/../.." || exit 1
PY="${PYTHON:-D:/Python/python.exe}"
for t in "$@"; do
  rm -f "Saved/$t.txt"
  "$PY" Tools/ue_run.py "Tools/tests/$t.py" >/dev/null 2>&1
  n=0
  until grep -q "^DONE" "Saved/$t.txt" 2>/dev/null; do
    sleep 5; n=$((n + 5))
    if [ "$n" -ge 540 ]; then echo "== $t：9 分钟还没跑完（卡住了？）"; break; fi
  done
  ok=$(grep -c "^✔" "Saved/$t.txt" 2>/dev/null)
  echo "== $t：通过 $ok 条，用时约 ${n} 秒"
  grep -E "^✘|出错|全部通过|没过|Traceback" "Saved/$t.txt" 2>/dev/null | cut -c1-240
  sleep 3
done
