# 打包导出 Windows 版（给玩家的正式版：Shipping）。编辑器必须先关。
# 用法：powershell -File Tools\package_game.ps1 [-Out E:\Projects\Dysis_Export]
# 出来的东西在 <Out>\Windows\ 下：Dysis.exe 双击就能玩（整个 Windows 文件夹一起拷走 / 压缩发给别人）。
#   · 游戏图标：Build\Windows\Application.ico（原图在 Art\UI\source\游戏图标\）。
#   · 正式版里没有控制台、没有屏幕上的调试字（引擎自己去掉），F3 的调试信息条和 M 键换海面也没有（DysisHUD.cpp 里包在 !UE_BUILD_SHIPPING 里）。
#   · 编辑器里用的工具插件（ModelContextProtocol、AllToolsets、MCPClientToolset）只给编辑器（Dysis.uproject 的 TargetAllowList），不会编进游戏。
#   · 代码里按路径载入的资产（音效、材质、字体、模型…）靠 Config\DefaultGame.ini 里的 DirectoriesToAlwaysCook 带上；新加了这样的文件夹要去那里补一行。
# 头一回打包要编译正式版的游戏、把所有资产和材质做一遍，要几十分钟；以后只做改过的。
param([string]$Out = "")
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$ue = if ($env:UE_ROOT) { $env:UE_ROOT } else { "D:\UE5\UE_5.8" }
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) { "编辑器还开着，先关掉再打包"; exit 2 }
if (-not $Out) { $Out = Join-Path (Split-Path -Parent $root) "Dysis_Export" }
$log = Join-Path $root "Saved\package_game.log"
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
$sw = [Diagnostics.Stopwatch]::StartNew()
& "$ue\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$root\Dysis.uproject" -noP4 -utf8output -unattended `
    -platform=Win64 -clientconfig=Shipping -build -nocompileeditor -skipbuildeditor -cook -stage -pak -compressed -prereqs -nodebuginfo `
    -archive "-archivedirectory=$Out" *> $log
$code = $LASTEXITCODE
Select-String -Path $log -Pattern "BUILD SUCCESSFUL|BUILD FAILED|AutomationTool exiting|Error: |error [A-Z]+[0-9]+" | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) } | Select-Object -Unique -Last 30
"exit=$code  用时 {0:N0} 秒  导出到 $Out\Windows  （完整日志 Saved\package_game.log）" -f $sw.Elapsed.TotalSeconds
exit $code
