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
        // 移除了 cultivation_total_exp 字段的查询
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT id, name, race, age, power_level, "
            "cultivation_level, cultivation_progress, "
            "cultivation_skill "  // 移除了 cultivation_total_exp
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
        sql::ResultSet* res = stmt->executeQuery(
            "SELECT level, exp_required, base_rate, time_required "
            "FROM cultivation_stages ORDER BY time_required ASC"
        );
        
        while (res->next()) {
            CultivationStage stage;
            stage.level = res->getString("level");
            stage.exp_required = res->getInt("exp_required");
            stage.base_rate = res->getDouble("base_rate");
            stage.time_required = res->getInt("time_required");
            stages[stage.level] = stage;
        }
        delete res;
        delete stmt;
        calculateStageMinExp(stages);
    } catch (const sql::SQLException &e) {
        std::cerr << "加载修为阶段时SQL错误: " << e.what() << '\n';
    }
    return stages;
}

void DatabaseManager::calculateStageMinExp(std::map<std::string, CultivationStage>& stages) {
    int totalExp = 0;
    for (auto& pair : stages) {
        pair.second.min_exp = totalExp;
        totalExp += pair.second.exp_required;
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

// 添加更新角色方法
bool DatabaseManager::updateCharacter(const Character& character) {
    if (!conn) {
        std::cerr << "数据库未连接" << std::endl;
        return false;
    }
    
    try {
        // 开始事务
        conn->setAutoCommit(false);
        
        // 1. 更新角色基本信息
        sql::PreparedStatement* pstmt = conn->prepareStatement(
            "UPDATE characters SET "
            "cultivation_level = ?, "
            "cultivation_progress = ? "
            "WHERE id = ?"
        );
        pstmt->setString(1, character.cultivation_level);
        pstmt->setString(2, character.cultivation_progress);
        pstmt->setInt(3, character.id);
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

// 添加保存历史记录方法
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
        
        // 推演前快照
        json beforeJson;
        beforeJson["name"] = before.name;
        beforeJson["cultivation_level"] = before.cultivation_level;
        beforeJson["cultivation_progress"] = before.cultivation_progress;
        for (const auto& skill : before.skills) {
            json skillJson;
            skillJson["name"] = skill.name;
            skillJson["stage"] = skill.stage;
            skillJson["current_exp"] = skill.current_exp;
            skillJson["max_stage_exp"] = skill.max_stage_exp;
            beforeJson["skills"].push_back(skillJson);
        }
        
        // 推演后快照
        json afterJson = beforeJson; // 复制基础信息
        afterJson["cultivation_level"] = after.cultivation_level;
        afterJson["cultivation_progress"] = after.cultivation_progress;
        for (size_t i = 0; i < after.skills.size(); i++) {
            afterJson["skills"][i]["stage"] = after.skills[i].stage;
            afterJson["skills"][i]["current_exp"] = after.skills[i].current_exp;
            afterJson["skills"][i]["max_stage_exp"] = after.skills[i].max_stage_exp;
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

