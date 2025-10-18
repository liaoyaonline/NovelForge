#include "Database.h"
#include <iostream>
#include <sstream>
#include <algorithm> // 添加此头文件用于std::transform
#include <cctype> // 添加这个头文件



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
    logFile.open("database.log", std::ios::app);
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
    
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
    }
    
    if (error) {
        std::cerr << logMessage << std::endl;
    }
}

bool Database::connect() {
    try {
        log("Connecting to database: tcp://" + host + ":3306");
        con = std::unique_ptr<sql::Connection>(
            driver->connect("tcp://" + host + ":3306", user, password)
        );
        con->setSchema(database);
        
        // 设置字符集为UTF8，确保支持中文
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        stmt->execute("SET NAMES 'utf8mb4'");
        
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
                // 将列名转换为小写，确保键名一致性
                std::transform(colName.begin(), colName.end(), colName.begin(), 
                              [](unsigned char c){ return std::tolower(c); });
                
                if (res->isNull(i)) {
                    row[colName] = "NULL";
                } else {
                    row[colName] = res->getString(i);
                }
            }
            results.push_back(row);
        }
        // 记录返回的列名
        std::string columnsList;
        for (int i = 1; i <= columns; ++i) {
            if (i > 1) columnsList += ", ";
            columnsList += meta->getColumnName(i);
        }
        log("Query returned columns: " + columnsList);
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
                            const std::string& description, const std::string& note, const std::string& operationReason) {
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

// 操作日志记录方法
bool Database::logOperation(const std::string& operationType, 
                           const std::string& itemName, 
                           const std::string& note) {
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
std::vector<std::map<std::string, std::string>> Database::getOperationLogs(int limit) {
    std::string query = "SELECT id, operation_type, item_name, operation_time, operation_note "
                        "FROM operation_log "
                        "ORDER BY operation_time DESC "
                        "LIMIT " + std::to_string(limit);
    
    return executeQuery(query);
}

// 获取库存信息（联表查询）
std::vector<std::map<std::string, std::string>> Database::getInventory() {
    std::string query = 
        "SELECT i.id AS inventory_id, i.item_id, il.name AS item_name, "
        "i.quantity, i.location, i.stored_time, i.last_updated "
        "FROM inventory i "
        "JOIN item_list il ON i.item_id = il.id "
        "ORDER BY i.last_updated DESC";
    
    return executeQuery(query);
}

// 按物品ID获取库存信息
std::vector<std::map<std::string, std::string>> Database::getInventoryByItemId(int itemId) {
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
    if (auto it = data.find(actualKey); it != data.end()) {
        return it->second;
    }
    
    // 小写检查
    std::string lowerKey = actualKey;
    std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), 
                  [](unsigned char c){ return std::tolower(c); });
    if (auto it = data.find(lowerKey); it != data.end()) {
        return it->second;
    }
    
    // 原始列名检查
    if (auto it = data.find(key); it != data.end()) {
        return it->second;
    }
    
    return defaultValue;
}

// ====== Database.cpp 新增方法实现 ======
// 更新库存项目
bool Database::updateInventoryItem(int inventoryId, int newQuantity, const std::string& newLocation,
                                  const std::string& operationReason) {
    log("Updating inventory item ID: " + std::to_string(inventoryId));
    
    if (!con || con->isClosed()) {
        log("Connection closed, attempting to reconnect...");
        if (!connect()) {
            log("Failed to connect for updateInventoryItem", true);
            return false;
        }
    }
    
    try {
        // 获取当前库存信息用于日志记录
        auto currentItem = getInventoryItemById(inventoryId);
        if (currentItem.empty()) {
            log("Inventory item not found: " + std::to_string(inventoryId), true);
            return false;
        }
        
        std::string itemName = safeGet(currentItem[0], "item_name");
        int oldQuantity = std::stoi(safeGet(currentItem[0], "quantity"));
        std::string oldLocation = safeGet(currentItem[0], "location");
        
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
        std::string errorMsg = "MySQL Error in updateInventoryItem: " + std::string(e.what());
        log(errorMsg, true);
        return false;
    }
}

// 删除库存项目
bool Database::deleteInventoryItem(int inventoryId, const std::string& operationReason) {
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
    std::string query = 
        "SELECT i.id AS inventory_id, i.item_id, il.name AS item_name, "
        "i.quantity, i.location, i.stored_time, i.last_updated "
        "FROM inventory i "
        "JOIN item_list il ON i.item_id = il.id "
        "WHERE i.id = " + std::to_string(inventoryId);
    
    return executeQuery(query);
}
