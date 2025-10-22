#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include "httplib.h"
#include "Config.h"  // 改为包含 Config.h 而不是 Database.h
#include "Database.h"

// 将 OperationLogEntry 定义在类内部
class WebServer {
public:

    WebServer(int port, Config& config);
    ~WebServer();
    
    void start();
    void stop();
    bool isRunning() const;
    
private:
    Config& config_;  // 修改为保存 Config 引用
    void setupRoutes();
    
    int port_;
    std::unique_ptr<httplib::Server> server;
    std::thread serverThread;
    bool running = false;
    std::mutex serverMutex;
};

#endif // WEBSERVER_H
