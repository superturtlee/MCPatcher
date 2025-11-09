#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <mutex>
#include <Wbemidl.h>
#include <comdef.h>
#include <comutil.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "comsuppw.lib")

// 服务相关
SERVICE_STATUS g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

// 线程安全的日志
std::mutex g_LogMutex;

// 日志函数
void LogMessage(const std::wstring& message) {
    std::lock_guard<std::mutex> lock(g_LogMutex);
    try {
        std::wofstream logFile(L"C:\\ProgramData\\MinecraftPatcher\\service.log", std::ios::app);
        if (logFile.is_open()) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t timeBuffer[64];
            swprintf_s(timeBuffer, L"[%04d-%02d-%02d %02d:%02d:%02d]",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            logFile << timeBuffer << L" " << message << std::endl;
            logFile.close();
        }
    }
    catch (...) {
        // 忽略日志错误，避免影响服务运行
    }
}

// 获取文件版本
bool GetFileVersion(const std::wstring& filePath, DWORD& major, DWORD& minor, DWORD& build, DWORD& revision) {
    DWORD dwHandle = 0;
    DWORD dwSize = GetFileVersionInfoSizeW(filePath.c_str(), &dwHandle);
    if (dwSize == 0) {
        DWORD error = GetLastError();
        LogMessage(L"GetFileVersionInfoSize failed: " + std::to_wstring(error) + L" for " + filePath);
        return false;
    }

    std::vector<BYTE> buffer(dwSize);
    if (!GetFileVersionInfoW(filePath.c_str(), dwHandle, dwSize, buffer.data())) {
        LogMessage(L"GetFileVersionInfo failed: " + std::to_wstring(GetLastError()));
        return false;
    }

    VS_FIXEDFILEINFO* pFileInfo = NULL;
    UINT len = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", (LPVOID*)&pFileInfo, &len)) {
        LogMessage(L"VerQueryValue failed: " + std::to_wstring(GetLastError()));
        return false;
    }

    if (len == 0 || pFileInfo == NULL) {
        LogMessage(L"Invalid version info structure");
        return false;
    }

    major = HIWORD(pFileInfo->dwFileVersionMS);
    minor = LOWORD(pFileInfo->dwFileVersionMS);
    build = HIWORD(pFileInfo->dwFileVersionLS);
    revision = LOWORD(pFileInfo->dwFileVersionLS);

    return true;
}

// 检查版本是否 >= 1.21.120.0
bool IsVersionValid(DWORD major, DWORD minor, DWORD build, DWORD revision) {
    if (major > 1) return true;
    if (major < 1) return false;

    if (minor > 21) return true;
    if (minor < 21) return false;

    if (build > 120) return true;
    if (build < 120) return false;

    return revision >= 0;
}

// DLL 注入函数
bool InjectDLL(DWORD processId, const std::wstring& dllPath) {
    LogMessage(L"Starting injection for PID: " + std::to_wstring(processId));

    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, processId);
    if (!hProcess) {
        DWORD error = GetLastError();
        LogMessage(L"Failed to open process: " + std::to_wstring(error));
        return false;
    }

    // 检查进程是否还在运行
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
        LogMessage(L"Process already terminated");
        CloseHandle(hProcess);
        return false;
    }

    size_t dllPathSize = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID pRemoteMemory = VirtualAllocEx(hProcess, NULL, dllPathSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pRemoteMemory) {
        DWORD error = GetLastError();
        LogMessage(L"Failed to allocate memory in target process: " + std::to_wstring(error));
        CloseHandle(hProcess);
        return false;
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteMemory, dllPath.c_str(), dllPathSize, &bytesWritten)) {
        DWORD error = GetLastError();
        LogMessage(L"Failed to write DLL path to target process: " + std::to_wstring(error));
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LogMessage(L"Wrote " + std::to_wstring(bytesWritten) + L" bytes to remote process");

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        LogMessage(L"Failed to get kernel32.dll handle");
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LPTHREAD_START_ROUTINE pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryW");
    if (!pLoadLibraryW) {
        LogMessage(L"Failed to get LoadLibraryW address");
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    DWORD threadId = 0;
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, pLoadLibraryW, pRemoteMemory, 0, &threadId);

    if (!hThread) {
        DWORD error = GetLastError();
        LogMessage(L"Failed to create remote thread: " + std::to_wstring(error));
        VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    LogMessage(L"Created remote thread with ID: " + std::to_wstring(threadId));

    // 等待线程完成，最多等待 5 秒
    DWORD waitResult = WaitForSingleObject(hThread, 5000);

    DWORD threadExitCode = 0;
    GetExitCodeThread(hThread, &threadExitCode);

    if (waitResult == WAIT_TIMEOUT) {
        LogMessage(L"Remote thread timeout");
    }
    else if (threadExitCode == 0) {
        LogMessage(L"LoadLibrary returned NULL (failed to load DLL)");
    }
    else {
        LogMessage(L"LoadLibrary succeeded, module handle: " + std::to_wstring(threadExitCode));
    }

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemoteMemory, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    bool success = (waitResult == WAIT_OBJECT_0 && threadExitCode != 0);
    LogMessage(L"Injection " + std::wstring(success ? L"succeeded" : L"failed"));
    return success;
}

// 获取进程完整路径
std::wstring GetProcessPath(DWORD processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) {
        LogMessage(L"Failed to open process for path query: " + std::to_wstring(GetLastError()));
        return L"";
    }

    wchar_t path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;

    // 尝试使用 QueryFullProcessImageName (Vista+)
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        return std::wstring(path);
    }

    // 回退到 GetModuleFileNameEx
    if (GetModuleFileNameExW(hProcess, NULL, path, MAX_PATH) != 0) {
        CloseHandle(hProcess);
        return std::wstring(path);
    }

    DWORD error = GetLastError();
    LogMessage(L"Failed to get process path: " + std::to_wstring(error));
    CloseHandle(hProcess);
    return L"";
}

// 处理新进程
void HandleNewProcess(DWORD processId, const std::wstring& processName) {
    if (_wcsicmp(processName.c_str(), L"Minecraft.Windows.exe") != 0) {
        return;
    }

    LogMessage(L"========================================");
    LogMessage(L"Detected Minecraft.Windows.exe started, PID: " + std::to_wstring(processId));

    // 等待一小段时间让进程初始化
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::wstring exePath = GetProcessPath(processId);
    if (exePath.empty()) {
        LogMessage(L"WARNING: Failed to get process path, assuming high version and proceeding with injection");

        // DLL 路径
        std::wstring dllPath = L"C:\\ProgramData\\MinecraftPatcher\\MCpatcher2.dll";

        if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            LogMessage(L"ERROR: DLL not found: " + dllPath);
            return;
        }

        auto startTime = std::chrono::high_resolution_clock::now();
        bool success = InjectDLL(processId, dllPath);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (success) {
            LogMessage(L"Injection completed in " + std::to_wstring(duration.count()) + L"ms");
        }
        else {
            LogMessage(L"Injection failed");
        }
        return;
    }

    LogMessage(L"Process path: " + exePath);

    DWORD major = 0, minor = 0, build = 0, revision = 0;
    bool versionObtained = GetFileVersion(exePath, major, minor, build, revision);

    if (!versionObtained) {
        LogMessage(L"WARNING: Failed to get file version, assuming high version (>= 1.21.120.0) and proceeding");
        major = 1;
        minor = 21;
        build = 120;
        revision = 0;
    }
    else {
        std::wstring versionStr = std::to_wstring(major) + L"." + std::to_wstring(minor) +
            L"." + std::to_wstring(build) + L"." + std::to_wstring(revision);
        LogMessage(L"Version: " + versionStr);

        if (!IsVersionValid(major, minor, build, revision)) {
            LogMessage(L"Version is below 1.21.120.0, skipping injection");
            LogMessage(L"========================================");
            return;
        }
    }

    // DLL 路径
    std::wstring dllPath = L"C:\\ProgramData\\MinecraftPatcher\\MCpatcher2.dll";

    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        LogMessage(L"ERROR: DLL not found: " + dllPath);
        LogMessage(L"========================================");
        return;
    }

    LogMessage(L"DLL found: " + dllPath);

    // 在 100ms 内注入
    auto startTime = std::chrono::high_resolution_clock::now();
    bool success = InjectDLL(processId, dllPath);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (success) {
        LogMessage(L"SUCCESS: Injection completed in " + std::to_wstring(duration.count()) + L"ms");
    }
    else {
        LogMessage(L"FAILURE: Injection failed after " + std::to_wstring(duration.count()) + L"ms");
    }
    LogMessage(L"========================================");
}

// WMI 事件接收器实现
class ProcessEventSink : public IWbemObjectSink {
private:
    LONG m_lRef;
    bool* m_pStopFlag;

public:
    ProcessEventSink(bool* pStopFlag) : m_lRef(0), m_pStopFlag(pStopFlag) {}
    ~ProcessEventSink() {}

    virtual ULONG STDMETHODCALLTYPE AddRef() {
        return InterlockedIncrement(&m_lRef);
    }

    virtual ULONG STDMETHODCALLTYPE Release() {
        LONG lRef = InterlockedDecrement(&m_lRef);
        if (lRef == 0) {
            delete this;
        }
        return lRef;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) {
        if (riid == IID_IUnknown || riid == IID_IWbemObjectSink) {
            *ppv = (IWbemObjectSink*)this;
            AddRef();
            return WBEM_S_NO_ERROR;
        }
        return E_NOINTERFACE;
    }

    virtual HRESULT STDMETHODCALLTYPE Indicate(
        LONG lObjectCount,
        IWbemClassObject** apObjArray) {

        for (LONG i = 0; i < lObjectCount; i++) {
            try {
                VARIANT vtProp;
                VariantInit(&vtProp);

                // 获取 TargetInstance
                HRESULT hr = apObjArray[i]->Get(L"TargetInstance", 0, &vtProp, 0, 0);
                if (SUCCEEDED(hr) && vtProp.vt == VT_UNKNOWN) {
                    IWbemClassObject* pTargetInstance = NULL;
                    hr = vtProp.punkVal->QueryInterface(IID_IWbemClassObject, (void**)&pTargetInstance);

                    if (SUCCEEDED(hr) && pTargetInstance) {
                        VARIANT vtProcessId, vtProcessName;
                        VariantInit(&vtProcessId);
                        VariantInit(&vtProcessName);

                        // 获取进程 ID
                        hr = pTargetInstance->Get(L"ProcessId", 0, &vtProcessId, 0, 0);
                        if (SUCCEEDED(hr) && vtProcessId.vt == VT_I4) {
                            DWORD processId = vtProcessId.lVal;

                            // 获取进程名称
                            hr = pTargetInstance->Get(L"Name", 0, &vtProcessName, 0, 0);
                            if (SUCCEEDED(hr) && vtProcessName.vt == VT_BSTR) {
                                std::wstring processName = vtProcessName.bstrVal;

                                // 在新线程中处理，避免阻塞 WMI 事件
                                std::thread([processId, processName]() {
                                    HandleNewProcess(processId, processName);
                                    }).detach();
                            }
                        }

                        VariantClear(&vtProcessId);
                        VariantClear(&vtProcessName);
                        pTargetInstance->Release();
                    }
                }

                VariantClear(&vtProp);
            }
            catch (...) {
                LogMessage(L"Exception in WMI event handler");
            }
        }

        return WBEM_S_NO_ERROR;
    }

    virtual HRESULT STDMETHODCALLTYPE SetStatus(
        LONG lFlags,
        HRESULT hResult,
        BSTR strParam,
        IWbemClassObject* pObjParam) {

        if (lFlags == WBEM_STATUS_COMPLETE) {
            LogMessage(L"WMI event monitoring completed");
        }
        else if (lFlags == WBEM_STATUS_PROGRESS) {
            LogMessage(L"WMI event monitoring in progress");
        }

        return WBEM_S_NO_ERROR;
    }
};

// 使用 WMI 事件监听进程启动
void MonitorProcessCreationWMI() {
    LogMessage(L"Initializing WMI event monitoring");

    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) {
        LogMessage(L"Failed to initialize COM library: " + std::to_wstring(hres));
        return;
    }

    hres = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
    );

    if (FAILED(hres)) {
        LogMessage(L"Failed to initialize security: " + std::to_wstring(hres));
        CoUninitialize();
        return;
    }

    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc
    );

    if (FAILED(hres)) {
        LogMessage(L"Failed to create IWbemLocator: " + std::to_wstring(hres));
        CoUninitialize();
        return;
    }

    IWbemServices* pSvc = NULL;
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc
    );

    if (FAILED(hres)) {
        LogMessage(L"Failed to connect to WMI: " + std::to_wstring(hres));
        pLoc->Release();
        CoUninitialize();
        return;
    }

    LogMessage(L"Connected to ROOT\\CIMV2 WMI namespace");

    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );

    if (FAILED(hres)) {
        LogMessage(L"Failed to set proxy blanket: " + std::to_wstring(hres));
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    // 创建事件接收器
    bool stopFlag = false;
    ProcessEventSink* pSink = new ProcessEventSink(&stopFlag);
    pSink->AddRef();

    // 注册异步事件通知
    hres = pSvc->ExecNotificationQueryAsync(
        _bstr_t(L"WQL"),
        _bstr_t(L"SELECT * FROM __InstanceCreationEvent WITHIN 1 WHERE TargetInstance ISA 'Win32_Process'"),
        WBEM_FLAG_SEND_STATUS,
        NULL,
        pSink
    );

    if (FAILED(hres)) {
        LogMessage(L"Failed to execute WMI query: " + std::to_wstring(hres));
        pSink->Release();
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return;
    }

    LogMessage(L"WMI event monitoring started successfully");

    // 等待服务停止事件
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    LogMessage(L"Stopping WMI event monitoring");

    // 取消异步操作
    pSvc->CancelAsyncCall(pSink);

    pSink->Release();
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();

    LogMessage(L"WMI event monitoring stopped");
}


// 服务控制处理
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        LogMessage(L"Received SERVICE_CONTROL_STOP");

        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING) break;

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);
        break;

    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        break;

    default:
        break;
    }
}

// 服务主函数
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandlerW(L"MinecraftPatchService", ServiceCtrlHandler);
    if (!g_StatusHandle) {
        return;
    }

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // 创建停止事件
    g_ServiceStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_ServiceStopEvent) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // 创建日志目录
    CreateDirectoryW(L"C:\\ProgramData\\MinecraftPatcher", NULL);

    LogMessage(L"========================================");
    LogMessage(L"MinecraftPatchService starting...");
    LogMessage(L"Version: 1.0.0");
    LogMessage(L"========================================");

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    LogMessage(L"Service status set to RUNNING");

    // 启动进程监控 - 优先使用 WMI，失败则回退到轮询

    LogMessage(L"Attempting to start WMI-based monitoring");
    MonitorProcessCreationWMI();

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    LogMessage(L"========================================");
    LogMessage(L"MinecraftPatchService stopped");
    LogMessage(L"========================================");
}

int wmain(int argc, wchar_t* argv[]) {
    // 如果是命令行调试模式
    if (argc > 1 && _wcsicmp(argv[1], L"--debug") == 0) {
        CreateDirectoryW(L"C:\\ProgramData\\MinecraftPatcher", NULL);
        LogMessage(L"Debug mode started");

        g_ServiceStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

        std::wcout << L"Press Ctrl+C to stop..." << std::endl;

        MonitorProcessCreationWMI();
   

        CloseHandle(g_ServiceStopEvent);
        return 0;
    }

    SERVICE_TABLE_ENTRYW ServiceTable[] = {
        { (LPWSTR)L"MinecraftPatchService", (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
        { NULL, NULL }
    };

    if (StartServiceCtrlDispatcherW(ServiceTable) == FALSE) {
        DWORD error = GetLastError();
        LogMessage(L"StartServiceCtrlDispatcher failed: " + std::to_wstring(error));
        return error;
    }

    return 0;
}