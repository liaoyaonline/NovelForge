#ifndef DB_CONFIG_H
#define DB_CONFIG_H

#include <string>

struct DBConfig {
    std::string host;
    std::string user;
    std::string password;
    std::string database;
    int port = 3306;
};

DBConfig loadConfig(const std::string& path);

#endif // DB_CONFIG_H
