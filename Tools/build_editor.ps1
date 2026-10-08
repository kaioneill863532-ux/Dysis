# 编译 DysisEditor（编辑器必须先关）。用法：powershell -File Tools\build_editor.ps1
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { "D:\UE5\UE_5.8" }
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) { "编辑器还开着，先关掉再编译"; exit 2 }
$log = Join-Path $root "Saved\build_editor.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
$sw = [Diagnostics.Stopwatch]::StartNew()
& "$ue\Engine\Build\BatchFiles\Build.bat" DysisEditor Win64 Development "-Project=$root\Dysis.uproject" -WaitMutex -NoHotReloadFromIDE *> $log
$code = $LASTEXITCODE
Select-String -Path $log -Pattern "error [A-Z]+[0-9]+|warning C[0-9]+|Result:" | ForEach-Object { $l = $_.Line -replace '^.*\\Source\\Dysis\\', ''; $l.Substring(0, [Math]::Min(260, $l.Length)) } | Select-Object -Unique -First 40
"exit=$code  用时 {0:N0} 秒  （完整日志 Saved\build_editor.log）" -f $sw.Elapsed.TotalSeconds
exit $code