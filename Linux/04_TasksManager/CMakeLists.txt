cmake_minimum_required(VERSION 3.10)
project(TaskManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

include_directories(inc)
set(SOURCES
    src/main.cpp
    src/Logger.cpp
    src/TaskManager.cpp
)

add_executable(TaskManager ${SOURCES})
