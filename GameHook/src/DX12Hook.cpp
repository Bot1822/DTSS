#include "pch.h"
#include "DX12Hook.h"
#include "HookDef.h"
#include "Logger.h"
#include "MinHook.h"
#include "RenderStatus.h"

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

    // 获取Fence的VTable
    ID3D12Fence* pFence = NULL;
    hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
    if (FAILED(hr))
    {
        ::DestroyWindow(window);
        ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
        MessageBoxA(NULL, "CreateFence failed!", "Error", MB_OK);
        return false;
    }
    g_pD3D12FenceVTable = (uint64_t*)::calloc(50, sizeof(uint64_t));
    ::memcpy((void*)g_pD3D12FenceVTable, *(void**)pFence, 11 * sizeof(uint64_t));

    writeLog("initDX12VTable done");
    writeLog("ExecuteCommandLists: " + std::to_string((uint64_t)g_pD3D12CommandQueueVTable[10]));
    writeLog("Present: " + std::to_string((uint64_t)g_pDXGISwapChainVTable[8]));
    writeLog("SetEventOnCompletion: " + std::to_string((uint64_t)g_pD3D12FenceVTable[9]));
    writeLog("Signal: " + std::to_string((uint64_t)g_pD3D12FenceVTable[10]));

    pFactory->Release();
    pAdapter->Release();
    pDevice->Release();
    pCommandQueue->Release();
    pSwapChain->Release();
    pFence->Release();
    ::DestroyWindow(window);
    ::UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);

    return true;
}


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
    // Hook D3D12Fence的SetEventOnCompletion方法
    if (MH_CreateHook((void*)g_pD3D12FenceVTable[9], hkSetEventOnCompletionHook, (void**)&pfnSetEventOnCompletionOrig) != MH_OK) {
        MessageBoxA(NULL, "MH_CreateHook SetEventOnCompletion failed!", "Error", MB_OK);
        return 1;
    }
    else {
        writeLog("MH_CreateHook SetEventOnCompletion done");
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
    if (MH_DisableHook((LPVOID)g_pDXGISwapChainVTable[8]) != MH_OK) {
        MessageBoxA(NULL, "MH_DisableHook Present failed!", "Error", MB_OK);
    }
    else {
        writeLog("MH_DisableHook Present done");
    }

    wakeRender();

    if (MH_DisableHook((LPVOID)g_pD3D12CommandQueueVTable[10]) != MH_OK) {
        MessageBoxA(NULL, "MH_DisableHook ExecuteCommandLists failed!", "Error", MB_OK);
    }
    else {
        writeLog("MH_DisableHook ExecuteCommandLists done");
    }

    if (MH_DisableHook((LPVOID)g_pD3D12FenceVTable[9]) != MH_OK) {
        MessageBoxA(NULL, "MH_DisableHook SetEventOnCompletion failed!", "Error", MB_OK);
    }
    else {
        writeLog("MH_DisableHook SetEventOnCompletion done");
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