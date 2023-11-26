#include "pch.h"
#include "HookDef.h"
#include "RenderStatus.h"
#include "Logger.h"

pfnExecuteCommandLists pfnExecuteCommandListsOrig = NULL;
pfnPresent pfnPresentOrig = NULL;
pfnSetEventOnCompletion pfnSetEventOnCompletionOrig = NULL;

// Hook后的ExecuteCommandLists方法
void WINAPI hkExecuteCommandListsHook(ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
    if (getRenderStatus() == 1)
    {
        // 调用原始的ExecuteCommandLists方法
        writeLog("Call ExecuteCommandLists");
        pfnExecuteCommandListsOrig(pCommandQueue, NumCommandLists, ppCommandLists);
        return;
    }

    else
    {
    // 等待时间片状态为1
    writeLog("Call ExecuteCommandLists but wait");

    waitRenderStatus(1);

    // 调用原始的ExecuteCommandLists方法
    writeLog("Resume ExecuteCommandLists");
    pfnExecuteCommandListsOrig(pCommandQueue, NumCommandLists, ppCommandLists);
    return;
    }
}

// Hook后的Present方法
HRESULT WINAPI hkPresentHook(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    writeLog("IN hkPresentHook");
    // 调用原始的Present方法
    HRESULT hr = pfnPresentOrig(pSwapChain, SyncInterval, Flags);
    // 每个调度周期只允许展示一次画面
    setRenderStatus(0);

    return hr;
}

// Hook后的SetEventOnCompletion方法
HRESULT WINAPI hkSetEventOnCompletionHook(ID3D12Fence* pFence, UINT64 Value, HANDLE hEvent)
{
    writeLog("IN hkSetEventOnCompletionHook");
    // 调用原始的SetEventOnCompletion方法
    HRESULT hr = pfnSetEventOnCompletionOrig(pFence, Value, hEvent);
    return hr;
}
