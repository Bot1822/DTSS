// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

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

// 全局资源，用于时间片轮转，指示时间片状态
// 如：1表示允许提交渲染命令，0表示不允许等
int tsFlag = 1; // Time Slice Flag
SRWLOCK tsFlagLock = SRWLOCK_INIT; // 用于保护tsFlag的读写
// 时间片数量
int gameTsNum = 4;
int otherTsNum = 12;

static uint64_t* g_pD3D12DeviceVTable;
static uint64_t* g_pD3D12CommandQueueVTable;
static uint64_t* g_pDXGISwapChainVTable;

HANDLE hTimerQueue = NULL;
HANDLE hTimer = NULL;

// 用于保存原始的ExecuteCommandLists与Present方法
typedef void(WINAPI* pfnExecuteCommandLists)(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
pfnExecuteCommandLists pfnExecuteCommandListsOrig = NULL;
typedef HRESULT(WINAPI* pfnPresent)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
pfnPresent pfnPresentOrig = NULL;

bool initDX12VTable()
{
    // 创建一个窗口，用于初始化DX12的VTable？？
    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = DefWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.hIcon = NULL;
    windowClass.hCursor = NULL;
    windowClass.hbrBackground = NULL;
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = TEXT("dx12");
    windowClass.hIconSm = NULL;

    ::RegisterClassEx(&windowClass);

    HWND window = ::CreateWindow(windowClass.lpszClassName, TEXT("DirectX Window"), WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);

    HMODULE hD3D12 = ::GetModuleHandle(TEXT("d3d12.dll"));
    HMODULE hDXGI = ::GetModuleHandle(TEXT("dxgi.dll"));
    if (hD3D12 == NULL || hDXGI == NULL)
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "GetModuleHandleA d3d12.dll or dxgi.dll failed!", "Error", MB_OK);
        return false;
    }

    void* pD3D12CreateDevice = GetProcAddress(hD3D12, "D3D12CreateDevice");
    if (pD3D12CreateDevice == NULL)
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "GetProcAddress D3D12CreateDevice failed!", "Error", MB_OK);
        return false;
    }

    void* pDXGICreateFactory = GetProcAddress(hDXGI, "CreateDXGIFactory");
    if (pDXGICreateFactory == NULL)
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "GetProcAddress CreateDXGIFactory failed!", "Error", MB_OK);
        return false;
    }

    IDXGIFactory* pFactory;
    HRESULT hr = ((HRESULT(__stdcall*)(const IID&, void**))(pDXGICreateFactory))(__uuidof(IDXGIFactory), (void**)&pFactory);
    if (FAILED(hr))
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "CreateDXGIFactory failed!", "Error", MB_OK);
        return false;
    }

    IDXGIAdapter* pAdapter = NULL;
    hr = pFactory->EnumAdapters(0, &pAdapter);
    if (FAILED(hr))
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "EnumAdapters failed!", "Error", MB_OK);
        return false;
    }

    // 获取D3D12Device的VTable
    ID3D12Device* pDevice = NULL;
    hr = ((HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**))pD3D12CreateDevice)(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice));
    if (FAILED(hr))
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "D3D12CreateDevice failed!", "Error", MB_OK);
        return false;
    }

    g_pD3D12DeviceVTable = (uint64_t*)::calloc(50, sizeof(uint64_t));
    ::memcpy((void*)g_pD3D12DeviceVTable, *(void**)pDevice, 44 * sizeof(uint64_t));

    // 获取CommandQueue的VTable
    ID3D12CommandQueue* pCommandQueue = NULL;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    hr = pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&pCommandQueue));
    if (FAILED(hr))
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "CreateCommandQueue failed!", "Error", MB_OK);
        return false;
    }

    g_pD3D12CommandQueueVTable = (uint64_t*)::calloc(50, sizeof(uint64_t));
    ::memcpy((void*)g_pD3D12CommandQueueVTable, *(void**)pCommandQueue, 19 * sizeof(uint64_t));

    // 获取DXGISwapChain的VTable
    IDXGISwapChain* pSwapChain = NULL;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width = 1;
    swapChainDesc.BufferDesc.Height = 1;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = GetForegroundWindow();
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    hr = pFactory->CreateSwapChain(pCommandQueue, &swapChainDesc, &pSwapChain);
    if (FAILED(hr))
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "CreateSwapChain failed!", "Error", MB_OK);
        return false;
    }

    g_pDXGISwapChainVTable = (uint64_t*)::calloc(50, sizeof(uint64_t));
    ::memcpy((void*)g_pDXGISwapChainVTable, *(void**)pSwapChain, 18 * sizeof(uint64_t));

    writeLog("initDX12VTable done");
    writeLog("ExecuteCommandLists: " + std::to_string((uint64_t)g_pD3D12CommandQueueVTable[10]));
    writeLog("Present: " + std::to_string((uint64_t)g_pDXGISwapChainVTable[8]));

    pFactory->Release();
    pAdapter->Release();
    pDevice->Release();
    pCommandQueue->Release();
    pSwapChain->Release();

    return true;
}


// Hook后的ExecuteCommandLists方法
void WINAPI hkExecuteCommandListsHook(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
    writeLog("IN hkExecuteCommandListsHook");
    // 当tsflag为0时，不允许提交渲染命令
    if (getTsFlag() == 0)
    {
        writeLog("tsFlag: " + std::to_string(getTsFlag()));
        return;
    }
}

// Hook后的Present方法
HRESULT WINAPI hkPresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    writeLog("IN hkPresentHook");
    // 调用原始的Present方法
    HRESULT hr = pfnPresentOrig(pSwapChain, SyncInterval, Flags);
    return hr;
}


// Hook DX12 VTable的线程
HANDLE hThread_HookDX12VTable = NULL;
DWORD WINAPI hookDX12VTable()
{
    if (!initDX12VTable())
    {
        MessageBoxA(NULL, "initDX12VTable failed!", "Error", MB_OK);
        return 1;
    }

    if (MH_Initialize() != MH_OK) {
        MessageBoxA(NULL, "MH_Initialize failed!", "Error", MB_OK);
        return 1;
    }
    else {
        writeLog("MH_Initialize done");
    }

    // Hook D3D12Device的ExecuteCommandLists方法
    if (MH_CreateHook((void*)g_pD3D12CommandQueueVTable[10], hkExecuteCommandListsHook, (void**)&pfnExecuteCommandListsOrig) != MH_OK) {
        MessageBoxA(NULL, "MH_CreateHook ExecuteCommandLists failed!", "Error", MB_OK);
        return 1;
    }
    else {
        writeLog("MH_CreateHook ExecuteCommandLists done");
    }
    // Hook DXGISwapChain的Present方法
    if (MH_CreateHook((void*)g_pDXGISwapChainVTable[8], hkPresentHook, (void**)&pfnPresentOrig) != MH_OK) {
        MessageBoxA(NULL, "MH_CreateHook Present failed!", "Error", MB_OK);
        return 1;
    }
    else {
        writeLog("MH_CreateHook Present done");
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        MessageBoxA(NULL, "MH_EnableHook failed!", "Error", MB_OK);
        return 1;
    }
    else {
        writeLog("MH_EnableHook done");
    }

    writeLog("DX12 Hook thread done");
    return 0;
}

void unhookDX12VTable()
{
    if (MH_DisableHook(MH_ALL_HOOKS)) {
        MessageBoxA(NULL, "MH_DisableHook failed!", "Error", MB_OK);
    }
    else {
        writeLog("MH_DisableHook done");
    }
    
    if (MH_Uninitialize()) {
        MessageBoxA(NULL, "MH_Uninitialize failed!", "Error", MB_OK);
    }
    else {
        writeLog("MH_Uninitialize done");
    }
    writeLog("unhookDX12VTable done");
    return;
}





void setTsFlag(int flag)
{
    AcquireSRWLockExclusive(&tsFlagLock);
    tsFlag = flag;
    ReleaseSRWLockExclusive(&tsFlagLock);
}
int getTsFlag()
{
    AcquireSRWLockShared(&tsFlagLock);
    int flag = tsFlag;
    ReleaseSRWLockShared(&tsFlagLock);
    return flag;
}


// HANDLE hThread_Timer = NULL;
// // 一个全局计时器线程，用于切换时间片状态
// DWORD WINAPI timerThread(LPVOID lpParam)
// {
//     while (WaitForSingleObject((HANDLE)lpParam, 0) != WAIT_OBJECT_0)
//     {
//         writeLog("SingleObject: " + std::to_string(WaitForSingleObject((HANDLE)lpParam, 0)));
//         // // 系统定时器精度为1ms，一整个周期为16ms，切分这个周期
//         // if (TsNum == gameTsNum) // 不切换时间片
//         // {
//         //     writeLog("keepTsFlag" + std::to_string(getTsFlag()));
//         //     Sleep(TsNum);
//         // }
//         // else // 切换时间片
//         // {
//         //     writeLog("setTsFlag");
//         //     setTsFlag(1);
//         //     Sleep(gameTsNum);
//         //     setTsFlag(0);
//         //     Sleep(TsNum - gameTsNum);
//         // }
//         Sleep(16);
//     } 
//     // Sleep(100);
//     writeLog("SingleObject: " + std::to_string(WaitForSingleObject((HANDLE)lpParam, 0)));
//     writeLog("timerThread Exit");
//     return 0;
// }

void CALLBACK timerThread(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    writeLog("IN timerThread");
    // 系统定时器精度为1ms，一整个周期为16ms，切分这个周期
    if(getTsFlag() == 1)
    {
        setTsFlag(0);
        writeLog("Do other works, tsFlag: " + std::to_string(getTsFlag()));
        if (ChangeTimerQueueTimer(hTimerQueue, hTimer, otherTsNum, 1))
        {
            writeLog("ChangeTimerQueueTimer: " + std::to_string(otherTsNum));
        }
        else
        {
            writeLog("ChangeTimerQueueTimer failed");
        }
        
    }
    else if(getTsFlag() == 0)
    {
        setTsFlag(1);
        writeLog("Do game works, tsFlag: " + std::to_string(getTsFlag()));
        if (ChangeTimerQueueTimer(hTimerQueue, hTimer, gameTsNum, 1))
        {
            writeLog("ChangeTimerQueueTimer: " + std::to_string(gameTsNum));
        }
        else
        {
            writeLog("ChangeTimerQueueTimer failed");
        }
    }
    else
    {
        writeLog("tsFlag error");
    }
    writeLog("timerThread Exit");
}

HANDLE g_hExitEvent; // 退出事件句柄

void InitiateThreads() {
    g_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // 创建退出事件

    // // 创建Timer线程，传递退出事件的句柄
    // hThread_Timer = CreateThread(NULL, 0, timerThread, (LPVOID)g_hExitEvent, 0, NULL);
    
    hTimerQueue = CreateTimerQueue();
    if (hTimerQueue == NULL) {
        MessageBoxA(NULL, "CreateTimerQueue failed!", "Error", MB_OK);
        return;
    }
    else {
        writeLog("CreateTimerQueue done");
    }

    if (!CreateTimerQueueTimer(&hTimer, hTimerQueue, (WAITORTIMERCALLBACK)timerThread, (LPVOID)g_hExitEvent, 0, 1, WT_EXECUTEINTIMERTHREAD)) {
        MessageBoxA(NULL, "CreateTimerQueueTimer failed!", "Error", MB_OK);
        return;
    }
    else {
        writeLog("CreateTimerQueueTimer done");
    }


    // 创建DX12 VTable Hook线程，该线程不需要传递参数
    hThread_HookDX12VTable = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hookDX12VTable, NULL, 0, NULL);
}

void CleanupThreads() {
    SetEvent(g_hExitEvent); // 触发退出事件

    // 等待DX12 VTable Hook线程退出
    if (hThread_HookDX12VTable != NULL) {
        WaitForSingleObject(hThread_HookDX12VTable, INFINITE);
        DWORD lpExitCode;
        GetExitCodeThread(hThread_HookDX12VTable, &lpExitCode);
        writeLog("hThread_HookDX12VTable exit code: " + std::to_string(lpExitCode));
        CloseHandle(hThread_HookDX12VTable);
        hThread_HookDX12VTable = NULL;
        writeLog("hThread_HookDX12VTable has exited");
    }

    // 销毁timer
    if (hTimer != NULL) {
        HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
        DeleteTimerQueueTimer(hTimerQueue, hTimer, event);
        WaitForSingleObject(event, INFINITE);
        writeLog("DeleteTimerQueueTimer done");
        CloseHandle(event);
        hTimer = NULL;
        writeLog("hTimer has exited");
    }
    if (hTimerQueue != NULL) {
        DeleteTimerQueue(hTimerQueue);
        hTimerQueue = NULL;
        writeLog("hTimerQueue has exited");
    }


    // 重置退出事件以备后用
    ResetEvent(g_hExitEvent); 
}


BOOL WINAPI DllMain(HINSTANCE hInstance,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hInstance);
        
        // 设置系统定时器精度
        timeBeginPeriod(1); 

        //启用日志
        logFile = std::ofstream("C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\log\\gamehook.log", std::ios::out | std::ios::trunc);
        if (!logFile.is_open())
        {
            std::cout << "log file open failed" << std::endl;
            exit(1);
        }
        else {
            writeLog("Log begin time: " + std::to_string(timeGetTime()));
        }

        InitiateThreads();
        break;
    case DLL_PROCESS_DETACH:
        unhookDX12VTable();

        CleanupThreads();

        
        // 恢复系统定时器精度
        timeEndPeriod(1);
        writeLog("Log end time: " + std::to_string(timeGetTime()));
        logFile.close();
        MessageBoxA(NULL, "DLL_PROCESS_DETACH", "Notice", MB_OK);
        break;
    }
    return TRUE;
}

