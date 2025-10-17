#ifndef DATABASE_H
#define DATABASE_H

#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <string>
#include <memory>

class Database {
public:
    Database(const std::string& host, const std::string& user, 
             const std::string& password, const std::string& database);
    
    bool connect();
    bool testConnection();
    void disconnect();

private:
    sql::Driver* driver;
    std::unique_ptr<sql::Connection> con;
    std::string host;
    std::string user;
    std::string password;
    std::string database;
};

#endif // DATABASE_H
