#ifndef DATABASE_H
#define DATABASE_H

#include "Config.h"
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <stdexcept>
#include <fstream>
#include <ctime>
#include <iomanip> // 用于时间格式化
#include <algorithm> // 添加这个头文件
#include <mutex>


// 日志级别常量定义 (确保与头文件一致)
const int LOG_DEBUG = 0;
const int LOG_INFO = 1;
const int LOG_WARNING = 2;
const int LOG_ERROR = 3;



class Config;


class Database {
public:

    struct InventoryItem {
        int id;
        int item_id;
        std::string item_name;
        int quantity;
        std::string location;
        std::string stored_time;
        std::string last_updated;
    };
    
    bool connect();
    bool testConnection();
    void disconnect();
    
    ~Database();
    // 查询方法
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& sql);
    
    // 更新方法
    int executeUpdate(const std::string& sql);
    
    // 物品管理方法
    // 修改以下方法的声明
    bool addItemToList(const std::string& name, const std::string& category,
                  const std::string& grade, const std::string& effect,
                  const std::string& description = "", 
                  const std::string& note = "", 
                  const std::string& operationReason = ""); // 添加默认值

    bool addItemToInventory(int itemId, int quantity, 
                        const std::string& location, 
                        const std::string& operationReason = ""); // 添加默认值

    bool updateInventoryItem(int inventoryId, int newQuantity, 
                            const std::string& newLocation, 
                            const std::string& operationReason = ""); // 添加默认值

    bool deleteInventoryItem(int inventoryId, 
                            const std::string& operationReason = ""); // 添加默认值

    
    bool itemExistsInList(const std::string& name);
    
    int getItemIdByName(const std::string& name);
    
    // 库存管理方法
    std::vector<std::map<std::string, std::string>> getInventory(int page = 1, int pageSize = 10, const std::string& search = "");
    std::vector<std::map<std::string, std::string>> getInventoryByItemId(int itemId);
    
    // 操作日志方法
    bool logOperation(const std::string& operationType, 
                     const std::string& itemName, 
                     const std::string& note = "");
    
    std::vector<std::map<std::string, std::string>> getOperationLogs(
        int page = 1, 
        int pageSize = 10, 
        const std::string& search = "");
    
    // 日志方法
    void log(const std::string& message, bool error = false);

    
    // 新增获取单条库存记录
    std::vector<std::map<std::string, std::string>> getInventoryItemById(int inventoryId);

    // 添加辅助函数用于安全获取值
    static std::string safeGet(const std::map<std::string, std::string>& data, 
                              const std::string& key, 
                              const std::string& defaultValue = "N/A");

    // 新增获取总数的方法
    int getTotalInventoryCount();
    int getTotalOperationLogsCount();
    Config& getConfig() { return config; }
    void reloadConfig();
    void updateDatabaseCredentials(const std::string& host, int port, 
                                  const std::string& user, const std::string& password,
                                  const std::string& dbName);
    // 添加分页获取库存数据的方法
    std::vector<InventoryItem> getInventoryPaginated(int page, int perPage, const std::string& search = "");
    explicit Database(Config& config);
    std::vector<std::map<std::string, std::string>> parseResultSet(sql::ResultSet* res);
    void ensureConnected();

private:
    std::mutex connectionMutex; // 添加互斥锁定义
    Database();
    

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sql::Driver* driver;
    std::unique_ptr<sql::Connection> con;
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    std::ofstream logFile; // 日志文件流
    Config config; // 配置对象
    bool logToConsole = true;  // 添加这行
    int logLevelFlag = LOG_INFO;  // 添加日志级别标志
    std::string logFileName;  // 修改为 logFileName
    bool connected;
};

#endif // DATABASE_H
