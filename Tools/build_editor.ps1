# 编译 DysisEditor（编辑器必须先关）。用法：powershell -File Tools\build_editor.ps1
# 带 -DisableAdaptiveUnity：不管文件改没改，都按“全新拉取下来”的方式合并编译。
# （默认引擎会把你正在改的文件单独编译、别的合并编译——改着的时候能过，提交以后别人拉下来一合并，
#   几个 .cpp 里同名的小函数 / 常数就撞了。这样编，这类问题当场就能看到。）
# 编之前先删掉引擎缓存的文件清单（Intermediate\Build 下的 Makefile.bin），让它每次都重新扫一遍源文件：
#   这份缓存有时认不出新加的 .cpp，结果链接时报“无法解析的外部符号”。
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { "D:\UE5\UE_5.8" }
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) { "编辑器还开着，先关掉再编译"; exit 2 }
$log = Join-Path $root "Saved\build_editor.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
Get-ChildItem (Join-Path $root "Intermediate\Build") -Recurse -Filter "Makefile*.bin" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
$sw = [Diagnostics.Stopwatch]::StartNew()
& "$ue\Engine\Build\BatchFiles\Build.bat" DysisEditor Win64 Development "-Project=$root\Dysis.uproject" -WaitMutex -NoHotReloadFromIDE -DisableAdaptiveUnity *> $log
$code = $LASTEXITCODE
Select-String -Path $log -Pattern "error [A-Z]+[0-9]+|warning C[0-9]+|Result:" | ForEach-Object { $l = $_.Line -replace '^.*\\Source\\Dysis\\', ''; $l.Substring(0, [Math]::Min(260, $l.Length)) } | Select-Object -Unique -First 40
"exit=$code  用时 {0:N0} 秒  （完整日志 Saved\build_editor.log）" -f $sw.Elapsed.TotalSeconds
exit $code