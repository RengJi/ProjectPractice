#include "Logger.h"
#include <iostream>
#include <ctime>
#include <chrono>

logger& logger::getInstance() 
{
    static logger instance;
    return instance;
}

logger::logger() 
{
    logFile.open("log.txt", std::ios::app);
    if (!logFile.is_open()) {
        std::cerr << "无法打开日志" << std::endl;
    }
}

logger::~logger()
{
    if (logFile.is_open()) {
        logFile.close();
    }
}

void logger::log(const std::string& message) 
{
    std::lock_guard<std::mutex> lock(mtx);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now(); // 获取现在时间
        const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        logFile << ctime(&now_time) << ": " << message << std::endl;
    }
}
