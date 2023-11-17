// 将动态库注入游戏进程

#include<iostream>
#include<Windows.h>
#include<tlhelp32.h>
#include<fstream>
#include<string>
#include<sstream>
#include<comdef.h>
#include<timeapi.h>
#pragma comment(lib, "winmm" )
#include <psapi.h>
#include <tchar.h>

// 日志文件
#define logEnabled true
std::ofstream logFile;
// 写日志方法
template<typename T>
void writeLog(const T& value)
{
    if (!logEnabled)
        return;
    std::stringstream ss;
    ss << value;
    logFile << ss.str() << std::endl;
}

// 用于保存注入的DLL句柄
HMODULE hDLL = NULL;

class VirtualMemory
{
private:
    LPVOID m_lpBaseAddress;
public:
    const HANDLE process;
	const SIZE_T size;
	const DWORD protectFlag;
    explicit VirtualMemory(HANDLE hProcess, SIZE_T dwSize, DWORD flProtect) :
		process(hProcess), size(dwSize), protectFlag(flProtect) 
    {
        m_lpBaseAddress = VirtualAllocEx(process, NULL, size, MEM_COMMIT, protectFlag);
        if (m_lpBaseAddress == NULL)
        {
            std::cout << "VirtualAllocEx failed" << std::endl;
            exit(1);
        }
    }
    ~VirtualMemory()
    {
        writeLog("IN ~VirtualMemory");
        if (m_lpBaseAddress != NULL)
            VirtualFreeEx(process, m_lpBaseAddress, 0, MEM_RELEASE);
    }
    LPVOID getBaseAddress() const
    {
        return m_lpBaseAddress;
    }

    void write(const void* value) const
    {
        SIZE_T written;
        if (!WriteProcessMemory(process, m_lpBaseAddress, value, size, &written))
        {
            std::cout << "WriteProcessMemory failed" << std::endl;
            writeLog("WriteProcessMemory failed");
            exit(1);
        }
        return;
    }
};

HANDLE hGameProcess;
std::string dllPath;
std::string gameName;


// 启用调试特权
bool EnableDebugPrivilege()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        std::cout << "OpenProcessToken failed" << std::endl;
        return false;
    }
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid))
    {
        std::cout << "LookupPrivilegeValue failed" << std::endl;
        CloseHandle(hToken);
        return false;
    }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL))
    {
        std::cout << "AdjustTokenPrivileges failed" << std::endl;
        return false;
    }
    CloseHandle(hToken);
    return true;
}

// 根据进程名获取进程ID
DWORD GetProcessIdByName(const std::string& name)
{
    writeLog("IN GetProcessIdByName: " + name);
    // 获取进程快照
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        std::cout << "CreateToolhelp32Snapshot failed" << std::endl;
        writeLog("CreateToolhelp32Snapshot failed");
        return 0;
    }
    // 遍历进程快照
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(hSnapshot, &pe))
    {
        std::cout << "Process32First failed" << std::endl;
        writeLog("Process32First failed");
        CloseHandle(hSnapshot);
        return 0;
    }
    do
    {
        _bstr_t bstrName(pe.szExeFile);
        if (strcmp(name.c_str(), (const char*)bstrName) == 0)
        {
            std::cout << "Found process: " << name << std::endl;
            writeLog("Found process: " + name);
            writeLog("ProcessID: " + std::to_string(pe.th32ProcessID));
            CloseHandle(hSnapshot);
            return pe.th32ProcessID;
        }
    } while (Process32Next(hSnapshot, &pe));
    std::cout << "Process not found: " << name << std::endl;
    writeLog("Process not found: " + name);
    CloseHandle(hSnapshot);
    return 0;
}

// 注入
bool Inject(HANDLE& hProcess, const std::string& dllPath)
{
    writeLog("IN Inject: " + dllPath);
    // 申请内存
    VirtualMemory dllPathMemory(hProcess, dllPath.size() + 1, PAGE_READWRITE);
    // 写入dll路径
    dllPathMemory.write(dllPath.c_str());
    // 获取LoadLibraryA地址
    HMODULE hKernel32 = GetModuleHandleA("Kernel32.dll");
    if (hKernel32 == NULL) {
        std::cout << "GetModuleHandleA failed" << std::endl;
        writeLog("GetModuleHandleA failed");
        return false;
    }
    else {
        writeLog("GetModuleHandleA success: " + std::to_string((DWORD)hKernel32));
    }
    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");
    if (pLoadLibraryA == NULL)
    {
        std::cout << "GetProcAddress LoadLibraryA failed" << std::endl;
        writeLog("GetProcAddress LoadLibraryA failed");
        return false;
    }
    else {
        writeLog("GetProcAddress LoadLibraryA success: " + std::to_string((DWORD)pLoadLibraryA));
    }
    // 创建远程线程
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryA, dllPathMemory.getBaseAddress(), CREATE_SUSPENDED, NULL);
    if (hRemoteThread == NULL)
    {
        std::cout << "CreateRemoteThread failed" << std::endl;
        writeLog("CreateRemoteThread failed");
        return false;
    }
    else {
        writeLog("CreateRemoteThread success: " + std::to_string((DWORD)hRemoteThread));
    }
    MessageBoxA(NULL, "Press OK to resume romote thread/Load DLL", "Resume Thread", MB_OK);
    ResumeThread(hRemoteThread);
    // 等待线程结束
    WaitForSingleObject(hRemoteThread, INFINITE);
    // 记录下载入的DLL句柄
    GetExitCodeThread(hRemoteThread, (LPDWORD)&hDLL);
    // 关闭线程
    CloseHandle(hRemoteThread);

    return true;
}

//  卸载注入的DLL
bool eject(HANDLE hProcess)
{
    writeLog("IN eject: " + dllPath);

    HANDLE hDLLLocal = hDLL;

    // 获取FreeLibrary地址
    HMODULE hKernel32 = GetModuleHandleA("Kernel32.dll");
    if (hKernel32 == NULL) {
        std::cout << "GetModuleHandleA failed" << std::endl;
        writeLog("GetModuleHandleA failed");
        return false;
    }
    else {
        writeLog("GetModuleHandleA success: " + std::to_string((DWORD)hKernel32));
    }
    FARPROC pFreeLibrary = GetProcAddress(hKernel32, "FreeLibrary");
    if (pFreeLibrary == NULL)
    {
        std::cout << "GetProcAddress FreeLibrary failed" << std::endl;
        writeLog("GetProcAddress FreeLibrary failed");
        return false;
    }
    else {
        writeLog("GetProcAddress FreeLibrary success: " + std::to_string((DWORD)pFreeLibrary));
    }
    


    HMODULE hMods[1024];
    DWORD cbNeeded;
    unsigned int i;

    if(EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for(i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModName[MAX_PATH];
            // 获取模块的完整路径
            if(GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR))) {
                // 比较模块的名称是否为我们要卸载的DLL
                if(_tcsstr(szModName, TEXT("GameHook.dll"))) {
                    // 找到了要卸载的DLL，hMods[i]是其模块句柄
                    // 比较模块句柄是否与记录的DLL句柄相同
                    if (hDLLLocal != hMods[i]) {
                        writeLog("hDLLLocal != hMods[i]");
                        writeLog("hDLLLocal: " + std::to_string((uintptr_t)hDLLLocal));
                        writeLog("hMods[i]: " + std::to_string((uintptr_t)hMods[i]));
                        hDLLLocal = hMods[i];
                    }
                    else {
                        writeLog("hDLLLocal == hMods[i]");
                        writeLog("hDLLLocal: " + std::to_string((uintptr_t)hDLLLocal));
                        writeLog("hMods[i]: " + std::to_string((uintptr_t)hMods[i]));
                    }
                    break;
                }
            }
        }
    }





    // 创建远程线程
    HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pFreeLibrary, hDLLLocal, CREATE_SUSPENDED, NULL);
    if (hRemoteThread == NULL)
    {
        std::cout << "CreateRemoteThread failed" << std::endl;
        writeLog("CreateRemoteThread failed");
        return false;
    }
    else {
        writeLog("CreateRemoteThread success: " + std::to_string((DWORD)hRemoteThread));
    }
    MessageBoxA(NULL, "Press OK to resume romote thread/Unload DLL", "Resume Thread", MB_OK);
    ResumeThread(hRemoteThread);
    // 等待线程结束
    WaitForSingleObject(hRemoteThread, INFINITE);
    // 关闭线程
    CloseHandle(hRemoteThread);
    return true;
}


// 窗口消息处理回调函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        switch (wParam)
        {
        case 1: // 注入
            writeLog("\nPress Inject Button");
            if(Inject(hGameProcess, dllPath)) {
                writeLog("Inject success");
            }
            else {
                writeLog("Inject failed");
            }
            break;
        case 2: // 卸载
            writeLog("\nPress Eject Button");
            if (eject(hGameProcess)) {
                writeLog("Eject success");
            }
            else {
                writeLog("Eject failed");
            }
            break;
        }
        break;
    case WM_DESTROY:
        writeLog("WM_DESTROY");
        eject(hGameProcess);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}
// 创建一个窗口，两个按钮，一个注入，一个卸载
DWORD WINAPI createWindow(HANDLE hGameProcess, const std::string& dllPath)
{
    TCHAR szClassName[] = TEXT("Controller");
    TCHAR szWindowName[] = TEXT("Injector");
    WNDCLASS wndclass;
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = NULL;
    wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szClassName;
    if (!RegisterClass(&wndclass))
    {
        std::cout << "RegisterClass failed" << std::endl;
        writeLog("RegisterClass failed");
        return -1;
    }
    HWND hWnd = CreateWindow(szClassName, szWindowName, WS_OVERLAPPEDWINDOW, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    if (hWnd == NULL)
    {
        std::cout << "CreateWindow failed" << std::endl;
        writeLog("CreateWindow failed");
        return -1;
    }
    // 注入按钮
    HWND hButtonInject = CreateWindow(TEXT("BUTTON"), TEXT("Inject"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 50, 50, 100, 50, hWnd, (HMENU)1, NULL, NULL);
    if (hButtonInject == NULL)
    {
        std::cout << "CreateWindow hButtonInject failed" << std::endl;
        writeLog("CreateWindow hButtonInject failed");
        return -1;
    }
    // 卸载按钮
    HWND hButtonEject = CreateWindow(TEXT("BUTTON"), TEXT("Eject"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 50, 100, 50, hWnd, (HMENU)2, NULL, NULL);
    if (hButtonEject == NULL)
    {
        std::cout << "CreateWindow hButtonEject failed" << std::endl;
        writeLog("CreateWindow hButtonEject failed");
        return -1;
    }
    // 显示窗口
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        // 翻译消息
        TranslateMessage(&msg);
        // 分发消息
        DispatchMessage(&msg);
    }

    return msg.wParam;
}



int main(int argc, char* argv[])
{
    // 启用日志
    logFile.open("C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\log\\injector.log");
    writeLog("Log begin time: " + std::to_string(timeGetTime()));
    if (!EnableDebugPrivilege())
    {
        std::cout << "EnableDebugPrivilege failed" << std::endl;
        writeLog("EnableDebugPrivilege failed");
        return 1;
    }
    // 游戏进程名、动态库路径从命令行参数获取
    // 例如 injector.exe game.exe path/to/dll.dll
    if (argc != 3)
    {
        writeLog("argc != 3\nUsage: injector.exe game.exe dll.dll");
        return 1;
    }
    gameName = argv[1];
    dllPath = argv[2];
    writeLog("gameName: " + gameName);
    writeLog("dllPath: " + dllPath);

    // 获取游戏进程ID
    DWORD pid = GetProcessIdByName(gameName);
    if (pid == 0)
    {
        MessageBox(NULL, TEXT("GetProcessIdByName failed"), TEXT("Error"), MB_OK);
        writeLog("GetProcessIdByName failed");
        return 1;
    }
    // 打开游戏进程
    hGameProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hGameProcess == NULL)
    {
        MessageBox(NULL, TEXT("OpenProcess failed"), TEXT("Error"), MB_OK);
        writeLog("OpenProcess failed");
        return 1;
    }
    // 创建窗口
    if (createWindow(hGameProcess, dllPath) != 0)
    {
        MessageBox(NULL, TEXT("createWindow failed"), TEXT("Error"), MB_OK);
        writeLog("createWindow failed");
        return 1;
    }
    // 关闭游戏进程
    CloseHandle(hGameProcess);
    // 关闭日志
    writeLog("Log end time: " + std::to_string(timeGetTime()));
    logFile.close();

    return 0;
}

