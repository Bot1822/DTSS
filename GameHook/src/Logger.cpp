#include "pch.h"
#include "Logger.h"

std::ofstream Logger::m_logFile;
std::mutex Logger::m_mutex;
bool Logger::m_isInitialized = false;

// 写文件方法
void writeLog(const std::string& message)
{
    Logger::LogSafe(message);
}

void Logger::Initialize(const std::string& logFilePath, bool isAppend)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_isInitialized)
    {
        throw std::runtime_error("Logger is already initialized.");
    }
    m_logFile.open(logFilePath, std::ios::out | (isAppend ? std::ios::app : std::ios::trunc));
    if (!m_logFile.is_open())
    {
        throw std::runtime_error("Failed to open log file.");
    }
    m_logFile << "Log file opened!" << std::endl;
    m_logFile.flush();
    m_isInitialized = true;
}

void Logger::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isInitialized)
    {
        throw std::runtime_error("Logger is not initialized.");
    }
    m_logFile << "Log file closed." << std::endl;
    m_logFile.flush();
    m_logFile.close();
    m_isInitialized = false;
}

void Logger::LogSafe(const std::string& message)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isInitialized)
    {
        throw std::runtime_error("Logger is not initialized.");
    }
    m_logFile << message << std::endl;
    m_logFile.flush();
}

void Logger::LogUnsafe(const std::string& message)
{
    if (!m_isInitialized)
    {
        throw std::runtime_error("Logger is not initialized.");
    }
    m_logFile << message << std::endl;
}

