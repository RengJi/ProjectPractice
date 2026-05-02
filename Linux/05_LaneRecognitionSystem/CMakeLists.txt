cmake_minimum_required(VERSION 3.10)

project(LaneRecognitionSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(TARGET_NAME lane_recognition)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/app/include)
set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/app/source)
set(3RDPARTY_DIR ${CMAKE_SOURCE_DIR}/3rdparty)

file(GLOB_RECURSE SOURCES 
    "${SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE HEADERS 
    "${INCLUDE_DIR}/*.h"
    "${INCLUDE_DIR}/*.hpp"
)

add_executable(${TARGET_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${TARGET_NAME} PRIVATE
    ${INCLUDE_DIR}
    ${3RDPARTY_DIR}/rknn/include
)

find_package(OpenCV REQUIRED)
find_package(OpenMP REQUIRED)

target_include_directories(${TARGET_NAME} PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

find_library(RKNN_LIBRARY NAMES rknnrt rknnmrt PATHS 
    /usr/lib 
    /usr/local/lib 
    /lib/aarch64-linux-gnu
    ${3RDPARTY_DIR}/rknn/lib
)

find_library(MPP_LIBRARY NAMES rockchip_mpp mpp PATHS
    /usr/lib
    /usr/local/lib
    /lib/aarch64-linux-gnu
)

find_library(RGA_LIBRARY NAMES rga librga PATHS
    /usr/lib
    /usr/local/lib
    /lib/aarch64-linux-gnu
)

target_link_libraries(${TARGET_NAME} PRIVATE
    ${OpenCV_LIBS}
    ${RKNN_LIBRARY}
    ${MPP_LIBRARY}
    ${RGA_LIBRARY}
    OpenMP::OpenMP_CXX
    pthread
    dl
)

if(RKNN_LIBRARY)
    message(STATUS "Found RKNN library: ${RKNN_LIBRARY}")
else()
    message(WARNING "RKNN library not found, please ensure it is installed")
endif()

if(MPP_LIBRARY)
    message(STATUS "Found MPP library: ${MPP_LIBRARY}")
else()
    message(WARNING "MPP library not found, please ensure it is installed")
endif()

if(RGA_LIBRARY)
    message(STATUS "Found RGA library: ${RGA_LIBRARY}")
else()
    message(WARNING "RGA library not found, please ensure it is installed")
endif()

target_compile_definitions(${TARGET_NAME} PRIVATE
    $<$<CONFIG:DEBUG>:DEBUG>
    $<$<CONFIG:RELEASE>:NDEBUG>
)

install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${INCLUDE_DIR}/
    DESTINATION include
)

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Lane Recognition System for Rockchip Platform")
