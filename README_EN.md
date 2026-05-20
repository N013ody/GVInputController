# GVInputController

GVInputController is a Windows user-mode example for controlling the GameViewer GVInput virtual HID device. The core implementation is packaged as a single-header library: `GVInput.hpp`.

`GVInput.hpp` enumerates the GVInput HID device and sends mouse or keyboard input through the `0x40` vendor output report.

## Features

- Enumerates the GVInput HID device with `VID_00FF&PID_BACC`.
- Selects the vendor output collection with `UsagePage=0xFFEE`, `Usage=0xCC`, and `OutputLen>=65`.
- Sends relative mouse movement reports.
- Sends absolute mouse coordinate reports.
- Sends left-click reports.
- Sends keyboard key-tap reports.
- Optionally extracts and installs the GVInput driver package from embedded resources.



## Requirements

- Windows x64.
- Visual Studio 2022 or a compatible MSVC toolchain.
- Windows SDK with HID, SetupAPI, and NewDev headers/libraries.
- Administrator privileges are required for driver installation.
- Driver installation depends on the original driver signature and the local Windows driver policy.

`GVInput.hpp` links the required Windows libraries through `#pragma comment(lib, ...)`:

```text
advapi32.lib
hid.lib
newdev.lib
setupapi.lib
shell32.lib
```

## Single-Header Usage

Copy `GVInput.hpp` into your project and include it:

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

## Common APIs

```cpp
gvinput::Device device;
device.open(true);

device.move_relative(80, 0);          // Move mouse right by 80
device.move_absolute(16384, 16384);   // Move to an absolute coordinate
device.click_left();                  // Left click
device.key_tap(0x04);                 // Press and release A
```

You can also use the lower-level handle API:

```cpp
gvinput::ScopedHandle handle = gvinput::OpenDevice(true);
gvinput::MoveMouseRelative(handle.get(), 80, 0, 0, true);
```

## Example Program

After building `GVInputController.sln`, run:

```powershell
GVInputController.exe move 80 0
GVInputController.exe abs 16384 16384
GVInputController.exe click
GVInputController.exe keya
GVInputController.exe demo
```

When launched without arguments, the example prints usage information and runs the default test:

```text
move 80 0
```

## Driver Installation

If GameViewer's GVInput driver is already installed, no installation step is usually required. The library can directly open the existing HID device.

If your program needs to install the driver, embed the driver package as resources. The resource IDs must match the constants in `GVInput.hpp`:

```text
101 gvinput.inf
102 gvinput.cat
103 gvinput.sys
104 gvinputmf.inf
105 gvinputmf.cat
106 gvinputmf.sys
107 WdfCoInstaller01009.dll
```

Example `GVInputResources.rc`:

```rc
101 RCDATA "drivers\\gvinput\\gvinput.inf"
102 RCDATA "drivers\\gvinput\\gvinput.cat"
103 RCDATA "drivers\\gvinput\\gvinput.sys"
104 RCDATA "drivers\\gvinput\\gvinputmf.inf"
105 RCDATA "drivers\\gvinput\\gvinputmf.cat"
106 RCDATA "drivers\\gvinput\\gvinputmf.sys"
107 RCDATA "drivers\\gvinput\\WdfCoInstaller01009.dll"
```

Installation API:

```cpp
gvinput::InstallResult result = gvinput::InstallDriverFromResources(false, true);
```

Automatic elevation helper:

```cpp
gvinput::InstallOrRelaunchElevated(true);
```

Example commands:

```powershell
GVInputController.exe install-elevated
GVInputController.exe install
```

The installation routine extracts embedded resources to:

```text
%TEMP%\GVInputControllerDriver
```

Then it performs:

```text
pnputil /add-driver gvinput.inf /install
SetupDiCreateDeviceInfo + UpdateDriverForPlugAndPlayDevices(Netease\gvinput)
pnputil /add-driver gvinputmf.inf /install
pnputil /scan-devices
```

## Notes

- Do not copy only `gvinput.sys`; that will not create the HID device.
- Driver installation requires the full `inf/cat/sys` package.
- Installation can fail if the driver signature is invalid or blocked by system policy.
- `gvinputmf.sys` is a HID mouse filter and should be installed together with `gvinput.sys`.
- Do not commit third-party driver binaries to a public repository unless redistribution is permitted.

## Remote Repository

```text
https://github.com/N013ody/GVInputController.git
```
