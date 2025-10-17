#include "Database.h"
#include <iostream>

Database::Database(const std::string& host, const std::string& user, 
                   const std::string& password, const std::string& database)
    : driver(nullptr), host(host), user(user), password(password), database(database) 
{
    // 使用正确的函数获取驱动实例
    driver = get_driver_instance();
}

bool Database::connect() {
    try {
        con = std::unique_ptr<sql::Connection>(
            driver->connect("tcp://" + host + ":3306", user, password)
        );
        con->setSchema(database);
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "MySQL Connection Error: " << e.what() << std::endl;
        return false;
    }
}

bool Database::testConnection() {
    if (!con || con->isClosed()) {
        if (!connect()) return false;
    }
    
    try {
        std::unique_ptr<sql::Statement> stmt(con->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));
        return res->next() && res->getInt(1) == 1;
    } catch (sql::SQLException &e) {
        std::cerr << "MySQL Test Query Error: " << e.what() << std::endl;
        return false;
    }
}

void Database::disconnect() {
    if (con && !con->isClosed()) {
        con->close();
    }
}
