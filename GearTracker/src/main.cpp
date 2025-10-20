#include "Database.h"
#include "WebServer.h"
#include "Config.h"
#include <iostream>
#include <limits>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>


// 清除输入缓冲区
void clearInputBuffer() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// ====== 新增分页控制类 ======
class PaginationController {
private:
    int currentPage;
    int totalPages;
    int pageSize;
    int totalItems;
    
public:
    PaginationController(int total, int pageSize = 10) 
        : totalItems(total), pageSize(pageSize), currentPage(1) {
        // 防止除以零
        totalPages = (pageSize > 0) ? ((totalItems + pageSize - 1) / pageSize) : 1;
        if (totalPages == 0) totalPages = 1;
    }
    void nextPage() {
        if (currentPage < totalPages) {
            currentPage++;
        } else {
            std::cout << "已经是最后一页！" << std::endl;
        }
    }
    
    void prevPage() {
        if (currentPage > 1) {
            currentPage--;
        } else {
            std::cout << "已经是第一页！" << std::endl;
        }
    }
    
    void goToPage(int page) {
        if (page >= 1 && page <= totalPages) {
            currentPage = page;
        } else {
            std::cout << "页码超出范围！有效范围: 1-" << totalPages << std::endl;
        }
    }
    
    int getCurrentPage() const { return currentPage; }
    int getTotalPages() const { return totalPages; }
    int getPageSize() const { return pageSize; }
    int getTotalItems() const { return totalItems; }
    
    void displayNavigation() const {
    std::cout << "\n===== 分页导航 =====";
    std::cout << "\n当前页: " << currentPage << "/" << totalPages;
    std::cout << "  每页: " << pageSize << "条记录";
    
    // 处理总记录数为0的情况
    if (totalItems <= 0) {
        std::cout << "  总记录数: 未知";
        std::cout << "  显示记录: 0-0";
    } else {
        std::cout << "  总记录数: " << totalItems;
        int start = (currentPage - 1) * pageSize + 1;
        int end = std::min(currentPage * pageSize, totalItems);
        std::cout << "  显示记录: " << start << "-" << end;
    }
    
    std::cout << "\n操作: (n)下一页, (p)上一页, (g)跳转到页, (s)设置每页数量, (q)返回主菜单";
    std::cout << "\n请选择: ";
}

};

// 函数声明
void displayInventory(Database& db);
void displayOperationLogs(Database& db);
void addItemToInventoryMenu(Database& db);
void addItemToListMenu(Database& db);
void modifyInventoryMenu(Database& db);
void deleteInventoryMenu(Database& db);
void displayInventoryItem(const std::map<std::string, std::string>& item);
void configManagementMenu(Database& db);
void showCurrentConfig(Database& db);
void modifyDatabaseConfig(Database& db);
void modifyAppConfig(Database& db);
void reloadConfig(Database& db);
void createDefaultConfigIfMissing();

int main() {
    // 创建默认配置（如果需要）
    createDefaultConfigIfMissing();
    
    try {
        // 创建配置实例
        Config config;
        
        // 创建数据库实例（堆分配）
         Database db(config);
        // 尝试连接数据库
        if (!db.connect()) {
            throw std::runtime_error("无法连接到数据库");
        }
        
        std::cout << "成功连接到MySQL数据库!\n";
        
        // 启动Web服务器
        int webPort = 8080; // 默认端口
        WebServer server(webPort, db);
        server.start();
        
        std::cout << "Web管理界面已启动: http://localhost:" << webPort << std::endl;
        int choice;
        bool running = true;
        
        while (running) {
            std::cout << "\n==== 仓库管理系统 ====\n";
            std::cout << "1. 添加新物品到物品列表\n";
            std::cout << "2. 添加物品到库存\n";
            std::cout << "3. 显示库存物品\n";
            std::cout << "4. 修改库存项目\n";
            std::cout << "5. 删除库存项目\n";
            std::cout << "6. 显示操作记录\n";
            std::cout << "7. 配置管理\n"; // 新增配置管理
            std::cout << "8. 退出\n";
            std::cout << "请选择操作: ";
            
            if (std::cin >> choice) {
                clearInputBuffer();
                
                switch (choice) {
                    case 1: addItemToListMenu(db); break;
                    case 2: addItemToInventoryMenu(db); break;
                    case 3: displayInventory(db); break;
                    case 4: modifyInventoryMenu(db); break;
                    case 5: deleteInventoryMenu(db); break;
                    case 6: displayOperationLogs(db); break;
                    case 7: configManagementMenu(db); break; // 新增
                    case 8: running = false; break;
                    default: std::cout << "无效的选择，请重新输入！\n";
                }
            } else {
                std::cout << "请输入有效的数字！" << std::endl;
                clearInputBuffer();
            }
        }
        
        // 停止Web服务器（如果需要显式停止）
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "初始化失败: " << e.what() << "\n";
        std::cerr << "请检查配置文件 config.ini\n";
        return 1;
    }
    return 0;
}

// ====== 修改显示函数 ======
void displayInventory(Database& db) {
    int pageSize = db.getConfig().getInt("application", "page_size", 10);
    int totalItems = db.getTotalInventoryCount();
    std::cout << "daiqiongfengtest111:" << totalItems << std::endl;
    PaginationController pagination(totalItems, pageSize);
    
    bool inPagination = true;
    while (inPagination) {
        auto inventory = db.getInventory(pagination.getCurrentPage(), pagination.getPageSize());
        
        if (inventory.empty()) {
            std::cout << "库存为空！" << std::endl;
            return;
        }
        
        std::cout << "\n==== 库存物品列表 (第" << pagination.getCurrentPage() 
                  << "页/共" << pagination.getTotalPages() << "页) ====\n";
        // ... 格式代码保持不变 ...
        
        for (const auto& item : inventory) {
            std::cout << std::setw(6) << Database::safeGet(item, "inventory_id") 
                      << std::setw(8) << Database::safeGet(item, "item_id")
                      << std::setw(20) << Database::safeGet(item, "item_name")
                      << std::setw(6) << Database::safeGet(item, "quantity")
                      << std::setw(15) << Database::safeGet(item, "location")
                      << std::setw(20) << Database::safeGet(item, "stored_time")
                      << Database::safeGet(item, "last_updated") << "\n";
        }
        
        // 显示分页导航
        pagination.displayNavigation();
        
        // 处理用户输入
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "n") {
            pagination.nextPage();
        } else if (input == "p") {
            pagination.prevPage();
        } else if (input == "q") {
            inPagination = false;
        } else if (input.rfind("g ", 0) == 0) {
            try {
                int page = std::stoi(input.substr(2));
                pagination.goToPage(page);
            } catch (...) {
                std::cout << "无效的页码！" << std::endl;
            }
        } else {
            std::cout << "无效的操作！" << std::endl;
        }
    }
}

void displayOperationLogs(Database& db) {
    int pageSize = db.getConfig().getInt("application", "page_size", 10);
    int totalItems = db.getTotalOperationLogsCount();
    PaginationController pagination(totalItems, pageSize);
    
    bool inPagination = true;
    while (inPagination) {
        auto logs = db.getOperationLogs(pagination.getCurrentPage(), pagination.getPageSize());
        
        if (logs.empty()) {
            std::cout << "暂无操作记录！" << std::endl;
            return;
        }
        
        std::cout << "\n==== 操作记录 (第" << pagination.getCurrentPage() 
                  << "页/共" << pagination.getTotalPages() << "页) ====\n";
        // ... 格式代码保持不变 ...
        
        for (const auto& log : logs) {
            std::cout << std::setw(5) << Database::safeGet(log, "id")
                      << std::setw(8) << Database::safeGet(log, "operation_type")
                      << std::setw(20) << Database::safeGet(log, "item_name")
                      << std::setw(25) << Database::safeGet(log, "operation_time")
                      << Database::safeGet(log, "operation_note") << "\n";
        }
        
        // 显示分页导航
        pagination.displayNavigation();
        
        // 处理用户输入
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "n") {
            pagination.nextPage();
        } else if (input == "p") {
            pagination.prevPage();
        } else if (input == "g") {
            std::cout << "输入要跳转的页码: ";
            int page;
            std::cin >> page;
            clearInputBuffer();
            pagination.goToPage(page);
        } else if (input == "q") {
            inPagination = false;
        } else {
            std::cout << "无效的操作！" << std::endl;
        }
    }
}

// 添加物品到库存
void addItemToInventoryMenu(Database& db) {
    std::string itemName, location;
    int quantity;
    
    std::cout << "\n==== 添加物品到库存 ====\n";
    
    // 输入物品名称
    while (true) {
        std::cout << "物品名称: ";
        std::getline(std::cin, itemName);
        
        if (itemName.empty()) {
            std::cout << "物品名称不能为空！" << std::endl;
            continue;
        }
        
        if (!db.itemExistsInList(itemName)) {
            std::cout << "物品不存在于物品列表！请先添加到物品列表。" << std::endl;
            return;
        }
        
        break;
    }
    
    // 获取物品ID
    int itemId = db.getItemIdByName(itemName);
    if (itemId == -1) {
        std::cout << "获取物品ID失败！" << std::endl;
        return;
    }
    
    // 输入数量
    while (true) {
        std::cout << "数量: ";
        if (std::cin >> quantity) {
            if (quantity <= 0) {
                std::cout << "数量必须大于0！" << std::endl;
            } else {
                clearInputBuffer();
                break;
            }
        } else {
            std::cout << "请输入有效的数字！" << std::endl;
            clearInputBuffer();
        }
    }
    
    // 输入位置
    while (true) {
        std::cout << "位置: ";
        std::getline(std::cin, location);
        
        if (location.empty()) {
            std::cout << "位置不能为空！" << std::endl;
        } else {
            break;
        }
    }

    // 新增：输入操作原因
    std::string operationReason;
    std::cout << "操作原因: ";
    std::getline(std::cin, operationReason);
    while (operationReason.empty()) {
        std::cout << "操作原因不能为空，请重新输入: ";
        std::getline(std::cin, operationReason);
    }
    // 修改调用，添加操作原因参数
    if (db.addItemToInventory(itemId, quantity, location, operationReason)) {
        std::cout << "物品成功添加到库存！" << std::endl;
    } else {
        std::cout << "添加物品到库存失败！" << std::endl;
    }
}

// 添加新物品到物品列表
void addItemToListMenu(Database& db) {
    std::string name, category, grade, effect, description, note;
    
    std::cout << "\n==== 添加新物品 ====\n";
    
    // 输入名称
    while (true) {
        std::cout << "物品名: ";
        std::getline(std::cin, name);
        
        if (name.empty()) {
            std::cout << "物品名不能为空！" << std::endl;
            continue;
        }
        
        if (db.itemExistsInList(name)) {
            std::cout << "物品已存在于物品列表！" << std::endl;
            return;
        }
        
        break;
    }
    
    // 输入类别
    while (true) {
        std::cout << "类别: ";
        std::getline(std::cin, category);
        
        if (category.empty()) {
            std::cout << "类别不能为空！" << std::endl;
        } else {
            break;
        }
    }
    
    // 输入品质
    while (true) {
        std::cout << "品质: ";
        std::getline(std::cin, grade);
        
        if (grade.empty()) {
            std::cout << "品质不能为空！" << std::endl;
        } else {
            break;
        }
    }
    
    // 输入效果
    while (true) {
        std::cout << "效果: ";
        std::getline(std::cin, effect);
        
        if (effect.empty()) {
            std::cout << "效果不能为空！" << std::endl;
        } else {
            break;
        }
    }
    
    // 输入描述
    std::cout << "描述 (可选): ";
    std::getline(std::cin, description);
    
    // 输入备注
    std::cout << "备注 (可选): ";
    std::getline(std::cin, note);

    // 新增：输入操作原因
    std::string operationReason;
    std::cout << "操作原因: ";
    std::getline(std::cin, operationReason);
    while (operationReason.empty()) {
        std::cout << "操作原因不能为空，请重新输入: ";
        std::getline(std::cin, operationReason);
    }
    // 修改调用，添加操作原因参数
    if (db.addItemToList(name, category, grade, effect, description, note, operationReason)) {
        std::cout << "物品成功添加到物品列表！" << std::endl;
    } else {
        std::cout << "添加物品到物品列表失败！" << std::endl;
    }
}


// ====== 新增库存修改功能 ======
void modifyInventoryMenu(Database& db) {
    int inventoryId;
    std::cout << "\n==== 修改库存项目 ====\n";
    
    // 显示当前库存
    displayInventory(db);
    
    // 获取库存ID
    while (true) {
        std::cout << "输入要修改的库存项目ID (0返回): ";
        if (std::cin >> inventoryId) {
            clearInputBuffer();
            
            if (inventoryId == 0) return;
            
            auto item = db.getInventoryItemById(inventoryId);
            if (!item.empty()) {
                displayInventoryItem(item[0]);
                break;
            } else {
                std::cout << "未找到ID为 " << inventoryId << " 的库存项目！\n";
            }
        } else {
            std::cout << "请输入有效的数字！\n";
            clearInputBuffer();
        }
    }
    
    // 获取新数量
    int newQuantity;
    while (true) {
        std::cout << "新数量: ";
        if (std::cin >> newQuantity) {
            if (newQuantity <= 0) {
                std::cout << "数量必须大于0！\n";
            } else {
                clearInputBuffer();
                break;
            }
        } else {
            std::cout << "请输入有效的数字！\n";
            clearInputBuffer();
        }
    }
    
    // 获取新位置
    std::string newLocation;
    while (true) {
        std::cout << "新位置: ";
        std::getline(std::cin, newLocation);
        
        if (newLocation.empty()) {
            std::cout << "位置不能为空！\n";
        } else {
            break;
        }
    }
    
    // 新增：输入操作原因
    std::string operationReason;
    std::cout << "操作原因: ";
    std::getline(std::cin, operationReason);
    while (operationReason.empty()) {
        std::cout << "操作原因不能为空，请重新输入: ";
        std::getline(std::cin, operationReason);
    }
    // 确认操作
    std::string confirm;
    std::cout << "确认修改库存项目? (y/n): ";
    std::getline(std::cin, confirm);
    
    if (confirm == "y" || confirm == "Y") {
        // 修改调用，添加操作原因参数
        if (db.updateInventoryItem(inventoryId, newQuantity, newLocation, operationReason)) {
            std::cout << "库存项目修改成功！\n";
        }else {
            std::cout << "库存项目修改失败！\n";
        }
    } else {
        std::cout << "已取消修改操作。\n";
    }
}

// ====== 新增库存删除功能 ======
void deleteInventoryMenu(Database& db) {
    int inventoryId;
    std::cout << "\n==== 删除库存项目 ====\n";
    
    // 显示当前库存
    displayInventory(db);
    
    // 获取库存ID
    while (true) {
        std::cout << "输入要删除的库存项目ID (0返回): ";
        if (std::cin >> inventoryId) {
            clearInputBuffer();
            
            if (inventoryId == 0) return;
            
            auto item = db.getInventoryItemById(inventoryId);
            if (!item.empty()) {
                displayInventoryItem(item[0]);
                break;
            } else {
                std::cout << "未找到ID为 " << inventoryId << " 的库存项目！\n";
            }
        } else {
            std::cout << "请输入有效的数字！\n";
            clearInputBuffer();
        }
    }

    std::string operationReason;
    std::cout << "操作原因: ";
    std::getline(std::cin, operationReason);
    while (operationReason.empty()) {
        std::cout << "操作原因不能为空，请重新输入: ";
        std::getline(std::cin, operationReason);
    }
    // 确认操作
    std::string confirm;
    std::cout << "确认删除库存项目? 此操作不可撤销! (y/n): ";
    std::getline(std::cin, confirm);
    
    if (confirm == "y" || confirm == "Y") {
        // 修改调用，添加操作原因参数
        if (db.deleteInventoryItem(inventoryId, operationReason)) {
            std::cout << "库存项目删除成功！\n";
        } else {
            std::cout << "库存项目删除失败！\n";
        }
    } else {
        std::cout << "已取消删除操作。\n";
    }
}

// ====== 新增辅助函数：显示单个库存项目 ======
void displayInventoryItem(const std::map<std::string, std::string>& item) {
    std::cout << "\n==== 库存项目详情 ====\n";
    std::cout << "库存ID: " << Database::safeGet(item, "inventory_id") << "\n";
    std::cout << "物品ID: " << Database::safeGet(item, "item_id") << "\n";
    std::cout << "物品名称: " << Database::safeGet(item, "item_name") << "\n";
    std::cout << "数量: " << Database::safeGet(item, "quantity") << "\n";
    std::cout << "位置: " << Database::safeGet(item, "location") << "\n";
    std::cout << "存储时间: " << Database::safeGet(item, "stored_time") << "\n";
    std::cout << "最后更新: " << Database::safeGet(item, "last_updated") << "\n";
}

// ====== 新增配置文件管理菜单 ======
void configManagementMenu(Database& db) {
    bool inMenu = true;
    while (inMenu) {
        std::cout << "\n==== 配置管理 ====\n";
        std::cout << "1. 查看当前配置\n";
        std::cout << "2. 修改数据库连接\n";
        std::cout << "3. 修改应用设置\n";
        std::cout << "4. 重新加载配置\n";
        std::cout << "5. 返回主菜单\n";
        std::cout << "请选择操作: ";
        
        int choice;
        if (std::cin >> choice) {
            clearInputBuffer();
            
            switch (choice) {
                case 1: showCurrentConfig(db); break;
                case 2: modifyDatabaseConfig(db); break;
                case 3: modifyAppConfig(db); break;
                case 4: reloadConfig(db); break;
                case 5: inMenu = false; break;
                default: std::cout << "无效的选择，请重新输入！\n";
            }
        } else {
            std::cout << "请输入有效的数字！\n";
            clearInputBuffer();
        }
    }
}

void showCurrentConfig(Database& db) {
    Config& config = db.getConfig();
    
    std::cout << "\n==== 当前配置 ====\n";
    std::cout << "[数据库配置]\n";
    std::cout << "主机: " << config.getString("database", "host") << "\n";
    std::cout << "端口: " << config.getInt("database", "port") << "\n";
    std::cout << "用户名: " << config.getString("database", "username") << "\n";
    std::cout << "密码: " << std::string(config.getString("database", "password").size(), '*') << "\n";
    std::cout << "数据库名: " << config.getString("database", "database") << "\n\n";
    
    std::cout << "[应用配置]\n";
    std::cout << "日志级别: " << config.getString("application", "log_level") << "\n";
    std::cout << "每页数量: " << config.getInt("application", "page_size") << "\n";
    std::cout << "日志文件: " << config.getString("application", "log_file") << "\n";
}

void modifyDatabaseConfig(Database& db) {
    Config& config = db.getConfig();
    
    std::cout << "\n==== 修改数据库配置 ====\n";
    
    std::string host, username, password, dbName;
    int port;
    
    // 获取输入
    std::cout << "主机地址 (当前: " << config.getString("database", "host") << "): ";
    std::getline(std::cin, host);
    if (host.empty()) host = config.getString("database", "host");
    
    std::cout << "端口 (当前: " << config.getInt("database", "port") << "): ";
    std::string portStr;
    std::getline(std::cin, portStr);
    port = portStr.empty() ? config.getInt("database", "port") : std::stoi(portStr);
    
    std::cout << "用户名 (当前: " << config.getString("database", "username") << "): ";
    std::getline(std::cin, username);
    if (username.empty()) username = config.getString("database", "username");
    
    // 密码可以留空保持原样
    std::cout << "密码 (留空保持原样): ";
    std::getline(std::cin, password);
    if (password.empty()) {
        password = config.getString("database", "password");
    }
    
    std::cout << "数据库名 (当前: " << config.getString("database", "database") << "): ";
    std::getline(std::cin, dbName);
    if (dbName.empty()) dbName = config.getString("database", "database");
    
    // 确认修改
    std::cout << "\n确认更新数据库配置? (y/n): ";
    std::string confirm;
    std::getline(std::cin, confirm);
    
    if (confirm == "y" || confirm == "Y") {
        try {
            db.updateDatabaseCredentials(host, port, username, password, dbName);
            std::cout << "数据库配置更新成功！\n";
        } catch (const std::exception& e) {
            std::cerr << "配置更新失败: " << e.what() << "\n";
        }
    } else {
        std::cout << "已取消配置更新。\n";
    }
}

void modifyAppConfig(Database& db) {
    Config& config = db.getConfig();
    
    std::cout << "\n==== 修改应用配置 ====\n";
    
    std::string logLevel, logFile;
    int pageSize;
    
    // 获取输入
    std::cout << "日志级别 (debug/info/warning/error) (当前: " 
              << config.getString("application", "log_level") << "): ";
    std::getline(std::cin, logLevel);
    if (logLevel.empty()) logLevel = config.getString("application", "log_level");
    
    std::cout << "每页显示数量 (当前: " 
              << config.getInt("application", "page_size") << "): ";
    std::string sizeStr;
    std::getline(std::cin, sizeStr);
    pageSize = sizeStr.empty() ? config.getInt("application", "page_size") : std::stoi(sizeStr);
    
    std::cout << "日志文件路径 (当前: " 
              << config.getString("application", "log_file") << "): ";
    std::getline(std::cin, logFile);
    if (logFile.empty()) logFile = config.getString("application", "log_file");
    
    // 更新配置
    config.setString("application", "log_level", logLevel);
    config.setInt("application", "page_size", pageSize);
    config.setString("application", "log_file", logFile);
    
    // 保存到文件
    config.save();
    
    // 重新加载配置
    db.reloadConfig();
    
    std::cout << "应用配置更新成功！\n";
}

void reloadConfig(Database& db) {
    db.reloadConfig();
    std::cout << "配置已重新加载！\n";
}

// 在首次运行时创建默认配置文件
void createDefaultConfigIfMissing() {
    std::ifstream testFile("config.ini");
    if (!testFile.good()) {
        std::ofstream configFile("config.ini");
        
        if (configFile.is_open()) {
            configFile << "[database]\n";
            configFile << "host = 192.168.1.7\n";
            configFile << "port = 3306\n";
            configFile << "username = vm_liaoya\n";
            configFile << "password = 123\n";
            configFile << "database = geartracker\n\n";
            
            configFile << "[application]\n";
            configFile << "log_level = info\n";
            configFile << "page_size = 10\n";
            configFile << "log_file = geartracker.log\n";
            
            configFile.close();
            std::cout << "已创建默认配置文件 config.ini，请修改其中的数据库凭证后再运行程序。\n";
        }
    }
}