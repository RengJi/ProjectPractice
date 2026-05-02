#pragma once 
#include "Logger.h"
#include "TaskManager.h"
#include <iostream>
#include <string>

// CPTR 基类模板
class CommandBase {
public:
    virtual void execute(const std::string& args) = 0;
};

template<typename Derived>
class Command : public CommandBase {
public:
    void execute(const std::string& args) 
    {
        static_cast<Derived*>(this)->executeImpl(args); // 将 this 指针从 Command 指针变成 Derived 指针 
    }
};

// 静态多态实现

// 添加命令
class AddCommand : public Command<AddCommand> {
public:
    AddCommand(TaskManager &manager) : taskManager(manager) {}
    void executeImpl(const std::string& args) 
    {
        size_t pos1 = args.find(',');
        size_t pos2 = args.find(',', pos1 + 1);

        if (pos1 == std::string::npos || pos2 == std::string::npos) {
            std::cerr << "格式错误，请使用：add<描述>,<优先级>,<截至日期>" << std::endl;
            return;
        }

        std::string description = args.substr(0, pos1);
        int poriority = std::stoi(args.substr(pos1 + 1, pos2 - pos1 -1));
        std::string dueDate = args.substr(pos2 + 1);
        taskManager.add_task(description, poriority, dueDate);
        std::cout << "添加任务成功" << std::endl;
    }

private:
    TaskManager& taskManager;
};

// 删除命令
class DeleteCommand : public Command<DeleteCommand> {
public:
    DeleteCommand(TaskManager &manager) : taskManager(manager) {}
    void executeImpl(const std::string& args) 
    {
        try {
            size_t pos;
            int id = std::stoi(args, &pos);
            if (pos != args.length()) {
                std::cerr << "参数格式错误。请使用：delete <id>" << std::endl;
                return;
            }
            taskManager.delete_task(id);
            std::cout << "删除任务成功" << std::endl;

        } catch(std::invalid_argument& e) {
            std::cerr << "参数格式错误。请使用：delete <id>" << std::endl;
            return;
        } catch(std::out_of_range& e) {
            std::cerr << "参数范围错误" << std::endl;
        }
    }
private:
    TaskManager& taskManager;
};

// 更新命令
class UpdateCommand : public Command<UpdateCommand> {
public:
    UpdateCommand(TaskManager &manager) : taskManager(manager) {}
    void executeImpl(const std::string& args) 
    {
        size_t pos1 = args.find(',');
        size_t pos2 = args.find(',', pos1 + 1);
        size_t pos3 = args.find(',', pos2 + 1);
        if (pos1 == std::string::npos || pos2 == std::string::npos || pos3 == std::string::npos) {
            std::cerr << "参数格式错误。请使用：update <id>,<描述>,<优先级>,<截至时间>" << std::endl;
            return;
        }
        int id = std::stoi(args.substr(0, pos1));
        std::string description = args.substr(pos1 + 1, pos2 - pos1 - 1);
        int poriority = std::stoi(args.substr(pos2 + 1, pos3 - pos2 - 1));
        std::string dueDate = args.substr(pos3 + 1);
        taskManager.update_task(id, description, poriority, dueDate);
        std::cout << "更新任务成功" << std::endl;
    }
private:
    TaskManager& taskManager;
};

// 列举命令
class ListCommand : public Command<ListCommand> {
public:
    ListCommand(TaskManager &manager) : taskManager(manager) {}
    void executeImpl(const std::string& args) 
    {
        int sortOption = 0;
        if (!args.empty()) {
            sortOption = std::stoi(args);
        }
        taskManager.list_tasks(sortOption);
    }
private:
    TaskManager& taskManager;
};