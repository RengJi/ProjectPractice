#pragma once

#include "Task.h"
#include <vector>

class TaskManager {
private:
    // 辅助排序函数
    static bool comparedbyporiority(const Task& a, const Task& b);
    static bool comparedbyduedata(const Task& a, const Task& b);
public:
    TaskManager();

    void add_task(const std::string& description, int poriority,const std::string& duedate);
    void delete_task(int id);
    void update_task(int id, const std::string& description, int poriority, const std::string& duedate);
    void list_tasks(int sortOption = 0); // 0-按ID 1-按优先级 2-按截至日期
    void load_tasks();
    void save_tasks() const;

private:
    std::vector<Task> tasks;
    int nextId;
};
