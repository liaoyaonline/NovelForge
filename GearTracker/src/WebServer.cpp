#include "WebServer.h"
#include "Config.h"
#include "Database.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <json/json.h> // 需要包含jsoncpp库
#include <nlohmann/json.hpp> // 确保包含 JSON 库头文件
#include <ctime> // 用于时间处理


using json = nlohmann::json;

WebServer::WebServer(int port, Database& db)
    : port_(port), db_(db), serverThread() {
    server = std::make_unique<httplib::Server>();
    running = false;
    
    std::cout << "WebServer 初始化完成，端口: " << port_ << std::endl;
}
WebServer::~WebServer() {
    stop();
}


void WebServer::start() {
    std::lock_guard<std::mutex> lock(serverMutex);
    if (running) return;
    
    setupRoutes();
    
    running = true;
    serverThread = std::thread([this]() {
        server->set_mount_point("/", "./web");
        server->set_file_extension_and_mimetype_mapping("js", "application/javascript");
        server->set_file_extension_and_mimetype_mapping("css", "text/css");
        server->set_file_extension_and_mimetype_mapping("html", "text/html");
        
        server->set_error_handler([](const httplib::Request &, httplib::Response &res) {
            res.set_content("<h1>Resource Not Found</h1>", "text/html");
        });
        std::cout << "启动Web服务器在端口: " << port_ << std::endl;
        server->listen("0.0.0.0", port_);
    });
    serverThread.detach(); // 或使用join，根据需求
}


void WebServer::stop() {
    if (running) {
        server->stop();
        if (serverThread.joinable()) {
            serverThread.join();
        }
        running = false;
    }
}

bool WebServer::isRunning() const {
    return running;
}

void WebServer::setupRoutes() {
    // API端点 - 库存数据
    server->Get("/api/inventory", [this](const httplib::Request &req, httplib::Response &res) {
        int page = 1;
        int perPage = 10;
        
        if (req.has_param("page")) {
            page = std::stoi(req.get_param_value("page"));
        }
        
        if (req.has_param("perPage")) {
            perPage = std::stoi(req.get_param_value("perPage"));
        }
        
        // 传递数据库引用给辅助函数
        res.set_content(getInventoryData(page, perPage), "application/json");
    });
    
    // API端点 - 操作日志
    server->Get("/api/operation_logs", [this](const httplib::Request &req, httplib::Response &res) {
        int page = 1;
        int perPage = 10;
        
        if (req.has_param("page")) {
            page = std::stoi(req.get_param_value("page"));
        }
        
        if (req.has_param("perPage")) {
            perPage = std::stoi(req.get_param_value("perPage"));
        }
        
        // 传递数据库引用给辅助函数
        res.set_content(getOperationLogs(page, perPage), "application/json");
    });
    
    // 其他路由...
}


std::string WebServer::getInventoryData(int page, int perPage) {
    Json::Value root;
    Json::Value items(Json::arrayValue);
    
    try {
        // 直接使用传入的数据库引用
        int total = db_.getTotalInventoryCount();
        auto invItems = db_.getInventoryPaginated(page, perPage);
        
        root["total"] = total;
        root["page"] = page;
        root["perPage"] = perPage;
        
        for (const auto& item : invItems) {
            Json::Value itemObj;
            itemObj["id"] = item.id;
            itemObj["item_id"] = item.item_id;
            itemObj["item_name"] = item.item_name;
            itemObj["quantity"] = item.quantity;
            itemObj["location"] = item.location;
            itemObj["stored_time"] = item.stored_time;
            itemObj["last_updated"] = item.last_updated;
            items.append(itemObj);
        }
        
        root["items"] = items;
    } catch (const std::exception& e) {
        root["error"] = e.what();
    }
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, root);
}


std::string WebServer::getOperationLogs(int page, int perPage) {
    try {
        auto logs = getOperationLogsPaginated(page, perPage);
        nlohmann::json result;
        result["status"] = "success";
        result["page"] = page;
        result["perPage"] = perPage;
        
        nlohmann::json logsArray = nlohmann::json::array();
        for (const auto& log : logs) {
            nlohmann::json logJson;
            logJson["id"] = log.id;
            logJson["operation_type"] = log.operation_type;
            logJson["item_name"] = log.item_name;
            
            // 格式化时间
            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&log.operation_time));
            logJson["operation_time"] = buffer;
            
            logJson["operation_note"] = log.operation_note;
            logsArray.push_back(logJson);
        }
        result["logs"] = logsArray;
        
        result["totalItems"] = getOperationLogsCount(); 
        result["totalPages"] = (result["totalItems"].get<int>() + perPage - 1) / perPage;
        
        return result.dump();
        
    } catch (const std::exception& e) {
        nlohmann::json error;
        error["status"] = "error";
        error["message"] = "获取操作日志失败: " + std::string(e.what());
        return error.dump();
    }
}


// WebServer.cpp
std::vector<OperationLogEntry> WebServer::getOperationLogsPaginated(int page, int perPage) {
    std::vector<OperationLogEntry> logs;
    
    // 使用 Database 已有的方法
    auto logData = db_.getOperationLogs(page, perPage);
    
    for (const auto& record : logData) {
        OperationLogEntry entry;
        entry.id = std::stoi(Database::safeGet(record, "id"));
        entry.operation_type = Database::safeGet(record, "operation_type");
        entry.item_name = Database::safeGet(record, "item_name");
        entry.operation_time = parseDateTime(Database::safeGet(record, "operation_time"));
        entry.operation_note = Database::safeGet(record, "operation_note");
        logs.push_back(entry);
    }
    
    return logs;
}

int WebServer::getOperationLogsCount() {
    return db_.getTotalOperationLogsCount();
}



// ======== 时间解析辅助函数 ========
// WebServer.cpp
std::time_t WebServer::parseDateTime(const std::string& datetimeStr) {
    std::tm tm = {};
    std::istringstream ss(datetimeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("Invalid datetime format: " + datetimeStr);
    }
    return std::mktime(&tm);
}
