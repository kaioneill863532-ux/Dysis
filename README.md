# 狄西斯的日落回廊 · Dysis and The Celestial Cloister

一款以行走、观察和空间推理为核心的 3D 解谜游戏。

引擎：Unreal Engine 5.8.2（C++ 工程）

## 目录结构

```
Dysis/
├── Dysis.uproject          UE 工程文件
├── Config/                 工程设置
├── Content/                UE 资源
│   └── Dysis/
│       ├── Maps/Dysis_Temple    神殿关卡（主关卡）
│       ├── Temple_v0_12/        导入的建筑与机关网格
│       └── Sky/                 月亮圆盘材质
├── Source/Dysis/           C++ 源码
│   ├── Sky/                日月轨道、时间函数库、玩家时间组件
│   ├── Audio/              背景音乐管理器；音效系统（设置页、播放、总调度、脚步）
│   └── Player/             测试用第一人称玩家与 GameMode
├── Art/Models/temple-v0.12/     建筑模型源文件：Blender、FBX、UE 导入脚本、机关清单、核对报告
├── Art/Audio/              265 个音效 WAV、一键导入脚本 ue_import_sfx.py、说明（README.md）
└── Docs/
    ├── UE实现说明.md        日月与时间系统的实现、给程序的接口、已知问题
    └── Design/             游戏设计、系统文档、施工图
```

## 开始

1. 安装 [Git LFS](https://git-lfs.com)（Git for Windows 自带），然后克隆：
   ```bash
   git lfs install
   git clone https://github.com/kaioneill863532-ux/Dysis.git
   ```
2. 安装 UE 5.8 和 Visual Studio 2022（勾选“使用 C++ 的游戏开发”）。
3. 双击 `Dysis.uproject`，提示重新编译模块时选“是”（仓库不带编译产物）。
   也可以右键 `Dysis.uproject` → Generate Visual Studio project files，用 VS 编译 `DysisEditor`。
4. 打开关卡 `Content/Dysis/Maps/Dysis_Temple`，点运行。

**操作**：WASD 移动 · 鼠标视角 · 空格跳 · Shift 跑
**控制台**（`~`）：`Dysis.Where` 打印当前区域、时刻、主光；`Dysis.Go <X> <Y> <Z> [night]` 把脚底传送到 UE 坐标（厘米）。

## 当前进度

- ✅ v0.12 建筑与机关模型导入 UE（导入核对 331/331）
- ✅ 日月轨道 + “站在哪里决定几点”：与灰盒标准答案逐条一致（7860/7860），PIE 实测通过
- ⬜ 光柱、光阶、光照后出现的路、机关动作、材质、特效、UI（程序负责，接口见 [Docs/UE实现说明.md](Docs/UE实现说明.md)）

## 注意

- 同一个工程不要同时开两个编辑器。
- 夜里画面被自动曝光提得偏亮，亮度待美术调整。
- `Content/NewMap` 是新建工程时自带的开放世界模板关卡，没有用到。
