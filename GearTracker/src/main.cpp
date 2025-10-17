#include "Database.h"
#include <iostream>
#include <iomanip>
#include <limits> // 用于处理输入流

// 打印查询结果
void printResults(const std::vector<std::map<std::string, std::string>>& results) {
    if (results.empty()) {
        std::cout << "没有找到结果" << std::endl;
        return;
    }
    
    // 打印表头
    for (const auto& col : results[0]) {
        std::cout << std::setw(15) << std::left << col.first << " | ";
    }
    std::cout << "\n";
    
    // 打印分隔线
    for (size_t i = 0; i < results[0].size(); i++) {
        std::cout << std::setw(15) << std::setfill('-') << "" << "-+-";
    }
    std::cout << std::setfill(' ') << "\n";
    
    // 打印数据行
    for (const auto& row : results) {
        for (const auto& col : row) {
            std::cout << std::setw(15) << std::left << col.second << " | ";
        }
        std::cout << "\n";
    }
}

// 添加新物品
void addNewItem(Database& db) {
    std::string name, category, grade, effect, description, note;
    
    std::cout << "\n==== 添加新物品 ====\n";
    std::cout << "物品名: ";
    std::getline(std::cin, name);
    
    // 检查物品是否已存在
    if (db.itemExistsInList(name)) {
        std::cout << "物品 '" << name << "' 已存在于物品列表中\n";
        return;
    }
    
    std::cout << "种类: ";
    std::getline(std::cin, category);
    std::cout << "等阶: ";
    std::getline(std::cin, grade);
    std::cout << "效果: ";
    std::getline(std::cin, effect);
    std::cout << "描述 (可选): ";
    std::getline(std::cin, description);
    std::cout << "备注 (可选): ";
    std::getline(std::cin, note);
    
    if (db.addItemToList(name, category, grade, effect, description, note)) {
        std::cout << "物品 '" << name << "' 添加成功!\n";
    } else {
        std::cout << "添加物品失败\n";
    }
}

// 添加物品到库存
void addItemToInventory(Database& db) {
    std::string name, location;
    int quantity;
    
    std::cout << "\n==== 添加物品到库存 ====\n";
    std::cout << "物品名: ";
    std::getline(std::cin, name);
    
    // 检查物品是否存在于物品列表
    if (!db.itemExistsInList(name)) {
        std::cout << "物品 '" << name << "' 不存在于物品列表中。请先添加到物品列表\n";
        return;
    }
    
    int itemId = db.getItemIdByName(name);
    if (itemId == -1) {
        std::cout << "无法获取物品ID\n";
        return;
    }
    
    std::cout << "数量: ";
    while (!(std::cin >> quantity) || quantity <= 0) {
        std::cout << "请输入有效的数量: ";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    std::cin.ignore(); // 清除换行符
    
    std::cout << "所属地: ";
    std::getline(std::cin, location);
    
    if (db.addItemToInventory(itemId, quantity, location)) {
        std::cout << "物品 '" << name << "' 成功添加到库存\n";
    } else {
        std::cout << "添加物品到库存失败\n";
    }
}

// 显示库存物品
void showInventory(Database& db) {
    auto inventory = db.executeQuery(
        "SELECT inventory.id, item_list.name, inventory.quantity, "
        "inventory.location, inventory.stored_time, inventory.last_updated "
        "FROM inventory "
        "JOIN item_list ON inventory.item_id = item_list.id"
    );
    
    std::cout << "\n==== 库存物品 ====\n";
    printResults(inventory);
}

// 显示菜单
void showMenu() {
    std::cout << "\n==== 仓库管理系统 ====\n";
    std::cout << "1. 添加新物品到物品列表\n";
    std::cout << "2. 添加物品到库存\n";
    std::cout << "3. 显示库存物品\n";
    std::cout << "4. 显示操作记录\n";
    std::cout << "5. 退出\n";
    std::cout << "请选择操作: ";
}

int main() {
    // 数据库连接参数
    const std::string host = "192.168.1.7";
    const std::string user = "vm_liaoya";
    const std::string password = "123";
    const std::string database = "geartracker";

    // 创建数据库对象
    Database db(host, user, password, database);
    
    // 测试连接
    if (db.connect()) {
        std::cout << "成功连接到MySQL数据库!\n";
        
        // 测试查询
        if (db.testConnection()) {
            std::cout << "数据库测试查询成功!\n";
        } else {
            std::cerr << "数据库测试查询失败!\n";
        }
    } else {
        std::cerr << "数据库连接失败!\n";
        return 1;
    }

    int choice = 0;
    do {
        showMenu();
        std::cin >> choice;
        std::cin.ignore(); // 清除换行符
        
        switch(choice) {
            case 1:
                addNewItem(db);
                break;
            case 2:
                addItemToInventory(db);
                break;
            case 3:
                showInventory(db);
                break;
            case 4:
                // 显示操作记录（后续实现）
                std::cout << "显示操作记录功能将在后续版本添加\n";
                break;
            case 5:
                std::cout << "退出程序...\n";
                break;
            default:
                std::cout << "无效选择，请重新输入\n";
        }
    } while (choice != 5);

    return 0;
}
