cmake_minimum_required(VERSION 3.10)
project(ItemParser)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)

# 包含头文件目录
include_directories(include)

# 查找依赖库
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
add_executable(item_parser 
    src/main.cpp 
    src/db_config.cpp 
    src/item.cpp
)

# 链接库
target_link_libraries(item_parser
    mysqlcppconn
    ssl
    crypto
    pthread
    jsoncpp
)

# 安装配置文件
configure_file(config.json ${CMAKE_CURRENT_BINARY_DIR}/config/config.json COPYONLY)

# 安装可执行文件
install(TARGETS item_parser DESTINATION bin)
