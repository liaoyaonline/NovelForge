cmake_minimum_required(VERSION 3.10)
project(geartracker)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 使用您提供的路径
set(MYSQL_INCLUDE_DIR "/usr/include")
set(MYSQL_LIB_DIR "/usr/lib/x86_64-linux-gnu")

# 手动设置包含路径和库
include_directories(
    include
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}/cppconn
    /usr/include/jsoncpp
)

link_directories(${MYSQL_LIB_DIR})

# 添加可执行文件
add_executable(geartracker 
    src/Database.cpp
    src/main.cpp
    src/Config.cpp  # 新增
    src/WebServer.cpp
)

# 链接MySQL库及所有依赖
target_link_libraries(geartracker
    mysqlcppconn
    ssl
    crypto
    pthread
    jsoncpp
)

# 安装配置文件
configure_file(config/config.ini ${CMAKE_CURRENT_BINARY_DIR}/config/config.ini COPYONLY)

# 新增：复制web文件夹到构建目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/web DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


# 添加编译定义
target_compile_definitions(geartracker PRIVATE CPPCONN_PUBLIC_FUNC=)
