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
#include <fstream> // 添加文件流支持
#include <ctime>   // 添加时间处理支持

class Database {
public:
    Database(const std::string& host, const std::string& user, 
             const std::string& password, const std::string& database);
    ~Database(); // 添加析构函数以关闭日志文件
    
    bool connect();
    bool testConnection();
    void disconnect();
    
    // 查询方法
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& sql);
    
    // 更新方法
    int executeUpdate(const std::string& sql);
    
    // 物品管理方法
    bool addItemToList(const std::string& name, const std::string& category,
                      const std::string& grade, const std::string& effect,
                      const std::string& description = "", const std::string& note = "");
    
    bool addItemToInventory(int itemId, int quantity, const std::string& location);
    
    bool itemExistsInList(const std::string& name);
    
    int getItemIdByName(const std::string& name);
    
    // 日志方法
    void log(const std::string& message, bool error = false);

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
