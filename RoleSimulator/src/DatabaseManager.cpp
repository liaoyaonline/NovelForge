#include "DatabaseManager.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;

DatabaseManager::DatabaseManager() : conn(nullptr) {}

DatabaseManager::~DatabaseManager() {
    if (conn) delete conn;
}

bool DatabaseManager::connect(const std::string& configPath) {
    try {
        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            std::cerr << "错误: 无法打开数据库配置文件\n";
            return false;
        }
        
        json config;
        configFile >> config;
        
        std::string host = config["host"];
        int port = config["port"];
        std::string user = config["user"];
        std::string password = config["password"];
        std::string database = config["database"];
        
        sql::Driver* driver = get_driver_instance();
        std::string connectionStr = "tcp://" + host + ":" + std::to_string(port);
        conn = driver->connect(connectionStr, user, password);
        conn->setSchema(database);
        return true;
    } catch (const sql::SQLException &e) {
        std::cerr << "SQL错误: " << e.what() << '\n';
        return false;
    } catch (const std::exception &e) {
        std::cerr << "错误: " << e.what() << '\n';
        return false;
    }
}


std::vector<Character> DatabaseManager::loadCharacters() {
    std::vector<Character> characters;
    if (!conn) return characters;
    
    try {
        sql::Statement* stmt = conn->createStatement();
        // 添加talent和comment字段查询
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT id, name, race, age, power_level, "
            "cultivation_level, cultivation_progress, "
            "cultivation_skill, talent, comment "  // 新增字段
            "FROM characters"
        );
        
        while (res->next()) {
            Character ch;
            ch.id = res->getInt("id");
            ch.name = res->getString("name");
            ch.race = res->getString("race");
            ch.age = res->getInt("age");
            ch.power_level = res->getString("power_level");
            ch.cultivation_level = res->getString("cultivation_level");
            ch.cultivation_progress = res->getString("cultivation_progress");
            ch.cultivation_skill = res->getString("cultivation_skill");
            ch.talent = res->getString("talent");          // 新增
            ch.comment = res->getString("comment");        // 新增
            
            sql::PreparedStatement* pstmt = conn->prepareStatement(
                "SELECT name, stage, current_exp, max_stage_exp "
                "FROM skills WHERE character_id = ?"
            );
            pstmt->setInt(1, ch.id);
            sql::ResultSet* skillsRes = pstmt->executeQuery();
            
            while (skillsRes->next()) {
                Skill skill;
                skill.name = skillsRes->getString("name");
                skill.stage = skillsRes->getString("stage");
                skill.current_exp = skillsRes->getInt("current_exp");
                skill.max_stage_exp = skillsRes->getInt("max_stage_exp");
                ch.skills.push_back(skill);
            }
            delete skillsRes;
            delete pstmt;
            
            characters.push_back(ch);
        }
        delete res;
        delete stmt;
    } catch (const sql::SQLException &e) {
        std::cerr << "加载角色时SQL错误: " << e.what() << '\n';
    }
    return characters;
}


std::map<std::string, SkillStage> DatabaseManager::loadSkillStages() {
    std::map<std::string, SkillStage> stages;
    if (!conn) return stages;
    
    try {
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT stage_name, stage_max_exp, avg_rate FROM skill_stages"
        );
        
        while (res->next()) {
            SkillStage stage;
            stage.stage_name = res->getString("stage_name");
            stage.stage_max_exp = res->getInt("stage_max_exp");
            stage.avg_rate = res->getDouble("avg_rate");
            stages[stage.stage_name] = stage;
        }
        delete res;
        delete stmt;
    } catch (const sql::SQLException &e) {
        std::cerr << "加载技能阶段时SQL错误: " << e.what() << '\n';
    }
    return stages;
}

std::map<std::string, CultivationStage> DatabaseManager::loadCultivationStages() {
    std::map<std::string, CultivationStage> stages;
    if (!conn) return stages;
    
    try {
        sql::Statement* stmt = conn->createStatement();
        // 添加stage_order和previous_stage字段
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT level, exp_required, base_rate, time_required, "
            "stage_order, previous_stage "
            "FROM cultivation_stages ORDER BY stage_order ASC"  // 按顺序排序
        );
        
        while (res->next()) {
            CultivationStage stage;
            stage.level = res->getString("level");
            stage.exp_required = res->getInt("exp_required");
            stage.base_rate = res->getDouble("base_rate");
            stage.time_required = res->getInt("time_required");
            stage.stage_order = res->getInt("stage_order");
            stage.previous = res->getString("previous_stage");
            stages[stage.level] = stage;
        }
        delete res;
        delete stmt;
        calculateStageMinExp(stages);
        
        // 验证阶段连续性
        validateStageContinuity(stages);
    } catch (const sql::SQLException &e) {
        std::cerr << "加载修为阶段时SQL错误: " << e.what() << '\n';
    }
    return stages;
}

void DatabaseManager::calculateStageMinExp(std::map<std::string, CultivationStage>& stages) {
    // 按阶段顺序排序
    std::vector<CultivationStage> orderedStages;
    for (auto& pair : stages) {
        orderedStages.push_back(pair.second);
    }
    
    // 按stage_order排序
    std::sort(orderedStages.begin(), orderedStages.end(), 
        [](const CultivationStage& a, const CultivationStage& b) {
            return a.stage_order < b.stage_order;
        });
    
    // 按顺序计算最小经验
    int totalExp = 0;
    for (auto& stage : orderedStages) {
        stages[stage.level].min_exp = totalExp;
        totalExp += stage.exp_required;
    }
}

void DatabaseManager::validateStageContinuity(std::map<std::string, CultivationStage>& stages) {
    for (const auto& pair : stages) {
        const CultivationStage& stage = pair.second;
        if (!stage.previous.empty()) {
            if (stages.find(stage.previous) == stages.end()) {
                std::cerr << "警告: 阶段 '" << stage.level 
                          << "' 的前置阶段 '" << stage.previous 
                          << "' 不存在!\n";
            } else if (stages[stage.previous].stage_order != stage.stage_order - 1) {
                std::cerr << "警告: 阶段 '" << stage.level 
                          << "' 的前置阶段顺序错误 (应为 " 
                          << (stage.stage_order - 1) << ")\n";
            }
        }
    }
}

std::map<std::string, double> DatabaseManager::loadSkillMultipliers() {
    std::map<std::string, double> multipliers;
    if (!conn) return multipliers;
    
    try {
        sql::Statement* stmt = conn->createStatement();
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT stage_name, multiplier FROM skill_stage_multipliers"
        );
        
        while (res->next()) {
            multipliers[res->getString("stage_name")] = res->getDouble("multiplier");
        }
        delete res;
        delete stmt;
    } catch (const sql::SQLException &e) {
        std::cerr << "加载技能加成时SQL错误: " << e.what() << '\n';
    }
    return multipliers;
}

bool DatabaseManager::saveCharacter(const Character& character) {
    return true;
}

// 添加更新角色方法 - 修改: 添加talent和comment字段
bool DatabaseManager::updateCharacter(const Character& character) {
    if (!conn) {
        std::cerr << "数据库未连接" << std::endl;
        return false;
    }
    
    try {
        // 开始事务
        conn->setAutoCommit(false);
        
        // 1. 更新角色基本信息 - 添加talent和comment字段
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "UPDATE characters SET "
            "name = ?, "
            "race = ?, "
            "age = ?, "
            "power_level = ?, "
            "cultivation_level = ?, "
            "cultivation_progress = ?, "
            "cultivation_skill = ?, "
            "talent = ?, "          // 新增
            "comment = ? "          // 新增
            "WHERE id = ?"
        );
        pstmt->setString(1, character.name);
        pstmt->setString(2, character.race);
        pstmt->setInt(3, character.age);
        pstmt->setString(4, character.power_level);
        pstmt->setString(5, character.cultivation_level);
        pstmt->setString(6, character.cultivation_progress);
        pstmt->setString(7, character.cultivation_skill);
        pstmt->setString(8, character.talent);       // 新增
        pstmt->setString(9, character.comment);      // 新增
        pstmt->setInt(10, character.id);             // 注意参数索引增加
        pstmt->executeUpdate();
        delete pstmt;
        
        // 2. 更新技能信息
        // 先删除旧的技能记录
        pstmt = conn->prepareStatement("DELETE FROM skills WHERE character_id = ?");
        pstmt->setInt(1, character.id);
        pstmt->executeUpdate();
        delete pstmt;
        
        // 插入新的技能记录
        for (const auto& skill : character.skills) {
            pstmt = conn->prepareStatement(
                "INSERT INTO skills (character_id, name, stage, current_exp, max_stage_exp) "
                "VALUES (?, ?, ?, ?, ?)"
            );
            pstmt->setInt(1, character.id);
            pstmt->setString(2, skill.name);
            pstmt->setString(3, skill.stage);
            pstmt->setInt(4, skill.current_exp);
            pstmt->setInt(5, skill.max_stage_exp);
            pstmt->executeUpdate();
            delete pstmt;
        }
        
        // 提交事务
        conn->commit();
        conn->setAutoCommit(true);
        return true;
    } catch (const sql::SQLException &e) {
        std::cerr << "更新角色时SQL错误: " << e.what() << std::endl;
        try {
            conn->rollback();
        } catch (const sql::SQLException &e2) {
            std::cerr << "回滚失败: " << e2.what() << std::endl;
        }
        return false;
    }
}

// 添加保存历史记录方法 - 修改: 添加talent和comment字段
bool DatabaseManager::saveSimulationHistory(int characterId, 
                                           int days, 
                                           const TimeAllocation& alloc, 
                                           const Character& before, 
                                           const Character& after) {
    if (!conn) {
        std::cerr << "数据库未连接" << std::endl;
        return false;
    }
    
    try {
        // 创建JSON对象
        using json = nlohmann::json;
        
        // 时间分配JSON
        json allocJson;
        allocJson["restHours"] = alloc.restHours;
        for (const auto& entry : alloc.skillHours) {
            allocJson["skills"][entry.first] = entry.second;
        }
        
        // 推演前快照 - 添加talent和comment
        json beforeJson;
        beforeJson["name"] = before.name;
        beforeJson["race"] = before.race;
        beforeJson["age"] = before.age;
        beforeJson["power_level"] = before.power_level;
        beforeJson["cultivation_level"] = before.cultivation_level;
        beforeJson["cultivation_progress"] = before.cultivation_progress;
        beforeJson["cultivation_skill"] = before.cultivation_skill;
        beforeJson["talent"] = before.talent;          // 新增
        beforeJson["comment"] = before.comment;        // 新增
        for (const auto& skill : before.skills) {
            json skillJson;
            skillJson["name"] = skill.name;
            skillJson["stage"] = skill.stage;
            skillJson["current_exp"] = skill.current_exp;
            skillJson["max_stage_exp"] = skill.max_stage_exp;
            beforeJson["skills"].push_back(skillJson);
        }
        
        // 推演后快照 - 添加talent和comment
        json afterJson;
        afterJson["name"] = after.name;
        afterJson["race"] = after.race;
        afterJson["age"] = after.age;
        afterJson["power_level"] = after.power_level;
        afterJson["cultivation_level"] = after.cultivation_level;
        afterJson["cultivation_progress"] = after.cultivation_progress;
        afterJson["cultivation_skill"] = after.cultivation_skill;
        afterJson["talent"] = after.talent;            // 新增
        afterJson["comment"] = after.comment;          // 新增
        for (const auto& skill : after.skills) {
            json skillJson;
            skillJson["name"] = skill.name;
            skillJson["stage"] = skill.stage;
            skillJson["current_exp"] = skill.current_exp;
            skillJson["max_stage_exp"] = skill.max_stage_exp;
            afterJson["skills"].push_back(skillJson);
        }
        
        // 插入历史记录
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "INSERT INTO simulation_history "
            "(character_id, simulation_days, time_allocation, before_snapshot, after_snapshot) "
            "VALUES (?, ?, ?, ?, ?)"
        );
        pstmt->setInt(1, characterId);
        pstmt->setInt(2, days);
        pstmt->setString(3, allocJson.dump());
        pstmt->setString(4, beforeJson.dump());
        pstmt->setString(5, afterJson.dump());
        pstmt->executeUpdate();
        delete pstmt;
        
        return true;
    } catch (const sql::SQLException &e) {
        std::cerr << "保存历史记录时SQL错误: " << e.what() << std::endl;
        return false;
    }
}

// 添加角色方法 - 修改: 添加talent和comment字段
bool DatabaseManager::addCharacter(Character& character) {
    if (!conn) return false;
    
    try {
        conn->setAutoCommit(false);
        
        // 插入角色基本信息 - 添加talent和comment字段
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "INSERT INTO characters (name, race, age, power_level, "
            "cultivation_level, cultivation_progress, cultivation_skill, talent, comment) " 
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)" 
        );
        pstmt->setString(1, character.name);
        pstmt->setString(2, character.race);
        pstmt->setInt(3, character.age);
        pstmt->setString(4, character.power_level);
        pstmt->setString(5, character.cultivation_level);
        pstmt->setString(6, character.cultivation_progress);
        pstmt->setString(7, character.cultivation_skill);
        pstmt->setString(8, character.talent);       // 新增
        pstmt->setString(9, character.comment);      // 新增
        pstmt->executeUpdate();
        
        // 获取生成的ID
        delete pstmt;
        pstmt = conn->prepareStatement("SELECT LAST_INSERT_ID() as id");
        sql::ResultSet* rs = pstmt->executeQuery();
        if (rs->next()) {
            character.id = rs->getInt("id");
        }
        delete rs;
        delete pstmt;
        
        // 插入技能信息
        for (const auto& skill : character.skills) {
            pstmt = conn->prepareStatement(
                "INSERT INTO skills (character_id, name, stage, current_exp, max_stage_exp) "
                "VALUES (?, ?, ?, ?, ?)"
            );
            pstmt->setInt(1, character.id);
            pstmt->setString(2, skill.name);
            pstmt->setString(3, skill.stage);
            pstmt->setInt(4, skill.current_exp);
            pstmt->setInt(5, skill.max_stage_exp);
            pstmt->executeUpdate();
            delete pstmt;
        }
        
        conn->commit();
        conn->setAutoCommit(true);
        return true;
    } catch (const sql::SQLException &e) {
        std::cerr << "添加角色时SQL错误: " << e.what() << std::endl;
        try {
            conn->rollback();
        } catch (...) {}
        return false;
    }
}


bool DatabaseManager::deleteCharacter(int characterId) {
    if (!conn) return false;
    
    try {
        conn->setAutoCommit(false);
        
        // 删除关联技能
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "DELETE FROM skills WHERE character_id = ?"
        );
        pstmt->setInt(1, characterId);
        pstmt->executeUpdate();
        delete pstmt;
        
        // 删除角色
        pstmt = conn->prepareStatement(
            "DELETE FROM characters WHERE id = ?"
        );
        pstmt->setInt(1, characterId);
        int rowsAffected = pstmt->executeUpdate();
        delete pstmt;
        
        conn->commit();
        conn->setAutoCommit(true);
        return rowsAffected > 0;
    } catch (const sql::SQLException &e) {
        std::cerr << "删除角色时SQL错误: " << e.what() << std::endl;
        try {
            conn->rollback();
        } catch (...) {}
        return false;
    }
}


// DatabaseManager.cpp
std::vector<DatabaseManager::SimulationHistory> DatabaseManager::loadSimulationHistory(int characterId) {
    std::vector<SimulationHistory> historyList;
    if (!conn) return historyList;
    
    try {
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "SELECT id, character_id, simulation_days, "
            "JSON_EXTRACT(time_allocation, '$') AS time_allocation, "
            "JSON_EXTRACT(before_snapshot, '$') AS before_snapshot, "
            "JSON_EXTRACT(after_snapshot, '$') AS after_snapshot, "
            "created_at "
            "FROM simulation_history "
            "WHERE character_id = ? "
            "ORDER BY created_at DESC"
        );
        pstmt->setInt(1, characterId);
        sql::ResultSet* res = pstmt->executeQuery();
        
        while (res->next()) {
            SimulationHistory history;
            history.id = res->getInt("id");
            history.character_id = res->getInt("character_id");
            history.simulation_days = res->getInt("simulation_days");
            history.time_allocation = res->getString("time_allocation");
            history.before_snapshot = res->getString("before_snapshot");
            history.after_snapshot = res->getString("after_snapshot");
            history.created_at = res->getString("created_at");
            historyList.push_back(history);
        }
        
        delete res;
        delete pstmt;
    } catch (const sql::SQLException &e) {
        std::cerr << "加载历史记录时SQL错误: " << e.what() << std::endl;
    }
    return historyList;
}
