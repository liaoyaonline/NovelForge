#include "Database.h"
#include <iostream>

int main() {
    // 数据库连接参数
    const std::string host = "192.168.1.7";
    const std::string user = "vm_liaoya";
    const std::string password = "123";
    const std::string database = "geartracker";

    // 创建数据库对象
    Database db(host, user, password, database);
    
    // 测试连接
    if (db.connect()) {
        std::cout << "Connected to MySQL database successfully!" << std::endl;
        
        // 测试查询
        if (db.testConnection()) {
            std::cout << "Database test query executed successfully!" << std::endl;
        } else {
            std::cerr << "Database test query failed!" << std::endl;
        }
    } else {
        std::cerr << "Failed to connect to database." << std::endl;
        return 1;
    }

    std::cout << "Program executed successfully." << std::endl;
    return 0;
}
