#include <mysql/mysql.h>
#include <iostream>

int main() {
    MYSQL *conn = mysql_init(nullptr);
    if (!conn) {
        std::cerr << "MySQL init failed" << std::endl;
        return 1;
    }
    std::cout << "MySQL client version: " << mysql_get_client_info() << std::endl;
    mysql_close(conn);
    return 0;
}
