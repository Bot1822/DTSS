#pragma once
#include <string>
#include <sstream>
#include <fstream>

#define logEnabled true

std::ofstream logFile = std::ofstream("C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\log\\gamehook.log", std::ios::out | std::ios::app);
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