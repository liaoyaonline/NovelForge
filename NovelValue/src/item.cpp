#include "item.h"
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <locale>

using json = nlohmann::json;

// === 保留第一个构造函数实现（包含回车符处理）===
Item::Item(sql::ResultSet* res) {
    id = res->getInt("id");
    name = res->getString("name");
    eng_name = res->getString("eng_name");
    source = res->getString("source");
    page = res->getInt("page");
    srd = res->getBoolean("srd");
    type = res->getString("type");
    rarity = res->getString("rarity");
    weight = res->getInt("weight");
    value = res->getInt("value");
    ac = res->getInt("ac");
    strength = res->getString("strength");
    armor = res->getBoolean("armor");
    stealth = res->getBoolean("stealth");
    
    // 解析条目
    std::string entriesStr = res->getString("entries");
    if (!entriesStr.empty()) {
        std::istringstream iss(entriesStr);
        std::string line;
        while (std::getline(iss, line)) {
            // 移除可能的回车符
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            entries.push_back(line);
        }
    }
}

// === 删除以下重复的构造函数定义 ===
/*
// 删除这个重复的定义（第71行开始的构造函数）
Item::Item(sql::ResultSet* res) {
    id = res->getInt("id");
    name = res->getString("name");
    eng_name = res->getString("eng_name");
    source = res->getString("source");
    page = res->getInt("page");
    srd = res->getBoolean("srd");
    type = res->getString("type");
    rarity = res->getString("rarity");
    weight = res->getInt("weight");
    value = res->getInt("value");
    ac = res->getInt("ac");
    strength = res->getString("strength");
    armor = res->getBoolean("armor");
    stealth = res->getBoolean("stealth");
    
    // 解析条目
    std::string entriesStr = res->getString("entries");
    if (!entriesStr.empty()) {
        std::istringstream iss(entriesStr);
        std::string line;
        while (std::getline(iss, line)) {
            entries.push_back(line);
        }
    }
}
*/

// 判断字符串是否包含中文字符
bool containsChinese(const std::string& str) {
    // UTF-8中文字符范围：E4-BA80 到 E9-BB80
    for (size_t i = 0; i < str.length();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (c >= 0xE4 && c <= 0xE9 && i + 2 < str.length()) {
            // 检查是否在中文范围内
            unsigned char c2 = static_cast<unsigned char>(str[i+1]);
            unsigned char c3 = static_cast<unsigned char>(str[i+2]);
            if (c2 >= 0x80 && c2 <= 0xBF && c3 >= 0x80 && c3 <= 0xBF) {
                return true;
            }
            i += 3;
        } else if (c < 0x80) {
            i++;
        } else {
            i += 2; // 跳过其他多字节字符
        }
    }
    return false;
}

json Item::toJson() const {
    json j;
    j["id"] = id;
    j["name"] = name;
    j["eng_name"] = eng_name;
    j["source"] = source;
    j["page"] = page;
    j["srd"] = srd;
    j["type"] = type;
    j["rarity"] = rarity;
    j["weight"] = weight;
    j["value"] = value;
    j["ac"] = ac;
    j["strength"] = strength;
    j["armor"] = armor;
    j["stealth"] = stealth;
    j["entries"] = entries;
    return j;
}

Item parseItem(const json& j) {
    Item item;
    item.id = 0; // 新物品ID为0
    
    // 确保所有字段都有默认值
    item.name = j.value("name", "");
    item.eng_name = j.value("ENG_name", "");
    item.source = j.value("source", "");
    item.page = j.value("page", 0);
    item.srd = j.value("srd", false);
    item.type = j.value("type", "");
    item.rarity = j.value("rarity", "");
    item.weight = j.value("weight", 0);
    item.value = j.value("value", 0);
    item.ac = j.value("ac", 0);
    
    if (j.contains("strength")) {
        if (j["strength"].is_string()) {
            item.strength = j["strength"].get<std::string>();
        } else if (j["strength"].is_number()) {
            item.strength = std::to_string(j["strength"].get<int>());
        } else {
            item.strength = "";
        }
    } else {
        item.strength = "";
    }
    
    item.armor = j.value("armor", false);
    item.stealth = j.value("stealth", false);
    
    if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto& entry : j["entries"]) {
            if (entry.is_string()) {
                item.entries.push_back(entry.get<std::string>());
            }
        }
    }
    
    return item;
}

void insertItem(sql::Connection* con, const Item& item) {
    try {
        // 跳过英文名称的物品
        if (!containsChinese(item.name)) {
            std::cout << "跳过英文物品: " << item.name << std::endl;
            return;
        }
        
        // =========== 添加存在性检查 ===========
        sql::PreparedStatement* checkStmt = con->prepareStatement(
            "SELECT COUNT(*) AS count FROM items WHERE name = ?"
        );
        checkStmt->setString(1, item.name);
        sql::ResultSet* res = checkStmt->executeQuery();
        
        int count = 0;
        if (res->next()) {
            count = res->getInt("count");
        }
        
        delete res;
        delete checkStmt;
        
        if (count > 0) {
            std::cout << "物品已存在，跳过: " << item.name << std::endl;
            return;
        }
        // =========== 结束存在性检查 ===========
        
        // 将条目数组连接为一个字符串
        std::stringstream entries_ss;
        for (const auto& e : item.entries) {
            entries_ss << e << "\n";
        }
        std::string entries_str = entries_ss.str();

        // 使用预处理语句
        sql::PreparedStatement* pstmt = con->prepareStatement(
            "INSERT INTO items ("
            "  name, eng_name, source, page, srd, type, rarity, weight, value, ac, "
            "  strength, armor, stealth, entries"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );
        
        // 绑定参数
        pstmt->setString(1, item.name);
        pstmt->setString(2, item.eng_name);
        pstmt->setString(3, item.source);
        pstmt->setInt(4, item.page);
        pstmt->setBoolean(5, item.srd);
        pstmt->setString(6, item.type);
        pstmt->setString(7, item.rarity);
        pstmt->setInt(8, item.weight);
        pstmt->setInt(9, item.value);
        pstmt->setInt(10, item.ac);
        pstmt->setString(11, item.strength);
        pstmt->setBoolean(12, item.armor);
        pstmt->setBoolean(13, item.stealth);
        pstmt->setString(14, entries_str);
        
        pstmt->execute();
        delete pstmt;
        
        std::cout << "成功插入: " << item.name << std::endl;
    } catch (sql::SQLException &e) {
        std::cerr << "插入错误: " << item.name << std::endl;
        std::cerr << "SQL错误: " << e.what() << std::endl;
        std::cerr << "错误代码: " << e.getErrorCode() << std::endl;
        std::cerr << "SQL状态: " << e.getSQLState() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "插入物品异常: " << e.what() << std::endl;
    }
}


std::vector<Item> getAllItems(sql::Connection* con) {
    std::vector<Item> items;
    try {
        sql::Statement* stmt = con->createStatement();
        sql::ResultSet* res = stmt->executeQuery("SELECT * FROM items ORDER BY id");
        
        while (res->next()) {
            items.emplace_back(res);
        }
        
        delete res;
        delete stmt;
        std::cout << "获取物品数量: " << items.size() << std::endl;
    } catch (sql::SQLException &e) {
        std::cerr << "获取物品错误: " << e.what() << std::endl;
    }
    return items;
}

void exportItemsToJson(sql::Connection* con, const std::string& filename) {
    try {
        auto items = getAllItems(con);
        
        json j;
        j["item"] = json::array();
        
        for (const auto& item : items) {
            j["item"].push_back(item.toJson());
        }
        
        std::ofstream outFile(filename);
        outFile << std::setw(4) << j << std::endl;
        outFile.close();
        
        std::cout << "成功导出 " << items.size() << " 个物品到 " << filename << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "导出错误: " << e.what() << std::endl;
    }
}
