# 打开编辑器到神殿关卡，并等它能接收远程 Python（Tools\ue_run.py）。已经开着就只等它就绪。
# -Fast：这次会话里关掉“处于后台时占用较少 CPU”（否则编辑器不在前台时 PIE 只有约 3 帧每秒，自动测试会很慢）。
param([switch]$Fast, [string]$Map = "/Game/Dysis/Maps/Dysis_Temple")
$root = Split-Path -Parent $PSScriptRoot
$ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { "D:\UE5\UE_5.8" }
$py = if ($env:DYSIS_PYTHON) { $env:DYSIS_PYTHON } else { "D:\Python\python.exe" }
if (Get-Process UnrealEditor-Cmd -ErrorAction SilentlyContinue) { "有无头的 UnrealEditor-Cmd 在跑，同一个工程不能同时开两个"; exit 2 }
if (-not (Get-Process UnrealEditor -ErrorAction SilentlyContinue)) {
  $a = '"' + "$root\Dysis.uproject" + '" ' + $Map
  if ($Fast) { $a += ' "-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False"' }
  Start-Process -FilePath "$ue\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $a
  "已启动编辑器"
} else { "编辑器已经开着" }
for ($i = 0; $i -lt 80; $i++) {
  $out = & $py -X utf8 "$PSScriptRoot\ue_run.py" -c "import unreal; print('EDITOR_READY', unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name())" 2>$null | Select-String "EDITOR_READY"
  if ($out) { $out.Line; exit 0 }
  Start-Sleep -Seconds 4
}
"编辑器 5 分钟内没有应答"; exit 3