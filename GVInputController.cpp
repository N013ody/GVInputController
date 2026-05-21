#include "GVInput.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int ParseInt(const wchar_t* text) {
    return static_cast<int>(std::wcstol(text, nullptr, 0));
}



int Install(bool allowElevate) {
    if (!gvinput::IsRunningAsAdmin()) {
        if (allowElevate) {
            if (gvinput::RelaunchElevated(L"install")) {
                std::wcout << L"Started elevated installer.\n";
                return 0;
            }
            std::wcerr << L"Failed to start elevated installer.\n";
            return 1;
        }

        std::wcerr << L"Administrator privileges are required. Run install-elevated or start as administrator.\n";
        return 1;
    }

    gvinput::InstallResult result = gvinput::InstallDriverFromResources(false, true);
    std::wcout << result.message << L"\n";
    if (!result.packageDirectory.empty()) {
        std::wcout << L"Extracted package: " << result.packageDirectory << L"\n";
    }
    if (result.rebootRequired) {
        std::wcout << L"A reboot is required before GVInput is usable.\n";
    }
    if (!result.ok) {
        std::wcerr << L"Install failed. error=" << result.error << L"\n";
        return 1;
    }

    return 0;
}

bool WasKeyPressed(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 1) != 0;
}

int RunHotkeyExample(gvinput::Device& device) {
    std::wcout << L"Hotkey example started.\n"
               << L"  F5  -> move mouse 80 pixels right\n"
               << L"  F6  -> tap Q\n"
               << L"  Esc -> exit\n";

    for (;;) {
        if (WasKeyPressed(VK_ESCAPE)) {
            std::wcout << L"Hotkey example stopped.\n";
            return 0;
        }

        if (WasKeyPressed(VK_F5)) {
            if (!device.move_relative(80, 0, 0, true)) {
                std::cerr << "F5 move failed: " << GetLastError() << "\n";
                return 3;
            }
        }

        if (WasKeyPressed(VK_F6)) {
            if (!device.key_tap(0x14, 0, true)) {
                std::cerr << "F6 Q key tap failed: " << GetLastError() << "\n";
                return 3;
            }
        }

        Sleep(10);
    }
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {


    gvinput::Device device;
    if (!device.open(true)) {
        if (gvinput::InstallOrRelaunchElevated(true)) {
            if (!gvinput::IsRunningAsAdmin()) {
                std::wcout << L"Started elevated embedded GVInput installer. Re-run this program after installation finishes.\n";
                return 0;
            }

            Sleep(1000);
            if (device.open(true)) {
                std::wcout << L"GVInput installed and opened.\n";
            } else {
                std::wcerr << L"GVInput install completed, but the device is not available yet. Reboot or re-run later.\n";
                return 1;
            }
        } else {
            std::wcerr << L"GVInput is not available and automatic install failed.\n"
                       << L"Run: GVInputController.exe install-elevated\n";
            return 1;
        }
    }

    RunHotkeyExample(device);
    std::cout << "GVInput report sent.\n";
    return 0;
}
