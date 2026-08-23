# ZCase

ZCase 是一个基于 Unreal Engine 5.8.1 的学习项目。目前的开发重点是在不使用 Gameplay Ability System（GAS）的情况下，构建一个小型且可测试的战斗基础。

## 环境要求

- Unreal Engine 5.8.1
- Visual Studio 2022，并安装“使用 C++ 的桌面开发”和“使用 C++ 的游戏开发”工作负载
- Git LFS

克隆项目后，请在打开项目之前获取二进制资源：

```powershell
git lfs install
git lfs pull
```

右键点击 `ZCase.uproject`，生成 Visual Studio 项目文件，然后构建 Win64 Development Editor 配置下的 `ZCaseEditor` 目标。也可以在编辑器目标构建完成后，直接用 Unreal Editor 打开项目。

## 演示地图

编辑器和游戏的启动地图是：

```text
/Game/_Game/Maps/TestLevel
```

当前演示操作沿用已有原型。完整的战斗演示流程将在第一次“攻击到死亡”的完整链路接通后补充。

## 验证

无界面运行项目自动化测试：

```powershell
F:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ZCase.uproject `
  -unattended -nop4 -nosplash -nullrhi `
  '-ExecCmds=Automation RunTests ZCase;Quit' `
  '-TestExit=Automation Test Queue Empty'
```

编译项目中的所有蓝图：

```powershell
F:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ZCase.uproject `
  -run=CompileAllBlueprints -unattended -nop4 -nosplash -nullrhi
```

该命令目前可以完成蓝图扫描，但由于现有 `GameFeatureData` 引用和 `BS_Link` 示例产生加载错误，最终会返回非零退出码。在这些资源修复之前，C++ 构建和 `ZCase` 自动化测试套件是主要的通过标准。

## 已知问题

- SPCRJointDynamics 插件在编辑器启动时会报告成员初始化校验错误。
- `BS_Link` 包含无效或缺失的动画采样，并会产生加载错误。
- `BP_Player` 仍然包含已弃用的符文菜单事件重载。在战斗功能链路稳定之前，这些内容会暂时保留。

这些已有诊断信息与 `ZCase` 自动化测试结果分开跟踪。

## 资源声明

本仓库包含用于非商业学习项目的第三方素材。这里不代表这些素材的公开再分发权已经过审查或获得授权。项目使用者应自行确认其使用行为适用的权利和许可。
