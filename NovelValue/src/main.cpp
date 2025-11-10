#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iomanip> // 添加的头文件
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <nlohmann/json.hpp>
#include "db_config.h"
#include "item.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

// 打印帮助信息
void printHelp() {
    std::cout << "物品管理工具\n"
              << "用法:\n"
              << "  item_tool import     从data目录导入物品\n"
              << "  item_tool list       列出所有物品(简略)\n"
              << "  item_tool listfull   列出所有物品(详细)\n"
              << "  item_tool export     导出物品到JSON文件\n"
              << "  item_tool help       显示帮助信息\n";
}

// 导入物品
void importItems() {
    try {
        // 加载数据库配置
        DBConfig config = loadConfig("config/config.json");
        
        // 创建MySQL连接
        sql::Driver* driver = get_driver_instance();
        std::string connectionStr = "tcp://" + config.host + ":" + std::to_string(config.port);
        sql::Connection* con = driver->connect(connectionStr, config.user, config.password);
        
        // 选择数据库
        con->setSchema(config.database);
        
        // 设置字符集为utf8mb4
        sql::Statement* stmt = con->createStatement();
        stmt->execute("SET NAMES 'utf8mb4'");
        delete stmt;
        
        // 处理data目录下的所有JSON文件
        for (const auto& entry : fs::directory_iterator("../data")) {
            if (entry.path().extension() == ".json") {
                std::cout << "处理文件: " << entry.path() << std::endl;
                
                std::ifstream f(entry.path());
                json data = json::parse(f);
                
                // 检查是否是包含item数组的结构
                if (data.is_object() && data.contains("item") && data["item"].is_array()) {
                    std::cout << "检测到 'item' 数组，包含 " << data["item"].size() << " 个物品" << std::endl;
                    
                    for (const auto& itemData : data["item"]) {
                        try {
                            Item item = parseItem(itemData);
                            if (containsChinese(item.name)) {
                                std::cout << "解析并插入: " << item.name << std::endl;
                                insertItem(con, item);
                            } else {
                                std::cout << "跳过英文物品: " << item.name << std::endl;
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "解析物品错误: " << e.what() << std::endl;
                        }
                    }
                }
                // 处理直接数组格式的JSON
                else if (data.is_array()) {
                    std::cout << "检测到直接数组，包含 " << data.size() << " 个物品" << std::endl;
                    
                    for (const auto& itemData : data) {
                        try {
                            Item item = parseItem(itemData);
                            if (containsChinese(item.name)) {
                                std::cout << "解析并插入: " << item.name << std::endl;
                                insertItem(con, item);
                            } else {
                                std::cout << "跳过英文物品: " << item.name << std::endl;
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "解析物品错误: " << e.what() << std::endl;
                        }
                    }
                } 
                // 处理对象格式的JSON
                else if (data.is_object()) {
                    try {
                        Item item = parseItem(data);
                        if (containsChinese(item.name)) {
                            std::cout << "解析并插入: " << item.name << std::endl;
                            insertItem(con, item);
                        } else {
                            std::cout << "跳过英文物品: " << item.name << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "解析物品错误: " << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "不支持的JSON格式: " << entry.path() << std::endl;
                }
            }
        }
        
        // 关闭连接
        delete con;
        std::cout << "数据导入完成" << std::endl;
    } catch (const sql::SQLException& e) {
        std::cerr << "MySQL错误: " << e.what() << std::endl;
        std::cerr << "错误代码: " << e.getErrorCode() << std::endl;
        std::cerr << "SQL状态: " << e.getSQLState() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
}

// 列出所有物品(简略)
void listItems(bool fullDetails = false) {
    try {
        // 加载数据库配置
        DBConfig config = loadConfig("config/config.json");
        
        // 创建MySQL连接
        sql::Driver* driver = get_driver_instance();
        std::string connectionStr = "tcp://" + config.host + ":" + std::to_string(config.port);
        sql::Connection* con = driver->connect(connectionStr, config.user, config.password);
        con->setSchema(config.database);
        
        auto items = getAllItems(con);
        std::cout << "数据库中共有 " << items.size() << " 个物品\n\n";
        
        if (fullDetails) {
            // 详细列表
            for (const auto& item : items) {
                std::cout << "ID: " << item.id << "\n"
                          << "名称: " << item.name << "\n"
                          << "英文名: " << item.eng_name << "\n"
                          << "来源: " << item.source << "\n"
                          << "页码: " << item.page << "\n"
                          << "重量: " << item.weight << " 磅\n"
                          << "价值: " << item.value << " 铜币\n"
                          << "护甲等级: " << item.ac << "\n"
                          << "力量需求: " << item.strength << "\n"
                          << "物品介绍:\n";
                
                for (const auto& entry : item.entries) {
                    std::cout << "  " << entry << "\n";
                }
                
                std::cout << "\n-----------------------------------\n";
            }
        } else {
            // 简要列表
            std::cout << std::left << std::setw(5) << "ID" 
                      << std::setw(30) << "名称" 
                      << std::setw(12) << "重量" 
                      << std::setw(12) << "价值" 
                      << std::setw(8) << "护甲" 
                      << "\n";
            std::cout << std::string(70, '-') << "\n";
            
            for (const auto& item : items) {
                std::cout << std::left 
                          << std::setw(5) << item.id
                          << std::setw(30) << (item.name.length() > 28 ? item.name.substr(0, 25) + "..." : item.name)
                          << std::setw(12) << (std::to_string(item.weight) + " 磅")
                          << std::setw(12) << (std::to_string(item.value) + " 铜币")
                          << std::setw(8) << (item.ac > 0 ? std::to_string(item.ac) : "-")
                          << "\n";
            }
        }
        
        delete con;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
}

// 导出物品
void exportItems() {
    try {
        // 加载数据库配置
        DBConfig config = loadConfig("config/config.json");
        
        // 创建MySQL连接
        sql::Driver* driver = get_driver_instance();
        std::string connectionStr = "tcp://" + config.host + ":" + std::to_string(config.port);
        sql::Connection* con = driver->connect(connectionStr, config.user, config.password);
        con->setSchema(config.database);
        
        exportItemsToJson(con, "exported_items.json");
        
        delete con;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "import") {
        importItems();
    } else if (command == "list") {
        listItems(false);
    } else if (command == "listfull") {
        listItems(true);
    } else if (command == "export") {
        exportItems();
    } else if (command == "help") {
        printHelp();
    } else {
        std::cerr << "未知命令: " << command << "\n\n";
        printHelp();
        return 1;
    }
    
    return 0;
}
