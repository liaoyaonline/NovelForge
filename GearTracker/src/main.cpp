#include "Database.h"
#include <iostream>
#include <limits>
#include <cctype>
#include <algorithm>
#include <iomanip>


// 清除输入缓冲区
void clearInputBuffer() {
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// 函数声明
void displayInventory(Database& db);
void displayOperationLogs(Database& db);
void addItemToInventoryMenu(Database& db);
void addItemToListMenu(Database& db);
void modifyInventoryMenu(Database& db);
void deleteInventoryMenu(Database& db);
void displayInventoryItem(const std::map<std::string, std::string>& item);

int main() {
    // 数据库配置
    const std::string host = "192.168.1.7";
    const std::string user = "vm_liaoya";
    const std::string password = "123";
    const std::string database = "geartracker";
    
    // 创建数据库连接
    Database db(host, user, password, database);
    
    if (!db.connect()) {
        std::cerr << "无法连接到数据库！" << std::endl;
        return 1;
    }
    
    if (!db.testConnection()) {
        std::cerr << "数据库连接测试失败！" << std::endl;
        return 1;
    }
    
    std::cout << "成功连接到MySQL数据库!\n";
    std::cout << "数据库测试查询成功!\n\n";
    
    int choice;
    bool running = true;
    
    while (running) {
        std::cout << "\n==== 仓库管理系统 ====\n";
        std::cout << "1. 添加新物品到物品列表\n";
        std::cout << "2. 添加物品到库存\n";
        std::cout << "3. 显示库存物品\n";
        std::cout << "4. 修改库存项目\n"; // 新增
        std::cout << "5. 删除库存项目\n"; // 新增
        std::cout << "6. 显示操作记录\n";
        std::cout << "7. 退出\n"; // 序号调整
        std::cout << "请选择操作: ";
        
        if (std::cin >> choice) {
            clearInputBuffer();
            
            switch (choice) {
                case 1: addItemToListMenu(db); break;
                case 2: addItemToInventoryMenu(db); break;
                case 3: displayInventory(db); break;
                case 4: modifyInventoryMenu(db); break; // 新增
                case 5: deleteInventoryMenu(db); break; // 新增
                case 6: displayOperationLogs(db); break;
                case 7: running = false; break;
                default: std::cout << "无效的选择，请重新输入！\n";
            }
        } else {
            std::cout << "请输入有效的数字！" << std::endl;
            clearInputBuffer();
        }
    }
    
    return 0;
}

// 显示库存
void displayInventory(Database& db) {
    auto inventory = db.getInventory();
    
    if (inventory.empty()) {
        std::cout << "库存为空！" << std::endl;
        return;
    }
    
    std::cout << "\n==== 库存物品列表 ====\n";
    // 设置列宽格式
    std::cout << std::left
              << std::setw(6) << "ID" 
              << std::setw(8) << "物品ID"
              << std::setw(20) << "物品名称"
              << std::setw(6) << "数量"
              << std::setw(15) << "位置"
              << std::setw(20) << "存储时间"
              << "最后更新时间\n";
    std::cout << "---------------------------------------------------"
              << "-----------------------------------\n";
    
    for (const auto& item : inventory) {
        std::cout << std::setw(6) << Database::safeGet(item, "inventory_id") 
                  << std::setw(8) << Database::safeGet(item, "item_id")
                  << std::setw(20) << Database::safeGet(item, "item_name")
                  << std::setw(6) << Database::safeGet(item, "quantity")
                  << std::setw(15) << Database::safeGet(item, "location")
                  << std::setw(20) << Database::safeGet(item, "stored_time")
                  << Database::safeGet(item, "last_updated") << "\n";
    }
}

// 显示操作记录
void displayOperationLogs(Database& db) {
    auto logs = db.getOperationLogs();
    
    if (logs.empty()) {
        std::cout << "暂无操作记录！" << std::endl;
        return;
    }
    
    // 优化显示格式
    std::cout << "\n==== 操作记录 ====\n";
    std::cout << std::left
              << std::setw(5) << "ID" 
              << std::setw(8) << "操作类型"
              << std::setw(20) << "物品名称"
              << std::setw(25) << "操作时间"
              << "备注\n";
    std::cout << "---------------------------------------------------"
              << "-----------------------------------\n";
    
    for (const auto& log : logs) {
        std::cout << std::setw(5) << Database::safeGet(log, "id")
                  << std::setw(8) << Database::safeGet(log, "operation_type")
                  << std::setw(20) << Database::safeGet(log, "item_name")
                  << std::setw(25) << Database::safeGet(log, "operation_time")
                  << Database::safeGet(log, "operation_note") << "\n";
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