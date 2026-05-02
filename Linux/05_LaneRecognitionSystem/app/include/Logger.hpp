#pragma once

#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <string>
#include <mutex>

namespace Logger
{
    enum class LogLevel
    {
        INFO,
        WARNING,
        ERROR
    };

    inline std::mutex logMutex;
    inline LogLevel currentLogLevel = LogLevel::INFO;

    // 获取当前时间的字符串表示
    inline std::string GetCurrentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&nowTime), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << nowMs.count();
        return ss.str();
    }

    // 将日志级别转换为字符串
    inline std::string LogLevelToString(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
        }
    }

    // 输出日志消息
    template <typename... Args>
    inline void Log(LogLevel level, const char *tag, Args... args)
    {
        if (level < currentLogLevel)
        {
            return; // 如果日志级别低于当前设置的级别，则不输出
        }

        std::lock_guard<std::mutex> lock(logMutex); // 确保线程安全

        std::stringstream ss;
        ss << "[" << GetCurrentTime() << "] [" << LogLevelToString(level) << "] [" << tag << "] ";
        (ss << ... << args); // 使用二元右折叠表达式将所有参数连接成一个字符串

        ss << std::endl; // 添加换行符

        std::cout << ss.str(); // 输出日志消息到标准输出流
    }
}


#define LOG_INFO(tag, ...) Logger::Log(Logger::LogLevel::INFO, tag, __VA_ARGS__)
#define LOG_WARNING(tag, ...) Logger::Log(Logger::LogLevel::WARNING, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::Log(Logger::LogLevel::ERROR, tag, __VA_ARGS__)
