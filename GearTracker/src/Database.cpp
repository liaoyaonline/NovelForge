#include "Database.h"
#include <iostream>
#include <iomanip> // 用于格式化时间

// 获取当前时间的字符串表示
std::string currentDateTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);
    return buf;
}

Database::Database(const std::string& host, const std::string& user, 
                   const std::string& password, const std::string& database)
    : driver(nullptr), host(host), user(user), password(password), database(database) 
{
    driver = get_driver_instance();
    logFile.open("database.log", std::ios::app); // 追加模式打开日志文件
    log("Database object created");
}

Database::~Database() {
    if (logFile.is_open()) {
        log("Database object destroyed");
        logFile.close();
    }
    disconnect();
}

void Database::log(const std::string& message, bool error) {
    std::string logMessage = "[" + currentDateTime() + "] " + message;
    
    // 写入日志文件
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
    }
    
    // 如果是错误消息，同时输出到标准错误
    if (error) {
        std::cerr << logMessage << std::endl;
    }
    // 普通消息输出到标准输出
    else {
        std::cout << logMessage << std::endl;
    }
}

bool Database::connect() {
    try {
        log("Connecting to database: tcp://" + host + ":3306");
        con = std::unique_ptr<sql::Connection>(
            driver->connect("tcp://" + host + ":3306", user, password)
        );
        con->setSchema(database);
        log("Connected to database successfully");
        return true;
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Connection Error: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

bool Database::testConnection() {
    log("Testing database connection...");
    if (!con || con->isClosed()) {
        if (!connect()) {
            log("Test connection failed: cannot connect to database", true);
            return false;
        }
    }
    
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));
        bool success = res->next() && res->getInt(1) == 1;
        
        if (success) {
            log("Database test query successful");
        } else {
            log("Database test query failed: unexpected result", true);
        }
        
        return success;
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Test Query Error: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

std::vector<std::map<std::string, std::string>> Database::executeQuery(const std::string& sql) {
    log("Executing query: " + sql);
    std::vector<std::map<std::string, std::string>> results;
    
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for query", true);
            return results;
        }
    }
    
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(sql));
        
        sql::ResultSetMetaData* meta = res->getMetaData();
        int columns = meta->getColumnCount();
        
        while (res->next()) {
            std::map<std::string, std::string> row;
            for (int i = 1; i <= columns; ++i) {
                std::string colName = meta->getColumnName(i);
                if (res->isNull(i)) {
                    row[colName] = "NULL";
                } else {
                    row[colName] = res->getString(i);
                }
            }
            results.push_back(row);
        }
        
        log("Query executed successfully, returned " + std::to_string(results.size()) + " rows");
        return results;
    } catch (sql::SQLException &e) {
        std::string errorMsg = "MySQL Query Error (" + sql + "): " + std::string(e.what());
        log(errorMsg, true);
        return results;
    }
}

int Database::executeUpdate(const std::string& sql) {
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
                            const std::string& description, const std::string& note) {
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

bool Database::addItemToInventory(int itemId, int quantity, const std::string& location) {
    log("Adding item to inventory. ID: " + std::to_string(itemId) + ", Quantity: " + std::to_string(quantity));
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for addItemToInventory", true);
            return false;
        }
    }
    
    try {
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
            log("Item added to inventory successfully. ID: " + std::to_string(itemId));
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
