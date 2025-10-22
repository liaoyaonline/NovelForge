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
    // 在 setupRoutes 函数中添加
    server->Delete("/api/inventory/:id", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            int id = std::stoi(req.path_params.at("id"));
            auto params = json::parse(req.body);
            std::string reason = params["reason"];
            
            if (db_.deleteInventoryItem(id, reason)) {
                res.set_content(json{{"success", true}, {"message", "删除成功"}}.dump(), "application/json");
            } else {
                res.set_content(json{{"success", false}, {"message", "删除失败"}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
    });
    // 在 setupRoutes 函数中添加
    server->Get("/api/inventory/item/:id", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            int id = std::stoi(req.path_params.at("id"));
            auto itemData = db_.getInventoryItemById(id);
            
            if (!itemData.empty()) {
                // 添加详细的调试输出
                std::ostringstream dataStream;
                dataStream << "dqftest1Raw item data: ";
                for (const auto& field : itemData[0]) {
                    dataStream << field.first << "=" << field.second << " | ";
                }
                db_.log(dataStream.str());
                db_.log("返回库存项目数据: inventory_id=" + itemData[0]["inventory_id"] +
                        ", item_id=" + itemData[0]["item_id"] +
                        ", item_name=" + itemData[0]["item_name"]);
                
                // 确保使用正确的字段名
                res.set_content(json{
                    {"inventory_id", itemData[0]["id"]},  // 使用完整的字段名
                    {"item_id", itemData[0]["item_id"]},
                    {"item_name", itemData[0]["name"]},
                    {"quantity", itemData[0]["quantity"]},
                    {"location", itemData[0]["location"]}
                }.dump(), "application/json");
            } else {
                res.set_content(json{{"error", "未找到库存项目"}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });


    server->Put("/api/inventory/:id", [this](const httplib::Request &req, httplib::Response &res) {
        try {
            // 安全的ID转换
            std::string idStr = req.path_params.at("id");
            db_.log("收到更新请求，库存ID: " + idStr + ", 数据: " + req.body);
            int id = 0;
            if (!idStr.empty()) {
                try {
                    id = std::stoi(idStr);
                } catch (const std::invalid_argument& e) {
                    db_.log("无效的库存ID: " + idStr, true);
                    res.set_content(json{{"success", false}, {"error", "无效的库存ID"}}.dump(), "application/json");
                    return;
                }
            }
            
            // 解析请求体
            auto params = json::parse(req.body);
            
            // 安全的quantity转换
            int quantity = 0;
            if (params["quantity"].is_number()) {
                quantity = params["quantity"];
            } else if (params["quantity"].is_string()) {
                try {
                    quantity = std::stoi(params["quantity"].get<std::string>());
                } catch (...) {
                    db_.log("无效的数量值: " + params["quantity"].dump(), true);
                    res.set_content(json{{"success", false}, {"error", "无效的数量值"}}.dump(), "application/json");
                    return;
                }
            }
            
            std::string location = params["location"];
            std::string reason = params["reason"];
            
            // 更新数据库
            if (db_.updateInventoryItem(id, quantity, location, reason)) {
                res.set_content(json{{"success", true}, {"message", "更新成功"}}.dump(), "application/json");
            } else {
                res.set_content(json{{"success", false}, {"message", "更新失败"}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            db_.log("Web API 更新错误: " + std::string(e.what()), true);
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
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
        Json::Value result;
        result["status"] = "success";
        result["page"] = page;
        result["perPage"] = perPage;
        
        Json::Value logsArray(Json::arrayValue);
        for (const auto& log : logs) {
            Json::Value logJson;
            logJson["id"] = log.id;
            logJson["operation_type"] = log.operation_type;
            logJson["item_name"] = log.item_name;
            logJson["operation_time"] = log.operation_time_str;
            logJson["operation_note"] = log.operation_note;
            logsArray.append(logJson);
        }
        result["logs"] = logsArray;
        
        result["totalItems"] = getOperationLogsCount(); 
        int totalPages = (result["totalItems"].asInt() + perPage - 1) / perPage;
        if (totalPages == 0) totalPages = 1;
        result["totalPages"] = totalPages;
        
        Json::StreamWriterBuilder builder;
        return Json::writeString(builder, result);
    } catch (const std::exception& e) {
        Json::Value error;
        error["status"] = "error";
        error["message"] = "获取操作日志失败: " + std::string(e.what());
        Json::StreamWriterBuilder builder;
        return Json::writeString(builder, error);
    }
}

// WebServer.cpp
std::vector<WebServer::OperationLogEntry> WebServer::getOperationLogsPaginated(int page, int perPage) {
    std::vector<OperationLogEntry> logs;
    
    // 使用 Database 已有的方法
    auto logData = db_.getOperationLogs(page, perPage);
    
    for (const auto& record : logData) {
        OperationLogEntry entry;
        entry.id = std::stoi(Database::safeGet(record, "id"));
        entry.operation_type = Database::safeGet(record, "operation_type");
        entry.item_name = Database::safeGet(record, "item_name");
        entry.operation_time_str = Database::safeGet(record, "operation_time"); // 直接使用数据库返回的格式化时间
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
