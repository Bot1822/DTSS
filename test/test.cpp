#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#pragma comment(lib, "winmm" )


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


DWORD WINAPI timerThread(LPVOID lpParam) {
    HANDLE hEvent = (HANDLE)lpParam;
    while (WaitForSingleObject(hEvent, 0) != WAIT_OBJECT_0) {
        // Do work here...
        std::cout << "Timer thread running..." << std::endl;
        writeLog("Timer thread running...");
        Sleep(50); // Simulate some work.
    }
    // Perform any cleanup tasks here...
    std::cout << "Timer thread exiting." << std::endl;
    writeLog("Timer thread exiting.");
    return 0;
}

int main() {

    //启用日志
    logFile = std::ofstream("C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\log\\test.log", std::ios::out | std::ios::trunc);
    if (!logFile.is_open())
    {
        std::cout << "log file open failed" << std::endl;
        exit(1);
    }
    else {
        writeLog("Log begin time: " + std::to_string(timeGetTime()));
    }

    // Create an event.
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hEvent == NULL) {
        std::cerr << "CreateEvent failed (" << GetLastError() << ")" << std::endl;
        return 1;
    }

    // Start the timer thread.
    DWORD dwThreadId;
    HANDLE hThread = CreateThread(NULL, 0, timerThread, hEvent, 0, &dwThreadId);
    if (hThread == NULL) {
        std::cerr << "CreateThread failed (" << GetLastError() << ")" << std::endl;
        CloseHandle(hEvent);
        return 1;
    }

    Sleep(1000); // Simulate some work.

    // Signal the event to stop the timer thread.
    SetEvent(hEvent);

    // Wait for the timer thread to exit.
    Sleep(100);
    DWORD lpExitCode;
    GetExitCodeThread(hThread, &lpExitCode);
    std::cout << "Timer thread exit code: " << lpExitCode << std::endl;
    writeLog("Timer thread exit code: " + std::to_string(lpExitCode));

    // Wait for the thread to finish.
    WaitForSingleObject(hThread, INFINITE);
    std::cout << "Timer thread finished." << std::endl;
    writeLog("Timer thread finished.");

    // Cleanup handles.
    CloseHandle(hThread);
    CloseHandle(hEvent);

    std::cout << "Main thread exiting." << std::endl;
    return 0;
}