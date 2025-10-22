#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include "httplib.h"
#include "Database.h"

// 将 OperationLogEntry 定义在类内部
class WebServer {
public:
    // 在类内部定义 OperationLogEntry
    struct OperationLogEntry {
        int id;
        std::string operation_type;
        std::string item_name;
        std::string operation_time_str; // 直接存储格式化后的时间字符串
        std::string operation_note;
    };

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
    std::string getInventoryData(int page, int perPage);
    std::string getOperationLogs(int page, int perPage);
    std::time_t parseDateTime(const std::string& datetimeStr);
    
    int port_;
    Database& db_;
    std::unique_ptr<httplib::Server> server;
    std::thread serverThread;
    bool running = false;
    std::mutex serverMutex;
};

#endif // WEBSERVER_H
