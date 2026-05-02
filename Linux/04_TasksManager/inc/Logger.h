#pragma once
#include <string>
#include <mutex>
#include <fstream>

class logger {
private:
    logger();
    ~logger();
public:
    // 获取单一实例接口
    static logger& getInstance();

    // 记录日志
    void log(const std::string& message);

    // 禁止拷贝
    logger(const logger& other) = delete;
    logger& operator=(const logger& other) = delete; 
private:
    std::mutex mtx;
    std::ofstream logFile;
};


