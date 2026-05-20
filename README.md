# GVInputController

[English README](README_EN.md)

GVInputController 是一个 Windows 用户态 GVInput 调用示例项目，核心能力封装在单头文件库 `GVInput.hpp` 中。

`GVInput.hpp` 负责枚举 GameViewer 的 GVInput 虚拟 HID 设备，并通过 `0x40` vendor output report 发送鼠标和键盘输入。

## 功能

- 枚举 `VID_00FF&PID_BACC` 的 GVInput HID 设备。
- 自动选择 `UsagePage=0xFFEE`、`Usage=0xCC`、`OutputLen>=65` 的 vendor output collection。
- 发送相对鼠标移动 report。
- 发送绝对鼠标坐标 report。
- 发送左键点击 report。
- 发送键盘按键 report。
- 支持从资源释放并安装 GVInput 驱动包。

## 环境要求

- Windows x64。
- Visual Studio 2022 或兼容 MSVC 工具链。
- Windows SDK，需包含 HID、SetupAPI、NewDev 头文件和库。
- 如果需要安装驱动，程序必须以管理员权限运行。
- 驱动安装依赖原始驱动签名和系统驱动策略。

`GVInput.hpp` 内部通过 `#pragma comment(lib, ...)` 自动链接：

```text
advapi32.lib
hid.lib
newdev.lib
setupapi.lib
shell32.lib
```

## 单头文件用法

把 `GVInput.hpp` 放到你的项目中，然后包含它：

```cpp
#include "GVInput.hpp"

int main() {
    gvinput::Device device;

    if (!device.open(true)) {
        gvinput::InstallOrRelaunchElevated(true);
        return 1;
    }

    device.move_relative(80, 0, 0, true);
    device.click_left(true);
    device.key_tap(0x04, 0, true); // HID usage 0x04 = A
    return 0;
}
```

## 常用 API

```cpp
gvinput::Device device;
device.open(true);

device.move_relative(80, 0);          // 鼠标右移 80
device.move_absolute(16384, 16384);   // 移动到绝对坐标附近
device.click_left();                  // 左键点击
device.key_tap(0x04);                 // 按下并释放 A
```

也可以使用底层句柄接口：

```cpp
gvinput::ScopedHandle handle = gvinput::OpenDevice(true);
gvinput::MoveMouseRelative(handle.get(), 80, 0, 0, true);
```

## 示例程序

编译 `GVInputController.sln` 后，可以运行：

```powershell
GVInputController.exe move 80 0
GVInputController.exe abs 16384 16384
GVInputController.exe click
GVInputController.exe keya
GVInputController.exe demo
```

无参数运行时，示例程序会打印帮助并执行默认测试：

```text
move 80 0
```

## 驱动安装

如果系统已经安装 GameViewer 的 GVInput 驱动，通常不需要执行安装流程，直接打开设备即可。

如果需要由程序安装驱动，可把驱动包作为资源嵌入 exe。资源 ID 需要与 `GVInput.hpp` 中的常量一致：

```text
101 gvinput.inf
102 gvinput.cat
103 gvinput.sys
104 gvinputmf.inf
105 gvinputmf.cat
106 gvinputmf.sys
107 WdfCoInstaller01009.dll
```

示例 `GVInputResources.rc`：

```rc
101 RCDATA "drivers\\gvinput\\gvinput.inf"
102 RCDATA "drivers\\gvinput\\gvinput.cat"
103 RCDATA "drivers\\gvinput\\gvinput.sys"
104 RCDATA "drivers\\gvinput\\gvinputmf.inf"
105 RCDATA "drivers\\gvinput\\gvinputmf.cat"
106 RCDATA "drivers\\gvinput\\gvinputmf.sys"
107 RCDATA "drivers\\gvinput\\WdfCoInstaller01009.dll"
```

安装接口：

```cpp
gvinput::InstallResult result = gvinput::InstallDriverFromResources(false, true);
```

自动提权安装：

```cpp
gvinput::InstallOrRelaunchElevated(true);
```

示例程序命令：

```powershell
GVInputController.exe install-elevated
GVInputController.exe install
```

安装流程会释放资源到：

```text
%TEMP%\GVInputControllerDriver
```

然后执行：

```text
pnputil /add-driver gvinput.inf /install
SetupDiCreateDeviceInfo + UpdateDriverForPlugAndPlayDevices(Netease\gvinput)
pnputil /add-driver gvinputmf.inf /install
pnputil /scan-devices
```

