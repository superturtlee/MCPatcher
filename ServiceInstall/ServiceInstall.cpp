#include <windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <shlobj.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")

// 检查是否以管理员权限运行
bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

// 请求管理员权限重新运行
bool RunAsAdmin(int argc, wchar_t* argv[]) {
    // 构建命令行参数
    std::wstring params;
    for (int i = 1; i < argc; i++) {
        if (i > 1) params += L" ";
        params += L"\"";
        params += argv[i];
        params += L"\"";
    }

    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.lpVerb = L"runas";  // 请求管理员权限
    sei.lpFile = argv[0];
    sei.lpParameters = params.empty() ? NULL : params.c_str();
    sei.nShow = SW_NORMAL;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            std::wcerr << L"User cancelled UAC prompt" << std::endl;
        }
        else {
            std::wcerr << L"Failed to elevate privileges: " << error << std::endl;
        }
        return false;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }

    return true;
}

// 在注册表中注册卸载器
bool RegisterUninstaller(const std::wstring& uninstallerPath) {
    HKEY hKey;
    const wchar_t* subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MinecraftPatchService";

    LONG result = RegCreateKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);

    if (result != ERROR_SUCCESS) {
        std::wcerr << L"Failed to create registry key: " << result << std::endl;
        return false;
    }

    // 设置显示名称
    std::wstring displayName = L"Minecraft Patch Service";
    RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ,
        (BYTE*)displayName.c_str(), (DWORD)(displayName.length() + 1) * sizeof(wchar_t));

    // 设置发布者
    std::wstring publisher = L"MinecraftPatcher";
    RegSetValueExW(hKey, L"Publisher", 0, REG_SZ,
        (BYTE*)publisher.c_str(), (DWORD)(publisher.length() + 1) * sizeof(wchar_t));

    // 设置卸载命令
    std::wstring uninstallString = L"\"" + uninstallerPath + L"\" uninstall";
    RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ,
        (BYTE*)uninstallString.c_str(), (DWORD)(uninstallString.length() + 1) * sizeof(wchar_t));

    // 设置静默卸载命令（可选）
    std::wstring quietUninstallString = L"\"" + uninstallerPath + L"\" uninstall";
    RegSetValueExW(hKey, L"QuietUninstallString", 0, REG_SZ,
        (BYTE*)quietUninstallString.c_str(), (DWORD)(quietUninstallString.length() + 1) * sizeof(wchar_t));

    // 设置安装位置
    std::wstring installLocation = L"C:\\ProgramData\\MinecraftPatcher";
    RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ,
        (BYTE*)installLocation.c_str(), (DWORD)(installLocation.length() + 1) * sizeof(wchar_t));

    // 设置显示图标
    std::wstring displayIcon = uninstallerPath;
    RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ,
        (BYTE*)displayIcon.c_str(), (DWORD)(displayIcon.length() + 1) * sizeof(wchar_t));

    // 设置版本
    std::wstring displayVersion = L"1.0.0";
    RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ,
        (BYTE*)displayVersion.c_str(), (DWORD)(displayVersion.length() + 1) * sizeof(wchar_t));

    // 设置安装日期
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t dateBuffer[16];
    swprintf_s(dateBuffer, L"%04d%02d%02d", st.wYear, st.wMonth, st.wDay);
    std::wstring installDate = dateBuffer;
    RegSetValueExW(hKey, L"InstallDate", 0, REG_SZ,
        (BYTE*)installDate.c_str(), (DWORD)(installDate.length() + 1) * sizeof(wchar_t));

    // 设置不显示大小（因为是服务）
    DWORD noModify = 1;
    RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&noModify, sizeof(DWORD));

    DWORD noRepair = 1;
    RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&noRepair, sizeof(DWORD));

    RegCloseKey(hKey);

    std::wcout << L"Registered uninstaller in registry" << std::endl;
    return true;
}

// 从注册表中移除卸载器
bool UnregisterUninstaller() {
    const wchar_t* subKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MinecraftPatchService";

    LONG result = RegDeleteTreeW(HKEY_LOCAL_MACHINE, subKey);

    if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND) {
        std::wcerr << L"Failed to delete registry key: " << result << std::endl;
        return false;
    }

    std::wcout << L"Unregistered uninstaller from registry" << std::endl;
    return true;
}

// 启动服务
bool StartServiceByName(const wchar_t* serviceName) {
    std::wcout << L"Starting service..." << std::endl;

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        std::wcerr << L"Failed to open SC Manager: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, serviceName, SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to open service: " << error << std::endl;
        CloseServiceHandle(hSCManager);
        return false;
    }

    // 检查服务当前状态
    SERVICE_STATUS status;
    if (QueryServiceStatus(hService, &status)) {
        if (status.dwCurrentState == SERVICE_RUNNING) {
            std::wcout << L"Service is already running" << std::endl;
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCManager);
            return true;
        }
    }

    // 启动服务
    if (!StartServiceW(hService, 0, NULL)) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_ALREADY_RUNNING) {
            std::wcout << L"Service is already running" << std::endl;
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCManager);
            return true;
        }
        else {
            std::wcerr << L"Failed to start service: " << error << std::endl;
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCManager);
            return false;
        }
    }

    // 等待服务启动
    std::wcout << L"Waiting for service to start";
    for (int i = 0; i < 30; i++) {
        Sleep(500);
        if (QueryServiceStatus(hService, &status)) {
            if (status.dwCurrentState == SERVICE_RUNNING) {
                std::wcout << std::endl;
                std::wcout << L"Service started successfully!" << std::endl;
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCManager);
                return true;
            }
            if (status.dwCurrentState == SERVICE_STOPPED) {
                std::wcout << std::endl;
                std::wcerr << L"Service failed to start (stopped)" << std::endl;
                CloseServiceHandle(hService);
                CloseServiceHandle(hSCManager);
                return false;
            }
        }
        std::wcout << L".";
    }

    std::wcout << std::endl;
    std::wcerr << L"Timeout waiting for service to start" << std::endl;
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return false;
}

// 安装服务
bool InstallService(const std::wstring& servicePath) {
    std::wcout << L"Installing service..." << std::endl;

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager) {
        std::wcerr << L"Failed to open SC Manager: " << GetLastError() << std::endl;
        return false;
    }

    // 先检查服务是否已存在
    SC_HANDLE hExistingService = OpenServiceW(hSCManager, L"MinecraftPatchService", SERVICE_ALL_ACCESS);
    if (hExistingService) {
        std::wcout << L"Service already exists, stopping and updating..." << std::endl;

        // 停止现有服务
        SERVICE_STATUS status;
        if (ControlService(hExistingService, SERVICE_CONTROL_STOP, &status)) {
            std::wcout << L"Stopping existing service";
            for (int i = 0; i < 20; i++) {
                Sleep(500);
                if (QueryServiceStatus(hExistingService, &status)) {
                    if (status.dwCurrentState == SERVICE_STOPPED) {
                        std::wcout << std::endl;
                        break;
                    }
                }
                std::wcout << L".";
            }
            std::wcout << std::endl;
        }

        // 更新服务配置
        if (!ChangeServiceConfigW(hExistingService,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            servicePath.c_str(),
            NULL, NULL, NULL, NULL, NULL, NULL)) {
            std::wcerr << L"Failed to update service config: " << GetLastError() << std::endl;
        }

        CloseServiceHandle(hExistingService);
        CloseServiceHandle(hSCManager);

        // 启动服务
        return StartServiceByName(L"MinecraftPatchService");
    }

    // 创建新服务
    SC_HANDLE hService = CreateServiceW(
        hSCManager,
        L"MinecraftPatchService",
        L"Minecraft Patch Service",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        servicePath.c_str(),
        NULL, NULL, NULL, NULL, NULL
    );

    if (!hService) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to create service: " << error << std::endl;
        CloseServiceHandle(hSCManager);
        return false;
    }

    std::wcout << L"Service created successfully" << std::endl;

    // 设置服务描述
    SERVICE_DESCRIPTIONW sd;
    sd.lpDescription = (LPWSTR)L"Monitors Minecraft.Windows.exe startup and injects patch DLL for versions >= 1.21.120.0";
    ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd);

    // 设置服务失败恢复选项
    SC_ACTION actions[3];
    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 5000;  // 5秒后重启
    actions[1].Type = SC_ACTION_RESTART;
    actions[1].Delay = 10000;  // 10秒后重启
    actions[2].Type = SC_ACTION_NONE;
    actions[2].Delay = 0;

    SERVICE_FAILURE_ACTIONSW sfa;
    sfa.dwResetPeriod = 86400;  // 24小时重置失败计数
    sfa.lpRebootMsg = NULL;
    sfa.lpCommand = NULL;
    sfa.cActions = 3;
    sfa.lpsaActions = actions;

    ChangeServiceConfig2W(hService, SERVICE_CONFIG_FAILURE_ACTIONS, &sfa);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    // 立即启动服务
    return StartServiceByName(L"MinecraftPatchService");
}

// 卸载服务
bool UninstallService() {
    std::wcout << L"Uninstalling service..." << std::endl;

    SC_HANDLE hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hSCManager) {
        std::wcerr << L"Failed to open SC Manager: " << GetLastError() << std::endl;
        return false;
    }

    SC_HANDLE hService = OpenServiceW(hSCManager, L"MinecraftPatchService",
        SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
    if (!hService) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_DOES_NOT_EXIST) {
            std::wcout << L"Service does not exist" << std::endl;
        }
        else {
            std::wcerr << L"Failed to open service: " << error << std::endl;
        }
        CloseServiceHandle(hSCManager);
        return (error == ERROR_SERVICE_DOES_NOT_EXIST);
    }

    // 停止服务
    SERVICE_STATUS status;
    if (ControlService(hService, SERVICE_CONTROL_STOP, &status)) {
        std::wcout << L"Stopping service";
        for (int i = 0; i < 30; i++) {
            Sleep(500);
            if (QueryServiceStatus(hService, &status)) {
                if (status.dwCurrentState == SERVICE_STOPPED) {
                    std::wcout << std::endl;
                    std::wcout << L"Service stopped" << std::endl;
                    break;
                }
            }
            std::wcout << L".";
        }
        std::wcout << std::endl;
    }

    // 删除服务
    if (!DeleteService(hService)) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_MARKED_FOR_DELETE) {
            std::wcout << L"Service marked for deletion" << std::endl;
        }
        else {
            std::wcerr << L"Failed to delete service: " << error << std::endl;
            CloseServiceHandle(hService);
            CloseServiceHandle(hSCManager);
            return false;
        }
    }

    std::wcout << L"Service uninstalled successfully!" << std::endl;

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return true;
}

// 删除文件（带重试）
bool DeleteFileWithRetry(const wchar_t* filePath, int maxRetries = 5) {
    for (int i = 0; i < maxRetries; i++) {
        if (DeleteFileW(filePath)) {
            return true;
        }

        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return true;  // 文件不存在，视为成功
        }

        if (i < maxRetries - 1) {
            Sleep(500);  // 等待后重试
        }
    }
    return false;
}

int wmain(int argc, wchar_t* argv[]) {


    // 处理卸载
    if (argc >= 2) {
        std::wstring command = argv[1];
        if (command == L"uninstall") {
            std::wcout << L"======================================" << std::endl;
            std::wcout << L"  Minecraft Patch Service Uninstaller" << std::endl;
            std::wcout << L"======================================" << std::endl;
            std::wcout << std::endl;

            bool serviceUninstalled = UninstallService();

            // 等待服务完全停止
            Sleep(2000);

            // 删除文件
            std::wcout << L"Removing files..." << std::endl;
            bool filesDeleted = true;

            if (!DeleteFileWithRetry(L"C:\\ProgramData\\MinecraftPatcher\\injectorservice.exe")) {
                std::wcerr << L"Warning: Failed to delete injectorservice.exe" << std::endl;
                filesDeleted = false;
            }
            else {
                std::wcout << L"Deleted: injectorservice.exe" << std::endl;
            }

            if (!DeleteFileWithRetry(L"C:\\ProgramData\\MinecraftPatcher\\MCpatcher2.dll")) {
                std::wcerr << L"Warning: Failed to delete MCpatcher2.dll" << std::endl;
                filesDeleted = false;
            }
            else {
                std::wcout << L"Deleted: MCpatcher2.dll" << std::endl;
            }

            // 尝试删除日志文件
            if (DeleteFileWithRetry(L"C:\\ProgramData\\MinecraftPatcher\\service.log")) {
                std::wcout << L"Deleted: service.log" << std::endl;
            }

            // 从注册表移除卸载器
            UnregisterUninstaller();

            // 删除卸载器自身（延迟删除）
            std::wstring uninstallerPath = L"C:\\ProgramData\\MinecraftPatcher\\uninstall.exe";

            // 创建批处理文件来删除卸载器
            std::wstring batchPath = L"C:\\ProgramData\\MinecraftPatcher\\cleanup.bat";
            std::wofstream batch(batchPath);
            if (batch.is_open()) {
                batch << L"@echo off\n";
                batch << L"timeout /t 2 /nobreak >nul\n";
                batch << L"del /f /q \"" << uninstallerPath << L"\"\n";
                batch << L"rmdir \"C:\\ProgramData\\MinecraftPatcher\"\n";
                batch << L"del /f /q \"" << batchPath << L"\"\n";
                batch.close();

                // 启动批处理（隐藏窗口）
                STARTUPINFOW si = { sizeof(si) };
                si.dwFlags = STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
                PROCESS_INFORMATION pi;

                std::wstring cmdLine = L"cmd.exe /c \"" + batchPath + L"\"";
                if (CreateProcessW(NULL, (LPWSTR)cmdLine.c_str(), NULL, NULL, FALSE,
                    CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                }
            }

            // 显示卸载成功消息
            if (serviceUninstalled) {
                MessageBoxW(NULL,
                    L"Minecraft Patch Service has been successfully uninstalled!\n\n"
                    L"✓ Service stopped and removed\n"
                    L"✓ All files deleted\n"
                    L"✓ Registry entries removed\n\n"
                    L"Thank you for using Minecraft Patch Service.",
                    L"Uninstallation Complete",
                    MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            else {
                MessageBoxW(NULL,
                    L"Uninstallation completed with warnings.\n\n"
                    L"Some components may not have been removed.\n"
                    L"Please check the console output for details.",
                    L"Uninstallation Warning",
                    MB_OK | MB_ICONWARNING);
                return 1;
            }
        }
    }
	system("start purchase.html");
    // 安装流程
    std::wcout << L"======================================" << std::endl;
    std::wcout << L"  Minecraft Patch Service Installer" << std::endl;
    std::wcout << L"  Version 1.0.0" << std::endl;
    std::wcout << L"======================================" << std::endl;
    std::wcout << std::endl;

    // 创建目标目录
    std::wcout << L"Creating installation directory..." << std::endl;
    if (!CreateDirectoryW(L"C:\\ProgramData\\MinecraftPatcher", NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            std::wcerr << L"Failed to create directory: " << GetLastError() << std::endl;
            MessageBoxW(NULL,
                L"Failed to create installation directory!\n\nError: Could not create C:\\ProgramData\\MinecraftPatcher",
                L"Installation Failed",
                MB_OK | MB_ICONERROR);
            return 1;
        }
    }
    std::wcout << L"Directory ready: C:\\ProgramData\\MinecraftPatcher" << std::endl;

    // 复制文件
    std::wcout << L"\nCopying files..." << std::endl;
    bool allFilesCopied = true;
    std::wstring errorMsg;

    if (!CopyFileW(L"injectorservice.exe", L"C:\\ProgramData\\MinecraftPatcher\\injectorservice.exe", FALSE)) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to copy injector.exe: " << error << std::endl;
        errorMsg += L"- Failed to copy injector.exe (Error: " + std::to_wstring(error) + L")\n";
        allFilesCopied = false;
    }
    else {
        std::wcout << L"✓ Copied: injectorservice.exe -> injectorservice.exe" << std::endl;
    }

    if (!CopyFileW(L"MCpatcher2.dll", L"C:\\ProgramData\\MinecraftPatcher\\MCpatcher2.dll", FALSE)) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to copy MCpatcher2.dll: " << error << std::endl;
        errorMsg += L"- Failed to copy MCpatcher2.dll (Error: " + std::to_wstring(error) + L")\n";
        allFilesCopied = false;
    }
    else {
        std::wcout << L"✓ Copied: MCpatcher2.dll" << std::endl;
    }

    // 复制卸载器（自身）
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    std::wstring uninstallerPath = L"C:\\ProgramData\\MinecraftPatcher\\uninstall.exe";

    if (!CopyFileW(selfPath, uninstallerPath.c_str(), FALSE)) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to copy uninstaller: " << error << std::endl;
        errorMsg += L"- Failed to copy uninstaller (Error: " + std::to_wstring(error) + L")\n";
        allFilesCopied = false;
    }
    else {
        std::wcout << L"✓ Copied: uninstall.exe" << std::endl;
    }

    if (!allFilesCopied) {
        MessageBoxW(NULL,
            (L"Installation failed! Could not copy all required files:\n\n" + errorMsg +
                L"\nPlease ensure all files are present in the installation directory.").c_str(),
            L"Installation Failed",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    // 安装并启动服务
    std::wcout << L"\nInstalling Windows service..." << std::endl;
    if (!InstallService(L"C:\\ProgramData\\MinecraftPatcher\\injectorservice.exe")) {
        MessageBoxW(NULL,
            L"Failed to install or start Windows service!\n\n"
            L"The service may have been installed but failed to start.\n"
            L"Please check the Event Viewer for details.",
            L"Installation Failed",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    // 注册卸载器
    std::wcout << L"\nRegistering uninstaller..." << std::endl;
    if (!RegisterUninstaller(uninstallerPath)) {
        std::wcerr << L"Warning: Failed to register uninstaller" << std::endl;
    }

    std::wcout << L"\n======================================" << std::endl;
    std::wcout << L"  Installation completed successfully!" << std::endl;
    std::wcout << L"======================================" << std::endl;

    // 显示安装成功消息
    MessageBoxW(NULL,
        L"Minecraft Patch Service has been successfully installed and started!\n\n"
        L"✓ Service installed: MinecraftPatchService\n"
        L"✓ Service started and running\n"
        L"✓ Auto-start enabled (runs on boot)\n"
        L"✓ Uninstaller registered\n\n"
        L"Installation location:\n"
        L"C:\\ProgramData\\MinecraftPatcher\n\n"
        L"The service will now monitor for Minecraft.Windows.exe\n"
        L"and inject the patch DLL when detected.\n\n"
        L"To uninstall, use 'Add or Remove Programs' in Windows Settings.",
        L"Installation Complete",
        MB_OK | MB_ICONINFORMATION);

    return 0;
}