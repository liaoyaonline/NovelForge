cmake_minimum_required(VERSION 3.10)
project(RoleSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MYSQL_INCLUDE_DIR "/usr/include")
set(MYSQL_CONNECTOR_INCLUDE_DIR "${MYSQL_INCLUDE_DIR}/cppconn")
set(MYSQL_LIB_DIR "/usr/lib/x86_64-linux-gnu")

find_package(nlohmann_json 3.2.0 REQUIRED)

include_directories(
    include
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_CONNECTOR_INCLUDE_DIR}
)

link_directories(${MYSQL_LIB_DIR})

add_executable(character_simulator
    src/main.cpp
    src/Character.cpp       # 添加此行
    src/DatabaseManager.cpp
    src/SimulatorApp.cpp
)

find_library(MYSQLCPPCONN_LIB mysqlcppconn HINTS ${MYSQL_LIB_DIR})
find_library(MYSQLCLIENT_LIB mysqlclient HINTS ${MYSQL_LIB_DIR})
find_library(SSL_LIB ssl)
find_library(CRYPTO_LIB crypto)
find_library(PTHREAD_LIB pthread)

set(FILESYSTEM_LIB stdc++fs)

target_link_libraries(character_simulator
    ${MYSQLCPPCONN_LIB}
    ${MYSQLCLIENT_LIB}
    ${SSL_LIB}
    ${CRYPTO_LIB}
    ${PTHREAD_LIB}
    nlohmann_json::nlohmann_json
    ${FILESYSTEM_LIB}
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/config)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/db_config.json 
               ${CMAKE_CURRENT_BINARY_DIR}/config/db_config.json COPYONLY)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/results)

target_compile_definitions(character_simulator PRIVATE CPPCONN_PUBLIC_FUNC=)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
    message(STATUS "Building debug version")
else()
    add_compile_options(-O2)
    message(STATUS "Building release version")
endif()
