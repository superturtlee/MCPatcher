#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <fstream>
#include <cctype>

using namespace std;

// 通过进程名查找进程ID
DWORD FindProcessId(const string& processName) {
    DWORD processId = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            char narrowExeName[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, pe32.szExeFile, -1, narrowExeName, MAX_PATH, NULL, NULL);
            if (_stricmp(narrowExeName, processName.c_str()) == 0) {
                processId = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return processId;
}

// 检查文件是否存在
bool FileExists(const string& path) {
    ifstream file(path);
    return file.good();
}

// 执行DLL注入
bool InjectDLL(DWORD processId, const string& dllPath) {
    cout << "[+] Opening target process..." << endl;

    // 打开目标进程
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        cerr << "[-] Failed to open process. Error: " << GetLastError() << endl;
        return false;
    }

    cout << "[+] Allocating memory in target process..." << endl;

    // 在目标进程中分配内存
    SIZE_T pathLen = dllPath.length() + 1;
    LPVOID pRemotePath = VirtualAllocEx(hProcess, NULL, pathLen,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pRemotePath == NULL) {
        cerr << "[-] Failed to allocate memory. Error: " << GetLastError() << endl;
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] Writing DLL path to target process..." << endl;

    // 写入DLL路径到目标进程
    if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathLen, NULL)) {
        cerr << "[-] Failed to write memory. Error: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] Getting LoadLibraryA address..." << endl;

    // 获取LoadLibraryA的地址
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryA");

    if (pLoadLibrary == NULL) {
        cerr << "[-] Failed to get LoadLibraryA address." << endl;
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] Creating remote thread..." << endl;

    // 在目标进程中创建远程线程执行LoadLibraryA
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary,
        pRemotePath, 0, NULL);
    if (hThread == NULL) {
        cerr << "[-] Failed to create remote thread. Error: " << GetLastError() << endl;
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    cout << "[+] DLL injected successfully!" << endl;
    cout << "[+] Waiting for remote thread to complete..." << endl;

    // 等待远程线程完成
    WaitForSingleObject(hThread, INFINITE);

    cout << "[+] Remote thread completed!" << endl;

    // 清理
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

// 判断字符串是否为数字
bool IsNumeric(const string& str) {
    for (char c : str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return !str.empty();
}

int main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "       DLL Injector      " << endl;
    cout << "========================================" << endl << endl;
    string target;
    string dllPath;
    bool dump = 0;
    if (argc < 2) {
		cout << "Runtime Mem Patch Mode" << endl;
        target = "Minecraft.Windows.exe";
        dllPath = "MCpatcher2.dll";
		system("start purchase.html");
    }
    else {
        cout << "Dump Game Mode" << endl;
        target = "Minecraft.Windows.exe";
        dllPath = "FileCopy.dll";
        dump = 1;
    }
    

    // 检查DLL文件是否存在
    cout << "[*] Checking DLL file..." << endl;
    if (!FileExists(dllPath)) {
        cerr << "[-] DLL file not found: " << dllPath << endl;
        return 1;
    }

    // 获取DLL的完整路径
    char fullDllPath[MAX_PATH];
    if (!GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullDllPath, NULL)) {
        cerr << "[-] Failed to get full path of DLL." << endl;
        return 1;
    }

    cout << "[+] DLL Path: " << fullDllPath << endl << endl;

    // 判断目标是进程名还是PID
    DWORD processId = 0;

	processId = 0;

    while (processId == 0) {
        processId = FindProcessId(target);
        if (dump) {
            Sleep(2000);
        }
    }

    cout << endl << "[*] Starting injection process..." << endl << endl;

    // 执行注入
    if(!dump)
    if (InjectDLL(processId, fullDllPath)) {
        cout << endl << "========================================" << endl;
        cout << "[SUCCESS] Injection completed!" << endl;
        cout << "========================================" << endl;
		return 0;
    }
    else {
        cerr << endl << "========================================" << endl;
        cerr << "[FAILED] Injection failed!" << endl;
        cerr << "========================================" << endl;
		return 1;
    }
    if(dump)
    if (InjectDLL(processId, fullDllPath)) {
        cout << endl << "========================================" << endl;
        cout << "[SUCCESS] Injection completed!" << endl;
        cout << "========================================" << endl;
        cout << "Wating the target process to exit..." << endl;
        HANDLE hrProcess = OpenProcess(SYNCHRONIZE, FALSE, processId);
        WaitForSingleObject(hrProcess, INFINITE);
        CloseHandle(hrProcess);
        return 0;
    }
    else {
        cerr << endl << "========================================" << endl;
        cerr << "[FAILED] Injection failed!" << endl;
        cerr << "========================================" << endl;
        return 1;
    }
}
