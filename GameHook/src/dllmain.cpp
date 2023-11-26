// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "Logger.h"
#include "RenderStatus.h"
#include "DX12Hook.h"

// 时间片数量
int gameTsNum = 4;
int otherTsNum = 20;
int TsNum = 24;

// Hook DX12 VTable的线程
HANDLE hThread_HookDX12VTable = NULL;

HANDLE hTimerQueue = NULL;
HANDLE hTimer_loop = NULL;

void CALLBACK timerThread(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
    writeLog("\nIN timerThread");

    wakeRender();
    
    return;
}

void InitiateThreads() {    
    hTimerQueue = CreateTimerQueue();
    if (hTimerQueue == NULL) {
        MessageBoxA(NULL, "CreateTimerQueue failed!", "Error", MB_OK);
        return;
    }
    else {
        writeLog("CreateTimerQueue done");
    }

    if (!CreateTimerQueueTimer(&hTimer_loop, hTimerQueue, (WAITORTIMERCALLBACK)timerThread, NULL, 0, TsNum, WT_EXECUTEINTIMERTHREAD)) {
        MessageBoxA(NULL, "CreateTimerQueueTimer failed!", "Error", MB_OK);
        return;
    }
    else {
        writeLog("CreateTimerQueueTimer done");
    }


    // 创建DX12 VTable Hook线程，该线程不需要传递参数
    hThread_HookDX12VTable = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)hookDX12VTable, NULL, 0, NULL);
}

void CleanupTimer() {
    // 销毁timer
    if (hTimer_loop != NULL) {
        HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (event == NULL) {
			std::runtime_error("CreateEvent failed!"); 
			return;
		}
        DeleteTimerQueueTimer(hTimerQueue, hTimer_loop, event);
        WaitForSingleObject(event, INFINITE);
        writeLog("DeleteTimerQueueTimer done");
        CloseHandle(event);
        hTimer_loop = NULL;
        writeLog("hTimer_loop has exited");
    }
    if (hTimerQueue != NULL) {
        DeleteTimerQueue(hTimerQueue);
        hTimerQueue = NULL;
        writeLog("hTimerQueue has exited");
    }

    writeLog("CleanupTimer done");

    return;
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

        initRenderStatus();

        //启用日志
        Logger::Initialize("C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\log\\gamehook.log", false);

        InitiateThreads();
        break;
    case DLL_PROCESS_DETACH:
        CleanupTimer();
        unhookDX12VTable();
        
        // 恢复系统定时器精度
        timeEndPeriod(1);
        writeLog("Log end time: " + std::to_string(timeGetTime()));

        // 关闭日志
        Logger::Close();

        MessageBoxA(NULL, "DLL_PROCESS_DETACH", "Notice", MB_OK);
        break;
    }
    return TRUE;
}

