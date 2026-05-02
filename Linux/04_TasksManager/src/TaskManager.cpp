#include "Task.h"
#include "Logger.h"
#include "TaskManager.h"
#include <iostream>
#include <algorithm>

// 任务管理构造函数
TaskManager::TaskManager() : nextId(1) 
{
    load_tasks();
}

// 添加任务
void TaskManager::add_task(const std::string& description, int poriority,const std::string& duedate)
{
    Task task;
    task.id = nextId++;
    task.description = description;
    task.poriority = poriority;
    task.dueDate = duedate;
    tasks.push_back(task);
    logger::getInstance().log("添加任务：" + task.toString());
    save_tasks();
}

// 删除任务
void TaskManager::delete_task(int id)
{
    auto it = std::find_if(tasks.begin(), tasks.end(), [&id](const Task& task) {
        return task.id == id;
    });
    if (it != tasks.end()) {
        logger::getInstance().log("删除任务：" + it->toString());
        tasks.erase(it);
        save_tasks();
    } else {
        std::ostringstream oss;
        oss << "未找到ID为 " << id << " 的任务!";
        logger::getInstance().log(oss.str());
    }
}

// 更新任务
void TaskManager::update_task(int id, const std::string& description, int poriority, const std::string& duedate)
{
    for (auto &task : tasks) {
        if (task.id == id) {
            logger::getInstance().log("准备更新任务，更新前任务：" + task.toString());
            task.description = description;
            task.poriority = poriority;
            task.dueDate = duedate;
            logger::getInstance().log("更新任务完成，更新后任务：" + task.toString());
            save_tasks();
            return;
        }
    }
    std::ostringstream oss;
    oss << "未找到ID为 " << id << " 的任务!";
    logger::getInstance().log(oss.str());
}

// 打印任务列表：0-按ID 1-按优先级 2-按截至日期
void TaskManager::list_tasks(int sortOption)
{
    switch (sortOption)
    {
        case 1:
            std::sort(tasks.begin(), tasks.end(), comparedbyporiority);
            break;
        case 2:
            std::sort(tasks.begin(), tasks.end(), comparedbyduedata);
            break;
        default:
            break;
    }
    for (const auto& task : tasks) {
        std::cout << task.toString() << std::endl;
    }
}

// 加载任务
void TaskManager::load_tasks()
{
    std::ifstream ifs("task.txt");
    if (!ifs.is_open()) {
        logger::getInstance().log("任务文件不存在，新建文件!");
        return;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line); // 将获取的信息存入字符串流
        Task task;
        char delimiter;
        iss >> task.id >> delimiter;
        std::getline(iss, task.description, ',');
        iss >> task.poriority >> delimiter;
        iss >> task.dueDate;
        tasks.push_back(task);
        if (task.id >= nextId) nextId = task.id + 1;
    }
    ifs.close();
    logger::getInstance().log("任务加载成功");
}

// 保存任务
void TaskManager::save_tasks() const
{
    std::ofstream ofs("task.txt");
    if (!ofs.is_open()) {
        logger::getInstance().log("无法打开任务文件!");
        return;
    }

    for (const auto &task : tasks) {
        ofs << task.id << "," << task.description << "," << task.poriority << "," << task.dueDate << "\n";
    }

    ofs.close();
    logger::getInstance().log("任务保存成功");
}

// 按优先级辅助排序
bool TaskManager::comparedbyporiority(const Task& a, const Task& b)
{
    return a.poriority < b.poriority;
}

// 按截至日期辅助排序
bool TaskManager::comparedbyduedata(const Task& a, const Task& b)
{
    return a.dueDate < b.dueDate;
}