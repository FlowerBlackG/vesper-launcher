#[[
    vesper launcher 顶层构建文件
    创建于 2024年2月25日 京沪高铁上
]]

cmake_minimum_required(VERSION 3.24.1)


#[[
    Vesper Launcher 版本号
]]
set(VESPER_LAUNCHER_VERSION_MAJOR 0)
set(VESPER_LAUNCHER_VERSION_MINOR 0)
set(VESPER_LAUNCHER_VERSION_PATCH 1)
set(VESPER_LAUNCHER_VERSION_TAIL "") # 例："-dev"

set(VESPER_LAUNCHER_VERSION_CODE 1)

set(
    VESPER_LAUNCHER_VERSION_NAME 
    "${VESPER_LAUNCHER_VERSION_MAJOR}.${VESPER_LAUNCHER_VERSION_MINOR}.${VESPER_LAUNCHER_VERSION_PATCH}${VESPER_LAUNCHER_VERSION_TAIL}"
)

#[[
    项目基本配置信息
]]
project(vesper-launcher)
configure_file(
    ${PROJECT_SOURCE_DIR}/config.h.in 
    ${PROJECT_SOURCE_DIR}/config.h
)

# 指定使用 C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)


include_directories(${PROJECT_SOURCE_DIR})


#[[ 
    GCC(g++) 编译选项
]]
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

#[[ 
    项目构造产物
]]
file(GLOB_RECURSE CPP_SOURCE_FILES *.cpp)
file(GLOB_RECURSE C_SOURCE_FILES *.c)
add_executable(
    ${PROJECT_NAME} ${CPP_SOURCE_FILES} ${C_SOURCE_FILES}
)


#[[ 
    寻找依赖库
]]
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake-modules)

find_package(Systemd REQUIRED)

#[[
    关联依赖库
]]

include_directories(
    ${SYSTEMD_INCLUDE_DIRS}
)

target_link_libraries(
    ${PROJECT_NAME}

    ${SYSTEMD_LIBRARIES}
)


# 暂无
