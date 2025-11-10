#ifndef ITEM_H
#define ITEM_H

#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <cppconn/driver.h>
#include <cppconn/resultset.h>

// 物品数据结构
struct Item {
    int id;
    std::string name;
    std::string eng_name;
    std::string source;
    int page = 0;
    bool srd = false;
    std::string type;
    std::string rarity;
    int weight = 0;
    int value = 0; // 铜币
    int ac = 0;
    std::string strength;
    bool armor = false;
    bool stealth = false;
    std::vector<std::string> entries;
    
    // 添加默认构造函数
    Item() = default;
    
    // 从数据库结果集构造
    Item(sql::ResultSet* res);
    
    // 转换为JSON
    nlohmann::json toJson() const;
};

// 解析单个物品
Item parseItem(const nlohmann::json& j);

// 插入物品到数据库
void insertItem(sql::Connection* con, const Item& item);

// 检查是否包含中文字符
bool containsChinese(const std::string& str);

// 获取所有物品
std::vector<Item> getAllItems(sql::Connection* con);

// 导出所有物品到JSON文件
void exportItemsToJson(sql::Connection* con, const std::string& filename);

#endif // ITEM_H
