#include "TaskManager.h"
#include "Command.h"
#include <iostream>
#include <unordered_map>
#include <memory>

int main() 
{
    // 实例化任务管理
    TaskManager taskmanager;

    // 命令映射
    std::unordered_map<std::string, std::unique_ptr<CommandBase>> commands;
    // 下面其实是 构造临时对象 + 移动语义
    commands["add"] = std::make_unique<AddCommand>(taskmanager);
    commands["delete"] = std::make_unique<DeleteCommand>(taskmanager);
    commands["update"] = std::make_unique<UpdateCommand>(taskmanager);
    commands["list"] = std::make_unique<ListCommand>(taskmanager);

    std::cout << "欢迎使用任务管理系统！" << std::endl;
    std::cout << "可用命令: add, delete, list, update, exit" << std::endl;

    std::string input;
    while (true) {
        std::cout <<">";
        std::getline(std::cin, input);
        if (input.empty()) continue;

        size_t spacePos = input.find(' ');
        std::string cmd = input.substr(0, spacePos);
        std::string args;
        if (spacePos != std::string::npos) {
            args = input.substr(spacePos + 1);
        }

        if (cmd == "exit") {
            std::cout << "程序退出" << std::endl;
            break;
        }

        auto it = commands.find(cmd);
        if (it != commands.end()) {
            it->second->execute(args);
        } else {
            std::cout << "未知指令：" << cmd << std::endl;
        }
    }
    
    return 0;
}
