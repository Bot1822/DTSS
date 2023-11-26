#pragma once

#include "pch.h"

// DX12虚函数表
static uint64_t *g_pD3D12DeviceVTable;
static uint64_t *g_pD3D12CommandQueueVTable;
static uint64_t *g_pDXGISwapChainVTable;
static uint64_t *g_pD3D12FenceVTable;

// 用于保存原始的方法、Hook后的方法
// ID3D12CommandQueue::ExecuteCommandLists
typedef void(WINAPI *pfnExecuteCommandLists)(ID3D12CommandQueue *pCommandQueue, UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists);
extern pfnExecuteCommandLists pfnExecuteCommandListsOrig;
void WINAPI hkExecuteCommandListsHook(ID3D12CommandQueue *pCommandQueue, UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists);

// IDXGISwapChain::Present
typedef HRESULT(WINAPI *pfnPresent)(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags);
extern pfnPresent pfnPresentOrig;
HRESULT WINAPI hkPresentHook(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags);

// ID3D12Fence::SetEventOnCompletion
typedef HRESULT(WINAPI *pfnSetEventOnCompletion)(ID3D12Fence *pFence, UINT64 Value, HANDLE hEvent);
extern pfnSetEventOnCompletion pfnSetEventOnCompletionOrig;
HRESULT WINAPI hkSetEventOnCompletionHook(ID3D12Fence *pFence, UINT64 Value, HANDLE hEvent);
