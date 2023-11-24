#pragma once

#include "pch.h"
#include "Logger.h"

class Logger
{
public:
    static void Initialize(const std::string& logFilePath, bool isAppend);
    static void Close();
    // 线程安全的写入方法
    static void LogSafe(const std::string& message);
    // 不需要锁的写入方法（仅在单线程环境或已经外部同步时使用）
    static void LogUnsafe(const std::string& message);

private:
    static std::ofstream m_logFile;
    static std::mutex m_mutex;
    static bool m_isInitialized;
};

