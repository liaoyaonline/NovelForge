#ifndef DATABASE_H
#define DATABASE_H

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


class Database {
public:
    Database(const std::string& host, const std::string& user, 
             const std::string& password, const std::string& database);
    ~Database();
    
    bool connect();
    bool testConnection();
    void disconnect();
    
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
    std::vector<std::map<std::string, std::string>> getInventory();
    std::vector<std::map<std::string, std::string>> getInventoryByItemId(int itemId);
    
    // 操作日志方法
    bool logOperation(const std::string& operationType, 
                     const std::string& itemName, 
                     const std::string& note = "");
    
    std::vector<std::map<std::string, std::string>> getOperationLogs(int limit = 50);
    
    // 日志方法
    void log(const std::string& message, bool error = false);

    
    // 新增获取单条库存记录
    std::vector<std::map<std::string, std::string>> getInventoryItemById(int inventoryId);

    // 添加辅助函数用于安全获取值
    static std::string safeGet(const std::map<std::string, std::string>& data, 
                              const std::string& key, 
                              const std::string& defaultValue = "N/A");

private:
    sql::Driver* driver;
    std::unique_ptr<sql::Connection> con;
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    std::ofstream logFile; // 日志文件流
};

#endif // DATABASE_H
