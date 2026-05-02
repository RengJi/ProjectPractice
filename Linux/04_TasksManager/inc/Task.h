#pragma once
#include <string>
#include <sstream>
#include <fstream>

// 任务结构体
struct Task {
    int id;
    int poriority;
    std::string description;
    std::string dueDate;

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "ID：" << id 
            << "，描述：" << description
            << "，优先级：" << poriority
            << "，截止日期：" << dueDate;
        return oss.str();
    }
};
