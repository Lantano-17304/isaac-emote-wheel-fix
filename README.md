# Isaac Emote Wheel Fix

[![build](https://github.com/Lantano-17304/isaac-emote-wheel-fix/actions/workflows/build.yml/badge.svg)](https://github.com/Lantano-17304/isaac-emote-wheel-fix/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/Lantano-17304/isaac-emote-wheel-fix?include_prereleases)](https://github.com/Lantano-17304/isaac-emote-wheel-fix/releases)
[![license](https://img.shields.io/github/license/Lantano-17304/isaac-emote-wheel-fix)](LICENSE)

《The Binding of Isaac: Repentance+》联机表情轮右摇杆修复。

在不禁止人物移动、不修改网络输入的情况下，将表情轮的方向选择从左摇杆分离到右摇杆：

| 输入 | 效果 |
| --- | --- |
| R3 | 打开表情轮 |
| 右摇杆 | 选择表情 |
| A 或 R3 | 确认并通过游戏原生逻辑发送 |
| 左摇杆 | 始终只控制人物移动 |

表情轮开启期间，右摇杆仍保留游戏原有的射击输入。

> [!WARNING]
> 当前版本是仅面向 J460 的 **Pre-release**。请先阅读下面的兼容范围，并只从本仓库的 Releases 页面下载。

## 下载

前往 [v0.1.0-pre.3 Release](https://github.com/Lantano-17304/isaac-emote-wheel-fix/releases/tag/v0.1.0-pre.3) 下载：

`IsaacEmoteWheelFix-0.1.0-pre.3-j460.zip`

不要下载 GitHub 自动生成的 “Source code” 压缩包作为安装包；它只包含源码。

当前安装包 SHA-256：

```text
BE89B4A6E136A20F2EC407C039E08DA6368ED4A626AFBC598A06F76D7213325E
```

## 兼容范围

当前已验证环境：

- Steam 原版《The Binding of Isaac: Repentance+》
- 游戏版本 `1.9.7.17.J460`
- `isaac-ng.exe` 为 PE32/x86
- 已验证 EXE SHA-256：
  `3BDFC8BAE0DC7E334B76009D0AD45DFBB16EE5F00C06FFBC3A0094E34D44616B`
- Windows 10/11 64 位
- Xbox/XInput 控制器
- 游戏目录中没有其他同名代理 DLL 或注入器

### 已知手柄限制

当前只有 Xbox/XInput 控制器确认可以正常工作。已有反馈表明，部分
PlayStation、Nintendo Switch、DirectInput 和其他通用手柄无法让表情轮读取到
正确的右摇杆方向。

Hook 本身不读取或伪造 XInput；它只把游戏原生表情轮读取器使用的摇杆组从左摇杆
组切换为右摇杆组。非 XInput 手柄经过 Steam Input 或厂商驱动后，不一定会被游戏
映射到同一个右摇杆组，因此当前版本不保证兼容。

不建议把右摇杆简单映射为左摇杆。这样会让人物移动和表情选择再次共用同一输入，
无法满足本项目的分离目标。后续兼容需要针对具体控制器后端获取诊断数据并单独验证。

目前不支持 Steam Deck、Wine/Proton、旧版 Windows、非 XInput 控制器或存在其他
游戏目录注入链的环境。

游戏更新后，EXE 哈希授权会自动失效，不会把旧补丁强行应用到新版本。

## 安装

1. 完全退出游戏。
2. 下载 Release 安装包并解压。
3. 将整个 `IsaacEmoteWheelFix` 文件夹放进游戏目录。默认目录通常为：

   ```text
   C:\Program Files (x86)\Steam\steamapps\common\The Binding of Isaac Rebirth
   ```

4. 运行 `IsaacEmoteWheelFix\IsaacEmoteFixManager.exe`。
5. 点击“安装并启用修复”，根据提示允许本次文件部署所需的管理员权限。
6. 通过 Steam 启动游戏并测试表情轮。

不要手工复制、移动或重命名 `payload` 中的 DLL。

### 管理器按钮

- **刷新**：重新检查游戏版本、进程和安装状态。
- **安装并启用修复**：安装诊断组件；对已验证的 J460 同时启用修复。
- **暂停修复**：保留文件，但切回不安装运行时补丁的诊断模式。
- **卸载**：逐个移除由本管理器安装且哈希仍匹配的文件。
- **打开日志目录**：打开诊断、配置与安装记录所在目录。

### 未识别的游戏版本

未知 EXE 哈希默认只进入诊断模式，不安装活动补丁：

1. 点击“安装并启用修复”。
2. 通过 Steam 启动游戏并正常退出一次。
3. 再次打开管理器并点击同一个按钮。

只有运行时签名唯一且结构校验通过时，管理器才允许为该完整 EXE 哈希启用兼容模式。扫描或校验失败时保持原版输入。

## 卸载与回滚

完全退出游戏，然后在管理器中点击“卸载”。

管理器只会逐个删除安装清单中记录、且哈希仍然匹配的本项目文件。文件被其他程序改动或出现同名冲突时，管理器会拒绝覆盖或删除；它不会递归删除游戏目录。

配置、授权、安装清单和日志位于：

```text
%LOCALAPPDATA%\IsaacEmoteWheelFix
```

## 工作原理与安全边界

游戏会从根目录加载本项目的 `userenv.dll` 转发代理。代理继续调用 Windows 系统 `userenv.dll`，并在 loader lock 释放后加载 `emote_input_hook.dll`。

Hook 扫描当前进程的 `.text`，要求 J460 表情轮更新路径的签名唯一并通过上下文校验。启用后，它只把表情轮调用原生摇杆向量读取器时的组参数从 `0`（左摇杆）改为 `1`（右摇杆）。修改仅存在于游戏进程内存中。

本项目：

- 不修改磁盘上的 `isaac-ng.exe`。
- 不挂接人物移动、射击动作或网络输入包。
- 不禁止人物移动，也不使用 Lua 输入拦截。
- 不轮询或伪造 XInput 状态。
- 不替换游戏的原生表情发送函数。
- 签名、结构、哈希或 trampoline 校验失败时不会安装对应 Hook。

由于安装包包含未签名的原生代理 DLL，部分安全软件可能产生误报。请核对下载来源和 SHA-256；有疑问时可以从源码自行构建。

## 构建与测试

需要 Visual Studio 2022 的 Win32 MSVC 工具链和 CMake：

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target package
```

所有可执行产物使用 `/MT` 静态运行库，普通用户不需要安装 .NET 或 VC++ Redistributable。每次推送也会由 GitHub Actions 执行 Win32 构建、代理转发测试和打包。

## 测试状态

- J460 安装、启动和基本功能测试已通过。
- Xbox/XInput 控制器已通过测试；其他控制器目前属于已知不兼容范围。
- 左摇杆移动与右摇杆表情选择可以同时工作。
- R3 打开、A/R3 确认以及轮盘期间继续射击已通过本机测试。
- GitHub Actions Win32 构建与代理转发测试已通过。

在扩大 Windows、控制器和联机样本，并持续检查 crash/desync 前，本项目会保持 Pre-release。

发现问题时，请在 [Issues](https://github.com/Lantano-17304/isaac-emote-wheel-fix/issues)
中附上游戏版本、EXE SHA-256、控制器品牌与型号、连接方式、Steam Input 是否开启、
游戏显示的控制器序号、复现步骤和日志。发布日志前请先检查并移除个人信息。

## 许可证

[MIT License](LICENSE)
