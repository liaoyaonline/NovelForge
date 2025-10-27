#ifndef CHARACTER_H
#define CHARACTER_H

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>

struct Skill {
    std::string name;
    std::string stage;
    int current_exp;
    int max_stage_exp;
};

struct SkillStage {
    std::string stage_name;
    int stage_max_exp;
    double avg_rate;
};

struct CultivationStage {
    std::string level;
    int min_exp;
    int exp_required;
    double base_rate;
    int time_required;
};

struct TimeAllocation {
    std::map<std::string, double> skillHours;  // 技能训练时间（小时）
    double restHours;  // 休息时间（小时）
    // 移除了 meditationHours 字段
};

class Character {
public:
    int id;
    std::string name;
    std::string race;
    int age;
    std::string power_level;
    std::string cultivation_level;
    std::string cultivation_progress;
    std::string cultivation_skill;  // 用于修为提升的特殊技能
    int cultivation_total_exp = 0;  // 修为总经验值
    std::vector<Skill> skills;      // 角色掌握的技能列表
    std::string talent;       // 新增天赋字段
    std::string comment;      // 新增评论字段

    // 获取当前修为值（进度条分子）
    int getCultivationValue() const;
    
    // 获取当前修为上限（进度条分母）
    int getCultivationMax() const;
    
    // 根据修为阶段配置计算总经验值
    void calculateTotalExp(const std::map<std::string, CultivationStage>& stages);
    
    // 更新修为进度字符串
    void updateCultivationProgress(const std::map<std::string, CultivationStage>& stages);
};

#endif // CHARACTER_H
