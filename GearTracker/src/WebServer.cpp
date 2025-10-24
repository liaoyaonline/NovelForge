#include "WebServer.h"
#include "Config.h"
#include "Database.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <json/json.h>
#include <nlohmann/json.hpp>
#include <ctime>

using json = nlohmann::json;

WebServer::WebServer(int port, Config& config)
    : port_(port), 
      config_(config),
      server(std::make_unique<httplib::Server>()),
      running(false) {
    serverThread = std::thread();
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
        
        server->set_read_timeout(20);
        server->set_write_timeout(20);
        
        server->set_logger([](const httplib::Request& req, const httplib::Response& res) {
            std::string log = "Request: " + req.method + " " + req.path + " -> " + std::to_string(res.status);
            if (res.status >= 400) {
                log += " Error: " + res.body.substr(0, 100);
            }
            std::cout << log << std::endl;
        });
        
        std::cout << "启动Web服务器在端口: " << port_ << std::endl;
        server->listen("0.0.0.0", port_);
    });
}

void WebServer::stop() {
    if (running) {
        running = false;
        server->stop();
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }
}

bool WebServer::isRunning() const {
    return running;
}

void WebServer::setupRoutes() {
    // API端点 - 库存数据
    server->Get("/api/inventory", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_); // 创建独立数据库实例
        // 强制测试连接
        if (!db.testConnection()) {
            res.status = 500;
            nlohmann::json error = {
                {"error", "无法连接数据库"},
                {"code", "DB_CONNECTION_FAILED"}
            };
            res.set_content(error.dump(), "application/json");
            return;
        }
        try {
            int page = 1;
            int perPage = 10;
            std::string searchTerm = "";
            
            if (req.has_param("page")) {
                try {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1) page = 1;
                } catch (const std::exception& e) {
                    // 使用局部db实例
                    db.log("无效的分页参数: " + std::string(e.what()), true);
                }
            }
            
            if (req.has_param("perPage")) {
                try {
                    perPage = std::stoi(req.get_param_value("perPage"));
                    if (perPage < 1) perPage = 10;
                    if (perPage > 100) perPage = 100;
                } catch (const std::exception& e) {
                    db.log("无效的每页数量参数: " + std::string(e.what()), true);
                }
            }
            
            if (req.has_param("search")) {
                searchTerm = req.get_param_value("search");
            }
            
            db.log("收到 /api/inventory 请求: page=" + std::to_string(page) + 
                   ", perPage=" + std::to_string(perPage) + 
                   ", search='" + searchTerm + "'");
            
            // 使用局部db实例
            if (!db.testConnection()) {
                res.status = 500;
                res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
                return;
            }
            
            try {
                auto inventoryData = db.getInventory(page, perPage, searchTerm);
                int totalItems = db.getTotalInventoryCount();
                
                Json::Value root;
                Json::Value items(Json::arrayValue);
                
                for (const auto& item : inventoryData) {
                    Json::Value itemObj;
                    itemObj["id"] = std::stoi(Database::safeGet(item, "inventory_id", "0"));
                    itemObj["item_id"] = std::stoi(Database::safeGet(item, "item_id", "0"));
                    itemObj["item_name"] = Database::safeGet(item, "item_name");
                    itemObj["quantity"] = std::stoi(Database::safeGet(item, "quantity"));
                    itemObj["location"] = Database::safeGet(item, "location");
                    itemObj["stored_time"] = Database::safeGet(item, "stored_time");
                    itemObj["last_updated"] = Database::safeGet(item, "last_updated");
                    items.append(itemObj);
                }
                
                root["items"] = items;
                root["total"] = totalItems;
                root["page"] = page;
                root["perPage"] = perPage;
                root["totalPages"] = (totalItems + perPage - 1) / perPage;
                
                Json::StreamWriterBuilder builder;
                builder["indentation"] = "";
                std::string output = Json::writeString(builder, root);
                
                res.set_content(output, "application/json");
                db.log("成功返回库存数据: " + std::to_string(items.size()) + " 条记录");
                
            } catch (const sql::SQLException& e) {
                db.log("数据库查询错误: " + std::string(e.what()), true);
                res.status = 500;
                res.set_content(json{{"error", "Database query failed"}, {"code", e.getErrorCode()}}.dump(), "application/json");
            } catch (const std::exception& e) {
                db.log("库存数据获取错误: " + std::string(e.what()), true);
                res.status = 500;
                res.set_content(json{{"error", "Failed to get inventory data"}}.dump(), "application/json");
            }
            
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", "Internal server error"}}.dump(), "application/json");
        }
    });

    // API端点 - 操作日志
    server->Get("/api/operation_logs", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_);
        
        int page = 1;
        int perPage = 10;
        std::string search = "";
        
        // 参数解析 - 添加默认值处理
        if (req.has_param("page")) {
            try {
                page = std::max(1, std::stoi(req.get_param_value("page")));
            } catch (...) {
                // 忽略错误，使用默认值
            }
        }
        
        if (req.has_param("perPage")) {
            try {
                perPage = std::clamp(std::stoi(req.get_param_value("perPage")), 1, 100);
            } catch (...) {
                // 忽略错误，使用默认值
            }
        }
        
        if (req.has_param("search")) {
            search = req.get_param_value("search");
        }
        
        try {
            // 测试连接
            if (!db.testConnection()) {
                res.status = 500;
                res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
                return;
            }
            
            // 获取日志数据
            auto logs = db.getOperationLogs(page, perPage, search);
            
            // 获取总数 - 先于数据处理，避免影响连接
            int totalItems = db.getTotalOperationLogsCount();
            int totalPages = (totalItems + perPage - 1) / perPage;
            if (totalPages == 0) totalPages = 1;
            
            // 使用 nlohmann::json 构建响应（更一致）
            nlohmann::json response;
            response["status"] = "success";
            response["page"] = page;
            response["perPage"] = perPage;
            response["totalItems"] = totalItems;
            response["totalPages"] = totalPages;
            
            nlohmann::json logsArray = nlohmann::json::array();
            for (const auto& logEntry : logs) {
                nlohmann::json logJson;
                logJson["id"] = std::stoi(Database::safeGet(logEntry, "id", "0"));
                logJson["operation_type"] = Database::safeGet(logEntry, "operation_type");
                logJson["item_name"] = Database::safeGet(logEntry, "item_name");
                
                // 关键修改：使用新的列名
                logJson["operation_time"] = Database::safeGet(logEntry, "formatted_time");
                
                logJson["operation_note"] = Database::safeGet(logEntry, "operation_note");
                logsArray.push_back(logJson);
            }
            response["logs"] = logsArray;
            
            res.set_content(response.dump(), "application/json");
            
        } catch (const sql::SQLException& e) {
            nlohmann::json error = {
                {"error", "数据库错误"},
                {"code", e.getErrorCode()},
                {"message", e.what()}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        } catch (const std::exception& e) {
            nlohmann::json error = {
                {"error", "获取操作日志失败"},
                {"message", e.what()}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    
    server->Delete("/api/inventory/:id", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_); // 创建独立数据库实例
        
        try {
            int id = std::stoi(req.path_params.at("id"));
            auto params = json::parse(req.body);
            std::string reason = params["reason"];
            
            if (!db.testConnection()) {
                res.status = 500;
                res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
                return;
            }
            
            if (db.deleteInventoryItem(id, reason)) {
                res.set_content(json{{"success", true}, {"message", "删除成功"}}.dump(), "application/json");
            } else {
                res.set_content(json{{"success", false}, {"message", "删除失败"}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
    });
    
    server->Get("/api/inventory/item/:id", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_); // 创建独立数据库实例
        
        try {
            int id = std::stoi(req.path_params.at("id"));
            
            if (!db.testConnection()) {
                res.status = 500;
                res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
                return;
            }
            
            auto itemData = db.getInventoryItemById(id);
            
            if (!itemData.empty()) {
                // 修复JSON构造问题
                nlohmann::json response = {
                    {"inventory_id", std::stoi(Database::safeGet(itemData[0], "id", "0"))},
                    {"item_id", std::stoi(Database::safeGet(itemData[0], "item_id", "0"))},
                    {"item_name", Database::safeGet(itemData[0], "item_name")},
                    {"quantity", std::stoi(Database::safeGet(itemData[0], "quantity", "0"))},
                    {"location", Database::safeGet(itemData[0], "location")}
                };
                res.set_content(response.dump(), "application/json");
            } else {
                res.set_content(json{{"error", "未找到库存项目"}}.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    server->Put("/api/inventory/:id", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_); // 创建独立数据库实例
        
        try {
            std::string idStr = req.path_params.at("id");
            db.log("收到更新请求，库存ID: " + idStr + ", 数据: " + req.body);
            int id = std::stoi(idStr);
            
            auto params = json::parse(req.body);
            
            int quantity = 0;
            if (params["quantity"].is_number()) {
                quantity = params["quantity"];
            } else if (params["quantity"].is_string()) {
                quantity = std::stoi(params["quantity"].get<std::string>());
            }
            
            std::string location = params["location"];
            std::string reason = params["reason"];
            
            if (!db.testConnection()) {
                res.status = 500;
                res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
                return;
            }
            
            if (db.updateInventoryItem(id, quantity, location, reason)) {
                res.set_content(json{{"success", true}, {"message", "更新成功"}}.dump(), "application/json");
            } else {
                res.set_content(json{{"success", false}, {"message", "更新失败"}}.dump(), "application/json");
            }
        } catch (const std::invalid_argument& e) {
            db.log("无效的库存ID: " + std::string(e.what()), true);
            res.set_content(json{{"success", false}, {"error", "无效的库存ID"}}.dump(), "application/json");
        } catch (const std::exception& e) {
            db.log("Web API 更新错误: " + std::string(e.what()), true);
            res.set_content(json{{"success", false}, {"error", e.what()}}.dump(), "application/json");
        }
    });
    
    server->Get("/api/connection-status", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json response;
        
        try {
            Database db(config_); // 创建独立数据库实例
            
            if (db.testConnection()) {
                response["status"] = "connected";
                response["message"] = "数据库连接正常";
            } else {
                response["status"] = "disconnected";
                response["error"] = "数据库连接失败";
            }
        } catch (const std::exception& e) {
            response["status"] = "disconnected";
            response["error"] = e.what();
        }
        
        res.set_content(response.dump(), "application/json");
    });

    /********************************************************************
    * 添加物品功能相关API
    ********************************************************************/
    
    // ====== 1. 检查物品是否存在 ======
    server->Get("/api/check-item", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_);
        if (!db.testConnection()) {
            res.status = 500;
            res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
            return;
        }
        
        if (!req.has_param("name")) {
            res.status = 400;
            res.set_content(json{{"error", "缺少物品名称参数"}}.dump(), "application/json");
            return;
        }
        
        std::string itemName = req.get_param_value("name");
        bool exists = db.itemExistsInList(itemName);
        int itemId = -1;
        if (exists) {
            itemId = db.getItemIdByName(itemName);
        }
        
        nlohmann::json response = {
            {"exists", exists},
            {"itemId", itemId}
        };
        res.set_content(response.dump(), "application/json");
    });
    
    server->Get("/api/search-items", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_);
        if (!db.testConnection()) {
            res.status = 500;
            res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
            return;
        }
        
        if (!req.has_param("q")) {
            res.status = 400;
            res.set_content(json{{"error", "缺少搜索参数"}}.dump(), "application/json");
            return;
        }
        
        std::string query = req.get_param_value("q");
        
        try {
            // 使用参数化查询防止SQL注入
            sql::Connection* connection = db.getConnection(); // 使用新添加的方法
            std::unique_ptr<sql::PreparedStatement> pstmt(
                connection->prepareStatement(
                    "SELECT id, name, category, grade, effect, description "
                    "FROM item_list WHERE name LIKE ? "
                    "ORDER BY name LIMIT 10"
                )
            );
            std::string searchPattern = "%" + query + "%";
            pstmt->setString(1, searchPattern);
            
            std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
            nlohmann::json items = nlohmann::json::array();
            
            while (result->next()) {
                nlohmann::json item;
                item["id"] = result->getInt("id");
                item["name"] = result->getString("name");
                item["category"] = result->getString("category");
                item["grade"] = result->getString("grade");
                item["effect"] = result->getString("effect");
                
                if (!result->isNull("description")) {
                    item["description"] = result->getString("description");
                } else {
                    item["description"] = "";
                }
                
                items.push_back(item);
            }
            
            res.set_content(items.dump(), "application/json");
            
        } catch (sql::SQLException &e) {
            nlohmann::json error = {
                {"error", "数据库查询错误"},
                {"code", e.getErrorCode()},
                {"message", e.what()}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        } catch (const std::exception& e) {
            nlohmann::json error = {
                {"error", "搜索物品失败"},
                {"message", e.what()}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    
    // ====== 3. 添加物品 ======
    server->Post("/api/add-item", [this](const httplib::Request &req, httplib::Response &res) {
        Database db(config_);
        if (!db.testConnection()) {
            res.status = 500;
            res.set_content(json{{"error", "无法连接数据库"}}.dump(), "application/json");
            return;
        }
        
        try {
            auto jsonBody = nlohmann::json::parse(req.body);
            bool isNewItem = jsonBody["isNewItem"];
            nlohmann::json itemInfo = jsonBody["item"];
            int quantity = jsonBody["quantity"];
            std::string location = jsonBody["location"];
            std::string reason = jsonBody["reason"];
            
            int itemId = -1;
            std::string itemName = itemInfo["name"];
            
            // 如果是新物品，先添加到物品列表
            if (isNewItem) {
                if (!db.addItemToList(
                    itemName,
                    itemInfo["category"].get<std::string>(),
                    itemInfo["grade"].get<std::string>(),
                    itemInfo["effect"].get<std::string>(),
                    itemInfo["description"].get<std::string>(),
                    itemInfo["note"].get<std::string>(),
                    reason
                )) {
                    nlohmann::json response = {
                        {"success", false},
                        {"message", "添加物品到列表失败"}
                    };
                    res.set_content(response.dump(), "application/json");
                    return;
                }
                
                // 获取新添加物品的ID
                itemId = db.getItemIdByName(itemName);
                if (itemId <= 0) {
                    throw std::runtime_error("获取新物品ID失败");
                }
            } else {
                itemId = itemInfo["id"];
            }
            
            // 添加到库存
            if (db.addItemToInventory(itemId, quantity, location, reason)) {
                nlohmann::json response = {
                    {"success", true}
                };
                res.set_content(response.dump(), "application/json");
            } else {
                nlohmann::json response = {
                    {"success", false},
                    {"message", "添加到库存失败"}
                };
                res.set_content(response.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            nlohmann::json response = {
                {"success", false},
                {"message", e.what()}
            };
            res.status = 400;
            res.set_content(response.dump(), "application/json");
        }
    });
}
