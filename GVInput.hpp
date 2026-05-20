#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <hidsdi.h>
#include <hidpi.h>
#include <newdev.h>
#include <setupapi.h>
#include <shellapi.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "shell32.lib")

namespace gvinput {

constexpr USHORT kVid = 0x00FF;
constexpr USHORT kPid = 0xBACC;
constexpr USAGE kVendorUsagePage = 0xFFEE;
constexpr USAGE kVendorUsage = 0x00CC;
constexpr BYTE kVendorReportId = 0x40;
constexpr size_t kVendorReportWriteSize = 65;
constexpr wchar_t kHardwareId[] = L"Netease\\gvinput";

constexpr int kResGvInputInf = 101;
constexpr int kResGvInputCat = 102;
constexpr int kResGvInputSys = 103;
constexpr int kResGvInputMfInf = 104;
constexpr int kResGvInputMfCat = 105;
constexpr int kResGvInputMfSys = 106;
constexpr int kResWdfCoInstaller = 107;

struct InstallResult {
    bool ok = false;
    bool rebootRequired = false;
    DWORD error = ERROR_SUCCESS;
    std::wstring message;
    std::wstring packageDirectory;
};

struct HidCapsInfo {
    USAGE usagePage = 0;
    USAGE usage = 0;
    USHORT inputReportByteLength = 0;
    USHORT outputReportByteLength = 0;
    USHORT featureReportByteLength = 0;
};

class ScopedHandle;

inline ScopedHandle OpenDevice(bool verbose);
inline bool SendReport(HANDLE device, const uint8_t* inner, size_t innerLen, bool verbose);
inline bool MoveMouseRelative(HANDLE device, int dx, int dy, int wheel, bool verbose);
inline bool MoveMouseAbsolute(HANDLE device, uint16_t x, uint16_t y, uint8_t buttons, int wheel, bool verbose);
inline bool ClickLeft(HANDLE device, bool verbose);
inline bool KeyTap(HANDLE device, uint8_t hidUsage, uint8_t modifier, bool verbose);

class ScopedHandle {
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : value_(handle) {}
    ~ScopedHandle() { reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : value_(other.value_) {
        other.value_ = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = other.value_;
            other.value_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    bool valid() const { return value_ != INVALID_HANDLE_VALUE && value_ != nullptr; }
    HANDLE get() const { return value_; }

    HANDLE release() {
        HANDLE handle = value_;
        value_ = INVALID_HANDLE_VALUE;
        return handle;
    }

    void reset(HANDLE handle = INVALID_HANDLE_VALUE) {
        if (valid()) {
            CloseHandle(value_);
        }
        value_ = handle;
    }

private:
    HANDLE value_ = INVALID_HANDLE_VALUE;
};

class DeviceInfoSet {
public:
    explicit DeviceInfoSet(HDEVINFO set) : value_(set) {}
    ~DeviceInfoSet() {
        if (value_ != INVALID_HANDLE_VALUE) {
            SetupDiDestroyDeviceInfoList(value_);
        }
    }

    DeviceInfoSet(const DeviceInfoSet&) = delete;
    DeviceInfoSet& operator=(const DeviceInfoSet&) = delete;

    bool valid() const { return value_ != INVALID_HANDLE_VALUE; }
    HDEVINFO get() const { return value_; }

private:
    HDEVINFO value_ = INVALID_HANDLE_VALUE;
};

class Device {
public:
    bool open(bool verbose = false) {
        handle_ = OpenDevice(verbose);
        return handle_.valid();
    }

    bool valid() const { return handle_.valid(); }
    HANDLE native_handle() const { return handle_.get(); }

    bool send_report(const uint8_t* inner, size_t innerLen, bool verbose = false) const {
        return SendReport(handle_.get(), inner, innerLen, verbose);
    }

    bool move_relative(int dx, int dy, int wheel = 0, bool verbose = false) const {
        return MoveMouseRelative(handle_.get(), dx, dy, wheel, verbose);
    }

    bool move_absolute(uint16_t x, uint16_t y, uint8_t buttons = 0, int wheel = 0, bool verbose = false) const {
        return MoveMouseAbsolute(handle_.get(), x, y, buttons, wheel, verbose);
    }

    bool click_left(bool verbose = false) const {
        return ClickLeft(handle_.get(), verbose);
    }

    bool key_tap(uint8_t hidUsage, uint8_t modifier = 0, bool verbose = false) const {
        return KeyTap(handle_.get(), hidUsage, modifier, verbose);
    }

private:
    ScopedHandle handle_;
};

inline int ClampInt(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

inline int ClampHidAxis(int value) {
    return ClampInt(value, -127, 127);
}

inline bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
            &ntAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0,
            0,
            0,
            0,
            0,
            0,
            &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

inline bool RelaunchElevated(const std::wstring& arguments) {
    wchar_t exePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        return false;
    }

    HINSTANCE result = ShellExecuteW(
        nullptr,
        L"runas",
        exePath,
        arguments.empty() ? nullptr : arguments.c_str(),
        nullptr,
        SW_SHOWNORMAL);

    return reinterpret_cast<INT_PTR>(result) > 32;
}

inline std::wstring JoinPath(const std::wstring& dir, const std::wstring& name) {
    if (dir.empty() || dir[dir.size() - 1] == L'\\' || dir[dir.size() - 1] == L'/') {
        return dir + name;
    }
    return dir + L"\\" + name;
}

inline bool EnsureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) {
        return true;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

inline bool WriteResourceToFile(int resourceId, const std::wstring& path, DWORD* error = nullptr) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!resource) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    HGLOBAL loaded = LoadResource(module, resource);
    DWORD size = SizeofResource(module, resource);
    const void* data = LockResource(loaded);
    if (!loaded || !data || size == 0) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    ScopedHandle file(CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));

    if (!file.valid()) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    DWORD written = 0;
    if (!WriteFile(file.get(), data, size, &written, nullptr) || written != size) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    return true;
}

inline bool ExtractDriverPackage(std::wstring* outputDirectory, DWORD* error = nullptr) {
    wchar_t tempPath[MAX_PATH] = {};
    if (!GetTempPathW(MAX_PATH, tempPath)) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    std::wstring dir = JoinPath(tempPath, L"GVInputControllerDriver");
    if (!EnsureDirectory(dir)) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    struct ResourceFile {
        int id;
        const wchar_t* name;
    };

    const ResourceFile files[] = {
        {kResGvInputInf, L"gvinput.inf"},
        {kResGvInputCat, L"gvinput.cat"},
        {kResGvInputSys, L"gvinput.sys"},
        {kResGvInputMfInf, L"gvinputmf.inf"},
        {kResGvInputMfCat, L"gvinputmf.cat"},
        {kResGvInputMfSys, L"gvinputmf.sys"},
        {kResWdfCoInstaller, L"WdfCoInstaller01009.dll"},
    };

    for (const ResourceFile& file : files) {
        if (!WriteResourceToFile(file.id, JoinPath(dir, file.name), error)) {
            return false;
        }
    }

    if (outputDirectory) {
        *outputDirectory = dir;
    }
    return true;
}

inline bool RunProcessAndWait(const std::wstring& commandLine, DWORD* exitCode = nullptr) {
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};

    if (!CreateProcessW(
            nullptr,
            mutableCommand.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &startup,
            &process)) {
        return false;
    }

    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);

    if (exitCode) {
        *exitCode = code;
    }
    return true;
}

inline bool RunPnpUtil(const std::wstring& args, DWORD* exitCode = nullptr) {
    wchar_t systemDir[MAX_PATH] = {};
    if (!GetSystemDirectoryW(systemDir, MAX_PATH)) {
        return false;
    }

    std::wstring command = L"\"" + JoinPath(systemDir, L"pnputil.exe") + L"\" " + args;
    return RunProcessAndWait(command, exitCode);
}

inline bool MultiSzContains(const wchar_t* multiSz, DWORD byteSize, const wchar_t* expected) {
    if (!multiSz || !expected || byteSize < sizeof(wchar_t)) {
        return false;
    }

    const wchar_t* current = multiSz;
    const wchar_t* end = reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(multiSz) + byteSize);
    while (current < end && *current) {
        if (_wcsicmp(current, expected) == 0) {
            return true;
        }
        current += wcslen(current) + 1;
    }

    return false;
}

inline bool RootDeviceExists() {
    DeviceInfoSet devices(SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES));
    if (!devices.valid()) {
        return false;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVINFO_DATA data = {};
        data.cbSize = sizeof(data);
        if (!SetupDiEnumDeviceInfo(devices.get(), index, &data)) {
            break;
        }

        BYTE buffer[1024] = {};
        DWORD propertyType = 0;
        DWORD requiredSize = 0;
        if (SetupDiGetDeviceRegistryPropertyW(
                devices.get(),
                &data,
                SPDRP_HARDWAREID,
                &propertyType,
                buffer,
                sizeof(buffer),
                &requiredSize) &&
            propertyType == REG_MULTI_SZ &&
            MultiSzContains(reinterpret_cast<const wchar_t*>(buffer), requiredSize, kHardwareId)) {
            return true;
        }
    }

    return false;
}

inline bool CreateRootDevice(DWORD* error = nullptr) {
    if (RootDeviceExists()) {
        return true;
    }

    GUID hidClassGuid = {0x745a17a0, 0x74d3, 0x11d0, {0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda}};
    DeviceInfoSet devices(SetupDiCreateDeviceInfoList(&hidClassGuid, nullptr));
    if (!devices.valid()) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    SP_DEVINFO_DATA deviceData = {};
    deviceData.cbSize = sizeof(deviceData);
    if (!SetupDiCreateDeviceInfoW(
            devices.get(),
            L"GVInput",
            &hidClassGuid,
            L"gvinput Device",
            nullptr,
            DICD_GENERATE_ID,
            &deviceData)) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    const wchar_t hardwareId[] = L"Netease\\gvinput\0\0";
    if (!SetupDiSetDeviceRegistryPropertyW(
            devices.get(),
            &deviceData,
            SPDRP_HARDWAREID,
            reinterpret_cast<const BYTE*>(hardwareId),
            sizeof(hardwareId))) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devices.get(), &deviceData)) {
        if (error) {
            *error = GetLastError();
        }
        return false;
    }

    return true;
}

inline bool ReadHidCaps(HANDLE device, HidCapsInfo* capsInfo) {
    if (!capsInfo) {
        return false;
    }

    PHIDP_PREPARSED_DATA preparsedData = nullptr;
    if (!HidD_GetPreparsedData(device, &preparsedData)) {
        return false;
    }

    HIDP_CAPS caps = {};
    NTSTATUS status = HidP_GetCaps(preparsedData, &caps);
    HidD_FreePreparsedData(preparsedData);

    if (status != HIDP_STATUS_SUCCESS) {
        return false;
    }

    capsInfo->usagePage = caps.UsagePage;
    capsInfo->usage = caps.Usage;
    capsInfo->inputReportByteLength = caps.InputReportByteLength;
    capsInfo->outputReportByteLength = caps.OutputReportByteLength;
    capsInfo->featureReportByteLength = caps.FeatureReportByteLength;
    return true;
}

inline bool IsOutputCollection(const HidCapsInfo& caps) {
    return caps.usagePage == kVendorUsagePage &&
           caps.usage == kVendorUsage &&
           caps.outputReportByteLength >= kVendorReportWriteSize;
}

inline ScopedHandle OpenHidPath(const wchar_t* path) {
    const DWORD desiredAccess[] = {
        GENERIC_READ | GENERIC_WRITE,
        GENERIC_WRITE,
    };

    for (DWORD access : desiredAccess) {
        ScopedHandle device(CreateFileW(
            path,
            access,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (device.valid()) {
            return device;
        }
    }

    return ScopedHandle();
}

inline ScopedHandle OpenDevice(bool verbose = false) {
    GUID hidGuid = {};
    HidD_GetHidGuid(&hidGuid);

    DeviceInfoSet devices(SetupDiGetClassDevsW(
        &hidGuid,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

    if (!devices.valid()) {
        if (verbose) {
            std::cerr << "SetupDiGetClassDevsW failed: " << GetLastError() << "\n";
        }
        return ScopedHandle();
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData = {};
        interfaceData.cbSize = sizeof(interfaceData);

        if (!SetupDiEnumDeviceInterfaces(devices.get(), nullptr, &hidGuid, index, &interfaceData)) {
            break;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devices.get(), &interfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) {
            continue;
        }

        std::vector<BYTE> detailBuffer(requiredSize);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(
                devices.get(),
                &interfaceData,
                detail,
                requiredSize,
                nullptr,
                nullptr)) {
            continue;
        }

        ScopedHandle device = OpenHidPath(detail->DevicePath);
        if (!device.valid()) {
            continue;
        }

        HIDD_ATTRIBUTES attributes = {};
        attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(device.get(), &attributes)) {
            continue;
        }

        if (attributes.VendorID != kVid || attributes.ProductID != kPid) {
            continue;
        }

        HidCapsInfo caps = {};
        if (!ReadHidCaps(device.get(), &caps)) {
            continue;
        }

        if (verbose) {
            std::wcout << L"Found GVInput collection: " << detail->DevicePath << L"\n"
                       << L"  UsagePage=0x" << std::hex << caps.usagePage
                       << L" Usage=0x" << caps.usage << std::dec
                       << L" InputLen=" << caps.inputReportByteLength
                       << L" OutputLen=" << caps.outputReportByteLength
                       << L" FeatureLen=" << caps.featureReportByteLength << L"\n";
        }

        if (IsOutputCollection(caps)) {
            if (verbose) {
                std::wcout << L"Opened GVInput output collection: " << detail->DevicePath << L"\n";
            }
            return device;
        }
    }

    if (verbose) {
        std::cerr << "GVInput output collection was not found.\n";
    }
    return ScopedHandle();
}

inline bool IsInstalled() {
    ScopedHandle device = OpenDevice(false);
    return device.valid();
}

inline InstallResult InstallDriverFromResources(bool force = false, bool verbose = false) {
    InstallResult result;

    if (!force && IsInstalled()) {
        result.ok = true;
        result.message = L"GVInput is already installed.";
        return result;
    }

    if (!IsRunningAsAdmin()) {
        result.error = ERROR_ACCESS_DENIED;
        result.message = L"Administrator privileges are required to install GVInput.";
        return result;
    }

    DWORD error = ERROR_SUCCESS;
    std::wstring packageDir;
    if (!ExtractDriverPackage(&packageDir, &error)) {
        result.error = error;
        result.message = L"Failed to extract embedded GVInput driver package.";
        return result;
    }

    result.packageDirectory = packageDir;

    const std::wstring gvinputInf = JoinPath(packageDir, L"gvinput.inf");
    const std::wstring gvinputMfInf = JoinPath(packageDir, L"gvinputmf.inf");

    DWORD exitCode = 0;
    RunPnpUtil(L"/add-driver \"" + gvinputInf + L"\" /install", &exitCode);
    if (verbose) {
        std::wcout << L"pnputil gvinput.inf exit code: " << exitCode << L"\n";
    }

    if (!CreateRootDevice(&error)) {
        result.error = error;
        result.message = L"Failed to create Netease\\gvinput root device.";
        return result;
    }

    BOOL rebootRequired = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(
            nullptr,
            kHardwareId,
            gvinputInf.c_str(),
            INSTALLFLAG_FORCE,
            &rebootRequired)) {
        error = GetLastError();
        if (error != ERROR_NO_SUCH_DEVINST) {
            result.error = error;
            result.message = L"Failed to bind gvinput.inf to Netease\\gvinput.";
            return result;
        }
    }

    result.rebootRequired = rebootRequired == TRUE;

    RunPnpUtil(L"/add-driver \"" + gvinputMfInf + L"\" /install", &exitCode);
    if (verbose) {
        std::wcout << L"pnputil gvinputmf.inf exit code: " << exitCode << L"\n";
    }

    RunPnpUtil(L"/scan-devices", &exitCode);
    if (verbose) {
        std::wcout << L"pnputil scan-devices exit code: " << exitCode << L"\n";
    }

    result.ok = true;
    result.message = L"GVInput driver package was installed.";
    return result;
}

inline bool EnsureInstalledFromResources(bool verbose = false) {
    if (IsInstalled()) {
        return true;
    }

    InstallResult result = InstallDriverFromResources(false, verbose);
    if (verbose && !result.message.empty()) {
        std::wcout << result.message << L"\n";
    }
    return result.ok;
}

inline bool InstallOrRelaunchElevated(bool verbose = false) {
    if (IsInstalled()) {
        return true;
    }

    if (!IsRunningAsAdmin()) {
        if (verbose) {
            std::wcerr << L"GVInput is not installed; requesting administrator privileges.\n";
        }
        return RelaunchElevated(L"install");
    }

    InstallResult result = InstallDriverFromResources(false, verbose);
    if (verbose && !result.message.empty()) {
        std::wcout << result.message << L"\n";
    }
    return result.ok;
}

inline bool SendReport(HANDLE device, const uint8_t* inner, size_t innerLen, bool verbose = false) {
    if (!device || device == INVALID_HANDLE_VALUE || !inner || innerLen > 63) {
        if (verbose) {
            std::cerr << "Invalid GVInput report arguments.\n";
        }
        return false;
    }

    std::array<uint8_t, kVendorReportWriteSize> report = {};
    report[0] = kVendorReportId;
    report[1] = static_cast<uint8_t>(innerLen);
    std::memcpy(report.data() + 2, inner, innerLen);

    DWORD written = 0;
    if (WriteFile(device, report.data(), static_cast<DWORD>(report.size()), &written, nullptr) &&
        written == report.size()) {
        if (verbose) {
            std::cout << "WriteFile sent report id 0x40, inner report id 0x"
                      << std::hex << static_cast<int>(inner[0]) << std::dec
                      << ", inner length " << innerLen << "\n";
        }
        return true;
    }

    DWORD writeError = GetLastError();
    if (verbose) {
        std::cerr << "WriteFile failed or wrote a short report. written=" << written
                  << ", error=" << writeError << "; trying HidD_SetOutputReport.\n";
    }

    if (HidD_SetOutputReport(device, report.data(), static_cast<ULONG>(report.size())) == TRUE) {
        if (verbose) {
            std::cout << "HidD_SetOutputReport sent report id 0x40, inner report id 0x"
                      << std::hex << static_cast<int>(inner[0]) << std::dec
                      << ", inner length " << innerLen << "\n";
        }
        return true;
    }

    if (verbose) {
        std::cerr << "HidD_SetOutputReport failed: " << GetLastError() << "\n";
    }
    SetLastError(writeError);
    return false;
}

inline bool MoveMouseRelative(HANDLE device, int dx, int dy, int wheel = 0, bool verbose = false) {
    uint8_t report[] = {
        0x04,
        0x00,
        static_cast<uint8_t>(static_cast<int8_t>(ClampHidAxis(dx))),
        static_cast<uint8_t>(static_cast<int8_t>(ClampHidAxis(dy))),
        static_cast<uint8_t>(static_cast<int8_t>(ClampHidAxis(wheel))),
    };

    return SendReport(device, report, sizeof(report), verbose);
}

inline bool MoveMouseAbsolute(HANDLE device, uint16_t x, uint16_t y, uint8_t buttons = 0, int wheel = 0, bool verbose = false) {
    x = static_cast<uint16_t>(ClampInt(x, 0, 0x7FFF));
    y = static_cast<uint16_t>(ClampInt(y, 0, 0x7FFF));

    uint8_t report[] = {
        0x03,
        buttons,
        static_cast<uint8_t>(x & 0xFF),
        static_cast<uint8_t>((x >> 8) & 0xFF),
        static_cast<uint8_t>(y & 0xFF),
        static_cast<uint8_t>((y >> 8) & 0xFF),
        static_cast<uint8_t>(static_cast<int8_t>(ClampHidAxis(wheel))),
    };

    return SendReport(device, report, sizeof(report), verbose);
}

inline bool ClickLeft(HANDLE device, bool verbose = false) {
    uint8_t leftDown[] = {0x04, 0x01, 0, 0, 0};
    uint8_t leftUp[] = {0x04, 0x00, 0, 0, 0};

    if (!SendReport(device, leftDown, sizeof(leftDown), verbose)) {
        return false;
    }

    Sleep(20);
    return SendReport(device, leftUp, sizeof(leftUp), verbose);
}

inline bool KeyTap(HANDLE device, uint8_t hidUsage, uint8_t modifier = 0, bool verbose = false) {
    uint8_t keyDown[] = {0x07, modifier, 0x00, hidUsage, 0, 0, 0, 0, 0};
    uint8_t keyUp[] = {0x07, 0x00, 0x00, 0x00, 0, 0, 0, 0, 0};

    if (!SendReport(device, keyDown, sizeof(keyDown), verbose)) {
        return false;
    }

    Sleep(20);
    return SendReport(device, keyUp, sizeof(keyUp), verbose);
}

} // namespace gvinput
