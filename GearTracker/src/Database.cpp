#include "Database.h"
#include "Config.h" 

// 首先包含 MySQL 头文件
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/connection.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h> // 添加这个

// 然后是标准库头文件
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip> // 添加这个用于时间格式化
#include <sstream>
#include <mutex>
#include <thread> // 添加头文件用于睡眠




// 获取当前时间的字符串表示
std::string currentDateTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}

// Database.cpp
Database::Database(Config& cfg) 
    : config(cfg), 
      driver(nullptr), 
      con(nullptr),
      logToConsole(true),
      logLevelFlag(LOG_INFO),
      connected(false)
{
    // 1. 从配置获取日志文件名
    logFileName = config.getString("application", "log_file", "geartracker.log");
    
    // 2. 设置日志级别
    std::string logLevel = config.getString("application", "log_level", "info");
    if (logLevel == "debug") {
        logLevelFlag = LOG_DEBUG;
    } else if (logLevel == "warning") {
        logLevelFlag = LOG_WARNING;
    } else if (logLevel == "error") {
        logLevelFlag = LOG_ERROR;
    } else {
        logLevelFlag = LOG_INFO;
    }
    
    // 3. 初始化日志系统并立即尝试连接数据库
    log("数据库实例已创建，配置加载完成 - 尝试连接数据库");
    
    // 关键修改：在构造函数中立即尝试连接
    connect();
}



// Database.cpp
// 保持析构函数实现不变
Database::~Database() {
    // 清理资源
    if (con && !con->isClosed()) {
        log("Disconnecting from database");
        con->close();
    }
}


// 修改日志函数使用配置文件中的日志文件
void Database::log(const std::string& message, bool isError) {
    int level = isError ? LOG_ERROR : LOG_INFO;
    
    // 检查日志级别
    if (level < logLevelFlag) return;
    
    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_time);
    
    // 格式化时间
    std::ostringstream timeStream;
    timeStream << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
    
    // 打开日志文件
    std::ofstream logFileStream(logFileName, std::ios_base::app);
    if (!logFileStream.is_open()) {
        // 如果文件打开失败，尝试使用默认文件名
        logFileStream.open("geartracker.log", std::ios_base::app);
    }
    
    if (logFileStream.is_open()) {
        logFileStream << "[" << timeStream.str() << "] ";
        if (isError) logFileStream << "[ERROR] ";
        else logFileStream << "[INFO] ";
        logFileStream << message << std::endl;
    }
    
    // 同时输出到控制台
    if (logToConsole) {
        std::cout << "[" << timeStream.str() << "] " << message << std::endl;
    }
}



bool Database::connect() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    
    // 清除任何现有无效连接
    if (con && !con->isClosed()) {
        try {
            log("关闭现有连接");
            con->close();
        } catch (const sql::SQLException& e) {
            std::ostringstream oss;
            oss << "关闭连接错误 [code:" << e.getErrorCode()
                << ", SQLState:" << e.getSQLState() << "]: "
                << e.what();
            log(oss.str(), true);
        } catch (...) {
            log("关闭连接时发生未知错误", true);
        }
        con.reset();
    }
    
    // 从配置获取最新连接参数
    std::string host = config.getString("database", "host", "127.0.0.1");
    int port = config.getInt("database", "port", 3306);
    std::string user = config.getString("database", "username", "");
    std::string password = config.getString("database", "password", "");
    std::string dbName = config.getString("database", "database", "geartracker");
    
    // 记录详细的连接参数
    log("连接参数: ");
    log("  主机: " + host);
    log("  端口: " + std::to_string(port));
    log("  用户: " + user);
    log("  数据库: " + dbName);
    
    try {
        // 确保驱动已初始化
        if (!driver) {
            log("获取MySQL驱动实例");
            driver = get_driver_instance();
            if (!driver) {
                log("无法获取MySQL驱动实例", true);
                return false;
            }
        }
        
        // 构建连接字符串
        std::string connectionStr = "tcp://" + host + ":" + std::to_string(port);
        log("创建连接: " + connectionStr);
        
        // 创建新连接
        log("正在连接到MySQL服务器...");
        sql::Connection* rawCon = driver->connect(connectionStr, user, password);
        if (!rawCon) {
            log("连接创建失败，但没有抛出异常", true);
            return false;
        }
        con.reset(rawCon);
        
        // ====== 添加超时设置 ======
        log("设置连接超时选项");
        con->setClientOption("OPT_CONNECT_TIMEOUT", "5");
        con->setClientOption("OPT_READ_TIMEOUT", "10");
        con->setClientOption("OPT_WRITE_TIMEOUT", "10");
        
        // 设置数据库
        log("选择数据库: " + dbName);
        con->setSchema(dbName);
        
        // 设置字符集
        log("设置字符集为utf8mb4");
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        stmt->execute("SET NAMES 'utf8mb4'");
        stmt->execute("SET CHARACTER SET utf8mb4");
        
        // ====== 优化连接保持设置 ======
        con->setClientOption("MYSQL_OPT_KEEPALIVE_INTERVAL", "60");
        con->setClientOption("MYSQL_OPT_TCP_KEEPALIVE", "1");
        log("已启用TCP keepalive");
        
        // 执行简单查询验证连接
        log("执行连接验证查询...");
        std::unique_ptr<sql::Statement> testStmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(testStmt->executeQuery("SELECT 1 AS test_value"));
        if (res->next() && res->getInt("test_value") == 1) {
            log("连接验证成功");
            connected = true;
            return true;
        } else {
            log("连接验证失败: 查询返回意外结果", true);
            connected = false;
            return false;
        }
    } catch (sql::SQLException &e) {
        std::ostringstream oss;
        oss << "MySQL连接错误 [code:" << e.getErrorCode()
            << ", SQLState:" << e.getSQLState() << "]: "
            << e.what();
        log(oss.str(), true);
        
        // 添加详细错误诊断
        std::ostringstream params;
        params << "连接参数: \n"
               << "  主机: " << host << "\n"
               << "  端口: " << port << "\n"
               << "  用户: " << user << "\n"
               << "  数据库: " << dbName;
        log(params.str(), true);
        
        connected = false;
        return false;
    } catch (const std::exception& e) {
        std::string errorMsg = "连接错误: " + std::string(e.what());
        log(errorMsg, true);
        connected = false;
        return false;
    } catch (...) {
        log("未知连接错误", true);
        connected = false;
        return false;
    }
}



bool Database::testConnection() {
    std::lock_guard<std::mutex> lock(connectionMutex);
    log("测试数据库连接状态");
    
    // 首先检查是否已有有效连接
    if (con && !con->isClosed() && con->isValid()) {
        log("连接状态良好，无需重新连接");
        return true;
    }
    
    // 如果没有连接或连接无效，尝试重新连接
    log("连接无效或未建立，尝试重新连接");
    return connect();
}

std::vector<std::map<std::string, std::string>> Database::executeQuery(const std::string& sql) {
    std::lock_guard<std::mutex> lock(connectionMutex);
    log("Executing query: " + sql);
    std::vector<std::map<std::string, std::string>> results;
    
    // 最大重试次数
    const int maxRetries = 2;
    int attempt = 0;
    
    while (attempt <= maxRetries) {
        attempt++;
        
        try {
            // 确保连接有效（修改点1）
            if (!con || con->isClosed() || !con->isValid()) {
                log("Connection invalid, reconnecting... (attempt " + std::to_string(attempt) + ")");
                if (!connect()) {
                    log("Failed to reconnect for query", true);
                    return results; // 返回空结果
                }
            }
            
            // 创建语句对象（修改点2）
            sql::Statement* stmt = con->createStatement();
            
            // 执行查询（修改点3）
            sql::ResultSet* res = stmt->executeQuery(sql);
            
            // 获取元数据
            sql::ResultSetMetaData* meta = res->getMetaData();
            const int columns = meta->getColumnCount();
            
            // 记录列信息（优化点4）
            std::ostringstream columnsStream;
            for (int i = 1; i <= columns; ++i) {
                if (i > 1) columnsStream << ", ";
                columnsStream << meta->getColumnName(i);
            }
            log("Query returned columns: " + columnsStream.str());
            
            // 处理结果集（修改点5）
            while (res->next()) {
                std::map<std::string, std::string> row;
                for (int i = 1; i <= columns; ++i) {
                    std::string colName = meta->getColumnName(i);
                    std::transform(colName.begin(), colName.end(), colName.begin(), 
                                  [](unsigned char c){ return std::tolower(c); });
                    
                    if (res->isNull(i)) {
                        row[colName] = "";
                    } else {
                        row[colName] = res->getString(i);
                    }
                }
                results.push_back(row);
            }
            
            // 显式释放资源（关键修改点6）
            delete res;
            delete stmt;
            
            log("Query executed successfully, returned " + std::to_string(results.size()) + " rows");
            
            // 只在调试时记录详细结果
            #ifdef DEBUG
            if (!results.empty()) {
                std::ostringstream oss;
                oss << "Query result details (first row): ";
                for (const auto& col : results[0]) {
                    oss << col.first << ": " << col.second << " | ";
                }
                log(oss.str());
            }
            #endif
            
            return results;
            
        } catch (sql::SQLException &e) {
            // 处理特定错误代码（修改点7）
            if (e.getErrorCode() == 2014 || e.getErrorCode() == 2006) { // Commands out of sync or server gone away
                log("Commands out of sync, resetting connection...", true);
                disconnect(); // 强制断开
                con.reset();  // 重置连接
                
                if (attempt <= maxRetries) {
                    log("Retrying query (attempt " + std::to_string(attempt) + ")");
                    continue; // 重试查询
                }
            }
            
            // 记录详细错误（修改点8）
            std::ostringstream errorMsg;
            errorMsg << "MySQL Query Error [" << e.getErrorCode() 
                     << ", SQLState: " << e.getSQLState() << "]: " 
                     << e.what() << "\nQuery: " << sql;
            log(errorMsg.str(), true);
            
            return results;
            
        } catch (const std::exception& e) {
            std::string errorMsg = "General query error: " + std::string(e.what());
            log(errorMsg, true);
            return results;
        }
    }
    
    return results; // 所有重试失败后返回
}



int Database::executeUpdate(const std::string& sql) {
    std::lock_guard<std::mutex> lock(connectionMutex); // 使用互斥锁
    ensureConnected();
    log("Executing update: " + sql);
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for update", true);
            return -1;
        }
    }
    
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        int result = stmt->executeUpdate(sql);
        log("Update executed successfully, affected rows: " + std::to_string(result));
        return result;
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Update Error (" + sql + "): " + std::string(e.what());
        log(errorMsg, true);
        return -1;
    }
}

bool Database::addItemToList(const std::string& name, const std::string& category,
                            const std::string& grade, const std::string& effect,
                            const std::string& description, const std::string& note, const std::string& operationReason) {
    ensureConnected();
    log("Adding item to list: " + name);
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for addItemToList", true);
            return false;
        }
    }
    
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "INSERT INTO item_list (name, category, grade, effect, description, note) "
                "VALUES (?, ?, ?, ?, ?, ?)"
            )
        );
        
        pstmt->setString(1, name);
        pstmt->setString(2, category);
        pstmt->setString(3, grade);
        pstmt->setString(4, effect);
        
        if (description.empty()) {
            pstmt->setNull(5, sql::DataType::LONGVARCHAR);
        } else {
            pstmt->setString(5, description);
        }
        
        if (note.empty()) {
            pstmt->setNull(6, sql::DataType::LONGVARCHAR);
        } else {
            pstmt->setString(6, note);
        }
        
        int result = pstmt->executeUpdate();
        if (result > 0) {
            log("Item added to list successfully: " + name);
            // 记录操作日志
            std::string opNote = "类别: " + category + ", 品质: " + grade;
            if (!operationReason.empty()) {
                opNote += " | 原因: " + operationReason;
            }
            logOperation("ADD", name, opNote);
            return true;
        } else {
            log("Failed to add item to list: " + name, true);
            return false;
        }
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in addItemToList: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

bool Database::addItemToInventory(int itemId, int quantity, const std::string& location, const std::string& operationReason) {
    ensureConnected(); // 确保连接有效
    log("Adding item to inventory. ID: " + std::to_string(itemId) + ", Quantity: " + std::to_string(quantity));
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for addItemToInventory", true);
            return false;
        }
    }
    
    try {
        // 获取物品名称用于日志记录
        std::string itemName = "未知物品";
        auto itemInfo = executeQuery("SELECT name FROM item_list WHERE id = " + std::to_string(itemId));
        if (!itemInfo.empty()) {
            itemName = itemInfo[0]["name"];
        }
        
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "INSERT INTO inventory (item_id, quantity, location) "
                "VALUES (?, ?, ?)"
            )
        );
        
        pstmt->setInt(1, itemId);
        pstmt->setInt(2, quantity);
        pstmt->setString(3, location);
        
        int result = pstmt->executeUpdate();
        if (result > 0) {
            // 修改日志记录，添加操作原因
            std::string opNote = "数量: " + std::to_string(quantity) + ", 位置: " + location;
            if (!operationReason.empty()) {
                opNote += " | 原因: " + operationReason;
            }
            logOperation("ADD", itemName, opNote);
            return true;
        } else {
            log("Failed to add item to inventory. ID: " + std::to_string(itemId), true);
            return false;
        }
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in addItemToInventory: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

bool Database::itemExistsInList(const std::string& name) {
    ensureConnected(); // 确保连接有效
    log("Checking if item exists: " + name);
    try {
        if (!con || con->isClosed()) {
            log("Connection closed, attempting to reconnect...");
            if (!connect()) {
                log("Failed to connect for itemExistsInList", true);
                return false;
            }
        }
        
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT COUNT(*) FROM item_list WHERE name = ?")
        );
        pstmt->setString(1, name);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        if (res->next()) {
            int count = res->getInt(1);
            if (count > 0) {
                log("Item exists: " + name);
            } else {
                log("Item does not exist: " + name);
            }
            return count > 0;
        }
        log("Item existence check failed for: " + name, true);
        return false;
    } catch (const std::exception& e) {
        std::string errorMsg = "Error in itemExistsInList: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

int Database::getItemIdByName(const std::string& name) {
    ensureConnected(); // 确保连接有效
    log("Getting item ID by name: " + name);
    try {
        if (!con || con->isClosed()) {
            log("Connection closed, attempting to reconnect...");
            if (!connect()) {
                log("Failed to connect for getItemIdByName", true);
                return -1;
            }
        }
        
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("SELECT id FROM item_list WHERE name = ?")
        );
        pstmt->setString(1, name);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        if (res->next()) {
            int id = res->getInt("id");
            log("Found item ID: " + std::to_string(id) + " for name: " + name);
            return id;
        }
        log("Item not found: " + name, true);
        return -1;
    } catch (const std::exception& e) {
        std::string errorMsg = "Error in getItemIdByName: " + std::string(e.what());
        log(errorMsg, true);
        return -1;
    }
}

void Database::disconnect() {
    if (con && !con->isClosed()) {
        log("Disconnecting from database");
        con->close();
    }
}

// 操作日志记录方法
bool Database::logOperation(const std::string& operationType, 
                           const std::string& itemName, 
                           const std::string& note) {
    ensureConnected(); // 确保连接有效
    log("Logging operation: " + operationType + " for item: " + itemName);
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for logOperation", true);
            return false;
        }
    }
    
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "INSERT INTO operation_log (operation_type, item_name, operation_note) "
                "VALUES (?, ?, ?)"
            )
        );
        
        pstmt->setString(1, operationType);
        pstmt->setString(2, itemName);
        pstmt->setString(3, note);
        
        int result = pstmt->executeUpdate();
        if (result > 0) {
            log("Operation logged successfully");
            return true;
        } else {
            log("Failed to log operation", true);
            return false;
        }
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in logOperation: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

// 获取操作日志
// 带分页的操作日志查询
// Database.cpp
// Database.cpp 实现文件中的修改
std::vector<std::map<std::string, std::string>> Database::getOperationLogs(
    int page, 
    int perPage,  // 更合理的参数命名
    const std::string& search) 
{
    ensureConnected();
    int offset = (page - 1) * perPage;  // 使用 perPage 而不是 pageSize
    
    // 构建基础查询 - 修改别名
    std::string baseQuery = 
        "SELECT "
        "  id, "
        "  operation_type, "
        "  item_name, "
        "  DATE_FORMAT(operation_time, '%Y-%m-%d %H:%i:%s') AS formatted_time, " // 改为 formatted_time
        "  operation_note "
        "FROM operation_log ";
    
    // 准备查询和参数
    std::string fullQuery;
    std::vector<std::string> params;
    
    if (!search.empty()) {
        baseQuery += "WHERE operation_type LIKE ? OR item_name LIKE ? OR operation_note LIKE ? ";
        params = { // 一次性初始化
            "%" + search + "%",
            "%" + search + "%",
            "%" + search + "%"
        };
    }
    
    // 添加排序和分页
    fullQuery = baseQuery + 
        "ORDER BY operation_time DESC "
        "LIMIT " + std::to_string(perPage) + 
        " OFFSET " + std::to_string(offset);
    
    log("Executing operation logs query: " + fullQuery);
    if (!search.empty()) {
        log("With search term: " + search);
    }
    
    // 执行查询
    if (!con || con->isClosed() || !con->isValid()) {
        log("Connection invalid, reconnecting...");
        if (!connect()) {
            log("Failed to reconnect for getOperationLogs", true);
            return {};
        }
    }
    
    try {
        // 统一使用预处理语句（更安全）
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(fullQuery)
        );
        
        for (size_t i = 0; i < params.size(); i++) {
            pstmt->setString(i + 1, params[i]);
        }
        
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        auto results = parseResultSet(res.get());
        
        // 添加调试日志
        log("Operation logs query returned " + std::to_string(results.size()) + " rows");
        if (!results.empty()) {
            std::ostringstream oss;
            oss << "First log entry fields: ";
            for (const auto& field : results[0]) {
                oss << field.first << " ";
            }
            log(oss.str());
        }
        
        return results;
    } catch (sql::SQLException &e) {
        std::ostringstream oss;
        oss << "MySQL Error in getOperationLogs ["
            << e.getErrorCode() << "]: " << e.what()
            << "\nQuery: " << fullQuery;
        log(oss.str(), true);
        return {};
    } catch (const std::exception& e) {
        std::string errorMsg = "Error in getOperationLogs: " + std::string(e.what());
        log(errorMsg, true);
        return {};
    }
}



// 添加辅助函数：解析结果集
std::vector<std::map<std::string, std::string>> Database::parseResultSet(sql::ResultSet* res) {
    std::vector<std::map<std::string, std::string>> results;
    
    if (!res) return results;
    
    sql::ResultSetMetaData* meta = res->getMetaData();
    int columns = meta->getColumnCount();
    
    // 记录列名
    std::ostringstream columnsList;
    for (int i = 1; i <= columns; ++i) {
        if (i > 1) columnsList << ", ";
        columnsList << meta->getColumnName(i) << " (Label: " << meta->getColumnLabel(i) << ")";
    }
    log("Result set columns: " + columnsList.str());
    
    while (res->next()) {
        std::map<std::string, std::string> row;
        for (int i = 1; i <= columns; ++i) {
            // 优先使用列标签（AS 别名），如果没有则使用列名
            std::string colName = meta->getColumnLabel(i);
            if (colName.empty()) {
                colName = meta->getColumnName(i);
                log("Warning: Empty column label for column " + std::to_string(i) + 
                    ", using name: " + colName);
            }
            
            // 将列名转换为小写，确保键名一致性
            std::transform(colName.begin(), colName.end(), colName.begin(), 
                          [](unsigned char c){ return std::tolower(c); });
            
            if (res->isNull(i)) {
                row[colName] = "";
            } else {
                try {
                    row[colName] = res->getString(i);
                } catch (const sql::SQLException& e) {
                    std::ostringstream oss;
                    oss << "Error getting string for column " << colName 
                         << " (index " << i << "): " << e.what();
                    log(oss.str(), true);
                    row[colName] = "[ERROR]";
                }
            }
        }
        results.push_back(row);
    }
    
    // 记录第一行数据
    if (!results.empty()) {
        std::ostringstream oss;
        oss << "First row data: ";
        for (const auto& pair : results[0]) {
            oss << pair.first << "=" << pair.second << " | ";
        }
        log(oss.str());
    }
    
    return results;
}


// 带分页的库存查询
std::vector<std::map<std::string, std::string>> 
Database::getInventory(int page, int pageSize, const std::string& search) 
{
    ensureConnected();
    log("获取库存数据，页码: " + std::to_string(page) + 
        ", 每页: " + std::to_string(pageSize) + 
        ", 搜索: '" + search + "'");
    
    // 计算偏移量
    int offset = (page - 1) * pageSize;
    
    // 构建基础查询
    std::string query = 
        "SELECT i.id AS inventory_id, i.item_id, il.name AS item_name, "
        "i.quantity, i.location, "
        "DATE_FORMAT(i.stored_time, '%Y-%m-%d %H:%i:%s') AS stored_time, "
        "DATE_FORMAT(i.last_updated, '%Y-%m-%d %H:%i:%s') AS last_updated "
        "FROM inventory i "
        "JOIN item_list il ON i.item_id = il.id ";
    
    // 添加搜索条件
    if (!search.empty()) {
        query += "WHERE il.name LIKE ? OR i.location LIKE ? ";
    }
    
    query += "ORDER BY i.last_updated DESC "
             "LIMIT ? OFFSET ?";
    
    log("执行查询: " + query);
    
    try {
        // 准备参数化查询
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(query));
        int paramIndex = 1;
        
        // 设置搜索参数
        if (!search.empty()) {
            std::string likePattern = "%" + search + "%";
            pstmt->setString(paramIndex++, likePattern);
            pstmt->setString(paramIndex++, likePattern);
        }
        
        // 设置分页参数
        pstmt->setInt(paramIndex++, pageSize);
        pstmt->setInt(paramIndex++, offset);
        
        // 执行查询
        log("执行查询...");
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        
        log("解析结果...");
        auto results = parseResultSet(res.get());
        
        log("获取 " + std::to_string(results.size()) + " 条库存记录");
        return results;
        
    } catch (sql::SQLException &e) {
        // 详细错误处理
        std::ostringstream errorMsg;
        errorMsg << "库存查询错误 [MySQL错误 " << e.getErrorCode() << "]: " 
                 << e.what() << "\nSQL状态: " << e.getSQLState();
        
        log(errorMsg.str(), true);
        
        // 如果是连接错误，尝试重新连接
        if (e.getErrorCode() == 2013 || e.getErrorCode() == 2006) {  // CR_SERVER_LOST 或 CR_SERVER_GONE_ERROR
            log("检测到连接丢失，尝试重新连接...");
            disconnect();
            connect();
        }
        
        // 重新抛出异常让上层处理
        throw;
        
    } catch (const std::exception& e) {
        log("库存查询异常: " + std::string(e.what()), true);
        throw;
    }
}


// 按物品ID获取库存信息
std::vector<std::map<std::string, std::string>> Database::getInventoryByItemId(int itemId) {
    ensureConnected(); // 确保连接有效
    std::string query = 
        "SELECT i.id AS inventory_id, i.item_id, il.name AS item_name, "
        "i.quantity, i.location, i.stored_time, i.last_updated "
        "FROM inventory i "
        "JOIN item_list il ON i.item_id = il.id "
        "WHERE i.item_id = " + std::to_string(itemId) + " "
        "ORDER BY i.last_updated DESC";
    
    return executeQuery(query);
}

// 安全获取值的辅助函数
std::string Database::safeGet(const std::map<std::string, std::string>& data, 
                             const std::string& key, 
                             const std::string& defaultValue) {
    // 尝试别名映射
    static const std::map<std::string, std::string> aliasMap = {
        {"inventory_id", "id"},
        {"item_name", "name"}
    };
    
    std::string actualKey = key;
    auto aliasIt = aliasMap.find(key);
    if (aliasIt != aliasMap.end()) {
        actualKey = aliasIt->second;
    }
    
    // 原始键名检查
    std::string value = defaultValue; // 初始化为默认值
    
    // 查找顺序：原始键->小写键->原始列名
    auto it = data.find(actualKey);
    if (it == data.end()) {
        std::string lowerKey = actualKey;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), 
                      [](unsigned char c){ return std::tolower(c); });
        it = data.find(lowerKey);
    }
    
    if (it == data.end()) {
        it = data.find(key);
    }
    
    // 找到值时的处理
    if (it != data.end()) {
        value = it->second;
        
        // 处理NULL问题：当数据库返回"NULL"字符串时，转换为空字符串
        if (value == "NULL") {
            return defaultValue;
        }
    }
    
    return value;
}


// ====== Database.cpp 新增方法实现 ======
// 更新库存项目
bool Database::updateInventoryItem(int inventoryId, int newQuantity, const std::string& newLocation,
                                  const std::string& operationReason) {
    ensureConnected(); // 确保连接有效
    log("Updating inventory item ID: " + std::to_string(inventoryId));
    if (inventoryId <= 0) {
        log("错误：无效的库存ID: " + std::to_string(inventoryId), true);
        return false;
    }
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for updateInventoryItem", true);
            return false;
        }
    }
    
    try {
        // 获取当前库存信息用于日志记录
        log("查询库存项目 ID: " + std::to_string(inventoryId));
        auto currentItem = getInventoryItemById(inventoryId);
        if (currentItem.empty()) {
            log("Inventory item not found: " + std::to_string(inventoryId), true);
            return false;
        }
        
        std::string itemName = safeGet(currentItem[0], "item_name", "未知物品");
        std::string oldLocation = safeGet(currentItem[0], "location", "未知位置");
        // 安全获取旧数量
        int oldQuantity = 0;
        try {
            if (currentItem[0].count("quantity")) {
                oldQuantity = std::stoi(currentItem[0]["quantity"]);
            }
        } catch (const std::exception& e) {
            log("转换旧数量失败: " + std::string(e.what()), true);
        }
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "UPDATE inventory SET quantity = ?, location = ? "
                "WHERE id = ?"
            )
        );
        
        pstmt->setInt(1, newQuantity);
        pstmt->setString(2, newLocation);
        pstmt->setInt(3, inventoryId);
        
        int result = pstmt->executeUpdate();
        if (result > 0) {
            // 修改日志记录，添加变化详情和操作原因
            std::string opNote = "数量: " + std::to_string(oldQuantity) + "→" + 
                                std::to_string(newQuantity) + 
                                ", 位置: " + oldLocation + "→" + newLocation;
            
            // 确保操作原因不为空
            if (operationReason.empty()) {
                opNote += " | 原因: 未提供";
            } else {
                opNote += " | 原因: " + operationReason;
            }
            
            logOperation("UPDATE", itemName, opNote);
            return true;
        } else {
            log("Failed to update inventory item. ID: " + std::to_string(inventoryId), true);
            return false;
        }
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL 更新错误: " + std::string(e.what()) + 
                              " (错误代码: " + std::to_string(e.getErrorCode()) + ")";
        log(errorMsg, true);
        return false;
    }
}

// 删除库存项目
bool Database::deleteInventoryItem(int inventoryId, const std::string& operationReason) {
    ensureConnected(); // 确保连接有效
    log("Deleting inventory item ID: " + std::to_string(inventoryId));
    
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for deleteInventoryItem", true);
            return false;
        }
    }
    
    try {
        // 获取库存信息用于日志记录
        auto currentItem = getInventoryItemById(inventoryId);
        if (currentItem.empty()) {
            log("Inventory item not found: " + std::to_string(inventoryId), true);
            return false;
        }
        
        std::string itemName = safeGet(currentItem[0], "item_name");
        int quantity = std::stoi(safeGet(currentItem[0], "quantity"));
        std::string location = safeGet(currentItem[0], "location");
        
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "DELETE FROM inventory WHERE id = ?"
            )
        );
        
        pstmt->setInt(1, inventoryId);
        
        int result = pstmt->executeUpdate();
        if (result > 0) {
            // 修改日志记录，添加操作原因
            std::string opNote = "数量: " + std::to_string(quantity) + ", 位置: " + location;
            if (operationReason.empty()) {
                opNote += " | 原因: 未提供";
            } else {
                opNote += " | 原因: " + operationReason;
            }
            logOperation("DELETE", itemName, opNote);
            return true;
        } else {
            log("Failed to delete inventory item. ID: " + std::to_string(inventoryId), true);
            return false;
        }
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in deleteInventoryItem: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

// 获取单个库存项目
std::vector<std::map<std::string, std::string>> Database::getInventoryItemById(int inventoryId) {
    ensureConnected(); // 确保连接有效
    if (inventoryId <= 0) {
        log("无效的库存ID: " + std::to_string(inventoryId), true);
        return {};
    }
    std::string query = 
        "SELECT i.id AS inventory_id, i.item_id, il.name AS item_name, "
        "i.quantity, i.location, i.stored_time, i.last_updated "
        "FROM inventory i "
        "JOIN item_list il ON i.item_id = il.id "
        "WHERE i.id = " + std::to_string(inventoryId);
    log("执行查询: " + query);
    auto result = executeQuery(query);
    
    // 添加结果验证
    if (result.empty()) {
        log("未找到库存项目: " + std::to_string(inventoryId), true);
    } else {
        log("找到库存项目: " + std::to_string(inventoryId));
    }
    
    return result;
}

// 获取库存总数
int Database::getTotalInventoryCount() {
    ensureConnected(); // 确保连接有效
    try {
        log("Executing total inventory count query");
         // 添加连接检查
        if (!con || con->isClosed()) {
            if (!connect()) {
                log("Failed to connect for total count", true);
                return 0;
            }
        }
        // 改为直接获取第一列的值
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) FROM inventory"));
        
        if (res->next()) {
            int count = res->getInt(1); // 直接获取第一列整数值
            log("Total inventory count: " + std::to_string(count));
            return count;
        }
        log("Total count query returned no result", true);
        return 0;
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in getTotalInventoryCount: " + std::string(e.what());
        log(errorMsg, true);
        return 0;
    } catch (const std::exception& e) {
        std::string errorMsg = "Error in getTotalInventoryCount: " + std::string(e.what());
        log(errorMsg, true);
        return 0;
    }
}



// 获取操作日志总数
int Database::getTotalOperationLogsCount() {
    ensureConnected(); // 确保连接有效
    try {
        log("Executing total operation logs count query");
        // 确保连接有效
        if (!con || con->isClosed()) {
            log("Connection closed, attempting to reconnect...");
            if (!connect()) {
                log("Failed to connect for getTotalOperationLogsCount", true);
                return 0;
            }
        }
        
        // 直接获取第一列的值
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT COUNT(*) FROM operation_log"));
        
        if (res->next()) {
            int count = res->getInt(1); // 直接获取第一列整数值
            log("Total operation logs count: " + std::to_string(count));
            return count;
        }
        log("Total operation logs count query returned no result", true);
        return 0;
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in getTotalOperationLogsCount: " + std::string(e.what());
        log(errorMsg, true);
        return 0;
    } catch (const std::exception& e) {
        std::string errorMsg = "Error in getTotalOperationLogsCount: " + std::string(e.what());
        log(errorMsg, true);
        return 0;
    }
}




void Database::reloadConfig() {
    config.reload();
    logFileName = config.getString("application", "log_file", "geartracker.log");  // 使用logFileName
    
    // 重新设置日志级别
    std::string logLevel = config.getString("application", "log_level", "info");
    if (logLevel == "debug") {
        logLevelFlag = LOG_DEBUG;
    } else if (logLevel == "warning") {
        logLevelFlag = LOG_WARNING;
    } else if (logLevel == "error") {
        logLevelFlag = LOG_ERROR;
    } else {
        logLevelFlag = LOG_INFO;
    }
}

void Database::updateDatabaseCredentials(const std::string& host, int port, 
                                        const std::string& user, const std::string& password,
                                        const std::string& dbName) {
    // 更新配置对象
    config.setString("database", "host", host);
    config.setInt("database", "port", port);
    config.setString("database", "username", user);
    config.setString("database", "password", password);
    config.setString("database", "database", dbName);
    
    // 保存到文件
    config.save();
    
    // 重新连接数据库
    try {
        if (con) {
            con->close();
        }
        
        // 构建连接字符串
        std::string connectionStr = "tcp://" + host + ":" + std::to_string(port);
        // 使用 DriverManager 获取驱动实例
        driver = get_driver_instance(); // 重新获取驱动实例

        // 创建新连接
        con.reset(driver->connect(connectionStr, user, password));
        con->setSchema(dbName);
        log("数据库连接已更新，成功连接到: " + dbName);
    } catch (sql::SQLException &e) {
        std::string errorMsg = "无法更新数据库连接: " + std::string(e.what());
        log(errorMsg, true);
        throw std::runtime_error(errorMsg);
    }
}
std::vector<Database::InventoryItem> Database::getInventoryPaginated(int page, int perPage, const std::string& searchTerm) {
    ensureConnected(); // 确保连接有效
    std::vector<InventoryItem> items;
    
    try {
        log("Executing getInventoryPaginated query with search: " + (searchTerm.empty() ? "(no search)" : searchTerm));
        
        // 确保连接有效
        if (!con || con->isClosed()) {
            log("Connection closed, attempting to reconnect...");
            if (!connect()) {
                log("Failed to connect for getInventoryPaginated", true);
                return items;
            }
        }
        
        int offset = (page - 1) * perPage;
        std::string baseQuery = 
            "SELECT i.id, i.item_id, il.name AS item_name, i.quantity, i.location, "
            "i.stored_time, i.last_updated "
            "FROM inventory i JOIN item_list il ON i.item_id = il.id ";
        
        // 添加搜索条件
        if (!searchTerm.empty()) {
            baseQuery += "WHERE il.name LIKE ? OR i.location LIKE ? ";
        }
        
        // 添加排序
        baseQuery += "ORDER BY i.last_updated DESC";
        
        log("SQL: " + baseQuery);
        
        if (!searchTerm.empty()) {
            // 使用预处理语句 - 更安全
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(baseQuery + " LIMIT ? OFFSET ?"));
            
            // 设置搜索参数 (添加%通配符)
            std::string searchPattern = "%" + searchTerm + "%";
            pstmt->setString(1, searchPattern);
            pstmt->setString(2, searchPattern);
            
            // 设置分页参数
            pstmt->setInt(3, perPage);
            pstmt->setInt(4, offset);
            
            // 执行查询
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            while (res->next()) {
                InventoryItem item;
                item.id = res->getInt("id");
                item.item_id = res->getInt("item_id");
                item.item_name = res->getString("item_name");
                item.quantity = res->getInt("quantity");
                item.location = res->getString("location");
                item.stored_time = res->getString("stored_time");
                item.last_updated = res->getString("last_updated");
                items.push_back(item);
            }
        } else {
            // 没有搜索条件 - 直接拼接分页参数
            std::string fullQuery = baseQuery + " LIMIT " + std::to_string(perPage) + 
                                   " OFFSET " + std::to_string(offset);
            
            log("Full query: " + fullQuery);
            
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(fullQuery));
            
            while (res->next()) {
                InventoryItem item;
                item.id = res->getInt("id");
                item.item_id = res->getInt("item_id");
                item.item_name = res->getString("item_name");
                item.quantity = res->getInt("quantity");
                item.location = res->getString("location");
                item.stored_time = res->getString("stored_time");
                item.last_updated = res->getString("last_updated");
                items.push_back(item);
            }
        }
        
        log("Retrieved " + std::to_string(items.size()) + " inventory items");
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Error in getInventoryPaginated: " + std::string(e.what());
        log(errorMsg, true);
        
        // 添加详细错误日志
        log("SQL error code: " + std::to_string(e.getErrorCode()), true);
        log("SQL state: " + std::string(e.getSQLState()), true);
        
        // 尝试重新连接
        if (e.getErrorCode() == 2013 || e.getErrorCode() == 2006) { // 连接丢失错误代码
            log("Attempting to reconnect due to lost connection");
            if (connect()) {
                log("Reconnection successful, retrying query");
                return getInventoryPaginated(page, perPage, searchTerm);
            }
        }
    } catch (const std::exception& e) {
        std::string errorMsg = "Error in getInventoryPaginated: " + std::string(e.what());
        log(errorMsg, true);
    }
    
    return items;
}



void Database::ensureConnected() {
    std::lock_guard<std::mutex> lock(connectionMutex); // 使用互斥锁
    
    // 如果连接不存在或已关闭
    if (!con || con->isClosed()) {
        log("连接已断开，尝试重连...");
        
        // 优雅地断开现有连接
        if (con) {
            try {
                con->close();
            } catch (const sql::SQLException& e) {
                log("关闭连接时出错: " + std::string(e.what()), true);
            }
        }
        
        // 尝试重连，最多重试3次
        for (int attempt = 1; attempt <= 3; attempt++) {
            try {
                if (connect()) {
                    log("重连成功!");
                    return;
                }
            } catch (const std::exception& e) {
                log("重连尝试 " + std::to_string(attempt) + " 失败: " + e.what(), true);
            }
            
            // 指数退避策略
            std::this_thread::sleep_for(std::chrono::seconds(attempt * 2));
        }
        
        throw std::runtime_error("无法重新连接数据库");
    }
    
    // 如果连接存在，发送保活ping
    try {
        log("发送保活PING...");
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));
        if (res->next()) {
            log("连接状态正常");
        }
    } catch (const sql::SQLException& e) {
        log("保活PING失败: " + std::string(e.what()), true);
        // 如果ping失败，标记连接为断开
        if (con) {
            try {
                con->close();
            } catch (...) {
                // 忽略关闭错误
            }
        }
        con.reset(); // 重置连接
        throw; // 重新抛出异常
    }
}

