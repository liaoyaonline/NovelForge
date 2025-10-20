#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <memory>
#include <mutex>
#include <thread> // 包含线程头文件 ✅
#include "httplib.h"
#include "Database.h"

struct OperationLogEntry {
    int id;
    std::string operation_type;
    std::string item_name;
    std::time_t operation_time;
    std::string operation_note;
};

class WebServer {
public:
    WebServer(int port, Database& db);
    ~WebServer();
    
    void start();
    void stop();
    bool isRunning() const;
    
    // 操作日志相关方法
    std::vector<OperationLogEntry> getOperationLogsPaginated(int page, int perPage);
    int getOperationLogsCount();
    
private:
    void setupRoutes();
    std::string getInventoryData(int page, int perPage); // ✅ 修正声明
    std::string getOperationLogs(int page, int perPage);
    std::time_t parseDateTime(const std::string& datetimeStr);
    
    int port_;
    Database& db_;
    std::unique_ptr<httplib::Server> server;
    std::thread serverThread; // ✅ 添加线程成员
    bool running = false;
    std::mutex serverMutex;
};

#endif // WEBSERVER_H
