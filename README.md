# Isaac Emote Wheel Fix

这是面向 Steam 原版《The Binding of Isaac: Repentance+》J460 的实验性 Windows 原生修复。目标是在不拦截人物移动、射击或网络输入的情况下，让表情轮使用右摇杆选择方向。

## 支持范围

- Windows 10/11 64 位
- Steam 原版当前 J460，游戏进程为 PE32/x86
- Xbox/XInput 控制器
- 不支持其他注入器、Steam Deck、Wine/Proton 或旧版 Windows

本项目目前必须以 **Pre-release** 发布。Steam 原版 J460 已确认会加载游戏根目录的 `userenv.dll`，且原版 EXE 的轮盘签名唯一、结构验证通过。正式稳定版仍需完成输入与联机测试。

## 安装流程

1. 将整个 `IsaacEmoteWheelFix` 文件夹放到游戏目录中，不要手工复制或重命名 `payload` 内的 DLL。
2. 确认游戏已完全退出。
3. 运行 `IsaacEmoteFixManager.exe`，点击“安装并启用修复”，并同意仅本次文件部署所需的 UAC。
4. 已验证的 Steam 原版 J460 会一次完成安装和哈希授权，下次启动直接生效。
5. 对未知哈希，管理器只安装安全诊断：通过 Steam 启动并正常退出一次，再点击同一个“安装并启用修复”按钮；只有签名唯一且上下文验证通过时才会启用。
6. 启动游戏验证输入。

未知 EXE 哈希默认只诊断，不修改游戏代码。兼容授权绑定完整 SHA-256；游戏更新后自动失效。

## 实际修改

Hook 在当前进程的 `.text` 中扫描并验证 J460 `EmoteWheel::Update` 的唯一上下文。启用后只把轮盘调用原生摇杆向量读取器时的组参数从 `0`（左摇杆）改为 `1`（右摇杆）。修改只存在于游戏进程内存中。

- R3 打开与 A/R3 确认继续使用游戏原生逻辑。
- 左摇杆、人物移动、射击动作和网络输入均不挂接。
- 轮盘期间右摇杆继续产生原生射击输入。
- 公开版诊断模式不安装 detour，也不轮询 XInput。

## 状态文件

配置、授权、安装清单和日志位于：

```text
%LOCALAPPDATA%\IsaacEmoteWheelFix
```

游戏目录只部署本项目的 `userenv.dll` 和 `emote_input_hook.dll`。发现未知同名 DLL 时管理器会拒绝覆盖。游戏运行期间管理器只允许查看状态和日志。

## 卸载

完全退出游戏，在管理器中点击“卸载”。管理器只删除哈希与安装清单一致的两个明确 DLL；文件被其他程序修改后会拒绝删除。管理器不会递归删除目录。

## 构建

使用 VS 2022 的 Win32 MSVC 工具链：

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
cmake --build build --config Release --target package
```

所有可执行产物使用 `/MT` 静态运行库，不要求安装 .NET 或 VC++ Redistributable。

## 发布前验证门槛

- 在独立 Steam 原版 J460 副本证明原版自然加载根目录代理。
- 验证系统 `userenv.dll` 转发、重复安装、冲突拒绝和逐文件卸载。
- 验证左右摇杆、八方向、A/R3、菜单、暂停和控制器重连。
- 完成至少一次联机会话和 20 次表情发送，检查 crash 与 desync。
- Windows 10/11 各取得至少一次测试结果。

未完成这些门槛前不得标记为稳定版。
