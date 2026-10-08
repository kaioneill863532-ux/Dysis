# 关掉编辑器（有没保存的改动就先存）。用法：powershell -File Tools\quit_editor.ps1 [-NoSave]
param([switch]$NoSave)
$py = if ($env:DYSIS_PYTHON) { $env:DYSIS_PYTHON } else { "D:\Python\python.exe" }
if (-not (Get-Process UnrealEditor -ErrorAction SilentlyContinue)) { "编辑器没开"; exit 0 }
$save = if ($NoSave) { "False" } else { "True" }
& $py -X utf8 "$PSScriptRoot\ue_run.py" -c "import unreal`nd = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()`nprint('未保存:', [p.get_name() for p in d])`nif d and ${save}: print('保存:', unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True))`nunreal.SystemLibrary.quit_editor()" 2>$null | Select-String "未保存|保存:" | ForEach-Object { $_.Line }
for ($i = 0; $i -lt 90; $i++) { if (-not (Get-Process UnrealEditor -ErrorAction SilentlyContinue)) { "编辑器已关闭"; exit 0 }; Start-Sleep -Seconds 1 }
"编辑器 90 秒内没有退出"; exit 3