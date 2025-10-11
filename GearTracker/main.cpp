#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <iostream>
#include <ctime>

class WarehouseManager {
private:
    sql::mysql::MySQL_Driver* driver;
    sql::Connection* con;

    // 数据库连接配置
    const std::string host = "tcp://127.0.0.1:3306";
    const std::string user = "your_username";
    const std::string pass = "your_password";
    const std::string db = "warehouse_db";

public:
    WarehouseManager() {
        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect(host, user, pass);
        con->setSchema(db);
    }

    ~WarehouseManager() {
        delete con;
    }

    // 添加新物品到物品栏
    void addItemToInventory(const std::string& name, int quantity, const std::string& location, const std::string& comment) {
        try {
            // 检查物品是否存在
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "SELECT id, type, grade, effect FROM item_list WHERE name = ?");
            pstmt->setString(1, name);
            sql::ResultSet* res = pstmt->executeQuery();

            int itemId = -1;
            std::string type, grade, effect;

            if (!res->next()) {
                // 物品不存在，需要创建新物品
                delete res;
                delete pstmt;

                std::cout << "新物品! 请输入必填信息:\n";
                std::cout << "种类: "; std::cin >> type;
                std::cout << "等阶: "; std::cin >> grade;
                std::cin.ignore(); // 清除输入缓冲
                std::cout << "效果: "; std::getline(std::cin, effect);

                // 创建新物品
                pstmt = con->prepareStatement(
                    "INSERT INTO item_list (name, type, grade, effect) VALUES (?,?,?,?)");
                pstmt->setString(1, name);
                pstmt->setString(2, type);
                pstmt->setString(3, grade);
                pstmt->setString(4, effect);
                pstmt->execute();
                delete pstmt;

                // 获取新物品ID
                pstmt = con->prepareStatement("SELECT LAST_INSERT_ID()");
                res = pstmt->executeQuery();
                res->next();
                itemId = res->getInt(1);
            }
            else {
                // 使用现有物品
                itemId = res->getInt("id");
                type = res->getString("type");
                grade = res->getString("grade");
                effect = res->getString("effect");
            }

            delete res;
            delete pstmt;

            // 添加到物品栏
            pstmt = con->prepareStatement(
                "INSERT INTO inventory_list (item_id, quantity, location) VALUES (?,?,?)");
            pstmt->setInt(1, itemId);
            pstmt->setInt(2, quantity);
            pstmt->setString(3, location);
            pstmt->execute();
            delete pstmt;

            // 记录操作
            recordOperation(name, "ADD", comment);

        }
        catch (sql::SQLException& e) {
            std::cerr << "SQL Error: " << e.what() << std::endl;
        }
    }

    // 删除物品
    void deleteItem(int inventoryId, const std::string& comment) {
        try {
            // 获取物品名
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "SELECT i.name FROM inventory_list inv "
                "JOIN item_list i ON inv.item_id = i.id "
                "WHERE inv.id = ?");
            pstmt->setInt(1, inventoryId);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next()) {
                std::string itemName = res->getString("name");

                // 删除物品栏记录
                delete pstmt;
                pstmt = con->prepareStatement("DELETE FROM inventory_list WHERE id = ?");
                pstmt->setInt(1, inventoryId);
                pstmt->execute();

                // 记录操作
                recordOperation(itemName, "DELETE", comment);
            }
            delete res;
            delete pstmt;

        }
        catch (sql::SQLException& e) {
            std::cerr << "SQL Error: " << e.what() << std::endl;
        }
    }

    // 更新物品数量
    void updateItemQuantity(int inventoryId, int newQuantity, const std::string& comment) {
        try {
            // 获取物品名
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "SELECT i.name FROM inventory_list inv "
                "JOIN item_list i ON inv.item_id = i.id "
                "WHERE inv.id = ?");
            pstmt->setInt(1, inventoryId);
            sql::ResultSet* res = pstmt->executeQuery();

            if (res->next()) {
                std::string itemName = res->getString("name");

                // 更新数量
                delete pstmt;
                pstmt = con->prepareStatement(
                    "UPDATE inventory_list SET quantity = ? WHERE id = ?");
                pstmt->setInt(1, newQuantity);
                pstmt->setInt(2, inventoryId);
                pstmt->execute();

                // 记录操作
                recordOperation(itemName, "UPDATE", comment);
            }
            delete res;
            delete pstmt;

        }
        catch (sql::SQLException& e) {
            std::cerr << "SQL Error: " << e.what() << std::endl;
        }
    }

    // 显示所有物品
    void displayAllItems() {
        try {
            sql::Statement* stmt = con->createStatement();
            sql::ResultSet* res = stmt->executeQuery(
                "SELECT inv.id, i.name, i.type, i.grade, i.effect, "
                "inv.quantity, inv.location, inv.in_time "
                "FROM inventory_list inv "
                "JOIN item_list i ON inv.item_id = i.id");

            std::cout << "ID\t名称\t种类\t等阶\t数量\t位置\n";
            while (res->next()) {
                std::cout << res->getInt("id") << "\t"
                    << res->getString("name") << "\t"
                    << res->getString("type") << "\t"
                    << res->getString("grade") << "\t"
                    << res->getInt("quantity") << "\t"
                    << res->getString("location") << "\n";
            }
            delete res;
            delete stmt;
        }
        catch (sql::SQLException& e) {
            std::cerr << "SQL Error: " << e.what() << std::endl;
        }
    }

    // 显示操作记录
    void displayOperationLog() {
        try {
            sql::Statement* stmt = con->createStatement();
            sql::ResultSet* res = stmt->executeQuery(
                "SELECT item_name, operation_type, operation_time, comment "
                "FROM operation_log ORDER BY operation_time DESC");

            std::cout << "时间\t\t操作\t物品\t说明\n";
            while (res->next()) {
                std::cout << res->getString("operation_time") << "\t"
                    << res->getString("operation_type") << "\t"
                    << res->getString("item_name") << "\t"
                    << res->getString("comment") << "\n";
            }
            delete res;
            delete stmt;
        }
        catch (sql::SQLException& e) {
            std::cerr << "SQL Error: " << e.what() << std::endl;
        }
    }

private:
    // 记录操作日志
    void recordOperation(const std::string& itemName,
        const std::string& operationType,
        const std::string& comment) {
        try {
            sql::PreparedStatement* pstmt = con->prepareStatement(
                "INSERT INTO operation_log (item_name, operation_type, comment) "
                "VALUES (?, ?, ?)");
            pstmt->setString(1, itemName);
            pstmt->setString(2, operationType);
            pstmt->setString(3, comment);
            pstmt->execute();
            delete pstmt;
        }
        catch (sql::SQLException& e) {
            std::cerr << "Log Error: " << e.what() << std::endl;
        }
    }
};
