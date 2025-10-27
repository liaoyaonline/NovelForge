#include "SimulatorApp.h"
#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <map>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <fstream>
#include <ctime>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// InputHandler 实现
int InputHandler::getSimulationDays() const {
    int days;
    while (true) {
        std::cout << "请输入推演天数 (1-365): ";
        if (!(std::cin >> days)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "输入无效，请输入一个整数。\n";
            continue;
        }
        
        if (days < 1 || days > 365) {
            std::cout << "天数必须在1到365之间，请重新输入。\n";
        } else {
            break;
        }
    }
    return days;
}

TimeAllocation InputHandler::getDailyTimeAllocation(const Character& character) const {
    TimeAllocation alloc;
    float totalHours = 0.0f;
    const float maxDailyHours = 24.0f;
    
    std::cout << "\n===== 每日时间分配 =====" << std::endl;
    std::cout << "请为以下技能分配时间 (单位:小时，一天最多24小时)：\n";
    
    // 为每个技能分配时间
    for (const auto& skill : character.skills) {
        float hours;
        std::string skillName = skill.name;
        
        while (true) {
            std::cout << "  " << skillName;
            // 如果是修为技能，显示特殊标识
            if (skillName == character.cultivation_skill) {
                std::cout << " [修为技能]";
            }
            std::cout << " (当前阶段: " << skill.stage << "): ";
            
            if (!(std::cin >> hours)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "输入无效，请输入一个数字。\n";
                continue;
            }
            
            if (hours < 0) {
                std::cout << "时间不能为负数，请重新输入。\n";
            } else if (totalHours + hours > maxDailyHours) {
                std::cout << "总时间已超过24小时，请调整分配。\n";
            } else {
                alloc.skillHours[skillName] = hours;
                totalHours += hours;
                break;
            }
        }
    }
    
    // 分配休息时间
    while (true) {
        float restHours;
        std::cout << "休息时间: ";
        if (!(std::cin >> restHours)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "输入无效，请输入一个数字。\n";
            continue;
        }
        
        if (restHours < 0) {
            std::cout << "时间不能为负数，请重新输入。\n";
        } else if (totalHours + restHours > maxDailyHours) {
            std::cout << "总时间已超过24小时，请调整分配。\n";
        } else {
            alloc.restHours = restHours;
            totalHours += restHours;
            break;
        }
    }
    
    std::cout << "时间分配完成: 技能修炼 " << (totalHours - alloc.restHours) 
              << "小时，休息 " << alloc.restHours << "小时\n";
    
    return alloc;
}
// Simulator 实现
const std::map<std::string, int> chineseNumbers = {
    {"一", 1}, {"二", 2}, {"三", 3}, {"四", 4}, {"五", 5},
    {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"十", 10}
};

void Simulator::dailyTraining(Character& character, 
                             const std::map<std::string, SkillStage>& skillStages,
                             const std::map<std::string, CultivationStage>& cultivationStages,
                             const std::map<std::string, double>& skillMultipliers,
                             const TimeAllocation& alloc) const {
    // 1. 技能熟练度提升
    for (auto& skill : character.skills) {
        auto it = alloc.skillHours.find(skill.name);
        if (it != alloc.skillHours.end() && it->second > 0) {
            // 查找该技能当前阶段的配置
            auto stageIt = skillStages.find(skill.stage);
            if (stageIt != skillStages.end()) {
                // 计算熟练度增长
                float expGain = it->second * stageIt->second.avg_rate;
                skill.current_exp += static_cast<int>(std::round(expGain));
                
                // 检查是否达到升级条件
                handleSkillUpgrade(skill, skillStages);
                
                // 如果是修为技能，同时增加修为
                if (skill.name == character.cultivation_skill) {
                    // 查找当前修为阶段配置
                    auto cultivationStageIt = cultivationStages.find(character.cultivation_level);
                    if (cultivationStageIt != cultivationStages.end()) {
                        const CultivationStage& stageConfig = cultivationStageIt->second;
                        
                        // 查找技能熟练度加成
                        double multiplier = 1.0;
                        auto multIt = skillMultipliers.find(skill.stage);
                        if (multIt != skillMultipliers.end()) {
                            multiplier = multIt->second;
                        }
                        
                        // 计算修为增长 = 时间 * 基础速率 * 熟练度加成
                        double cultivationExpGain = it->second * stageConfig.base_rate * multiplier;
                        character.cultivation_total_exp += static_cast<int>(std::round(cultivationExpGain));
                        
                        // 检查是否达到突破条件
                        handleCultivationUpgrade(character, cultivationStages);
                    }
                }
            }
        }
    }
}

void Simulator::handleCultivationUpgrade(Character& character,
                                        const std::map<std::string, CultivationStage>& cultivationStages) const {
    auto currentStageIt = cultivationStages.find(character.cultivation_level);
    if (currentStageIt == cultivationStages.end()) {
        throw std::runtime_error("未知的修为阶段: " + character.cultivation_level);
    }
    
    const CultivationStage& currentStage = currentStageIt->second;
    int currentExpInStage = character.cultivation_total_exp - currentStage.min_exp;
    
    if (currentExpInStage >= currentStage.exp_required) {
        std::string nextLevel;
        for (const auto& stage : cultivationStages) {
            if (stage.second.min_exp == currentStage.min_exp + currentStage.exp_required) {
                nextLevel = stage.first;
                break;
            }
        }
        
        if (!nextLevel.empty()) {
            character.cultivation_level = nextLevel;
            std::cout << "修为突破! 达到 " << nextLevel << "!\n";
        } else {
            currentExpInStage = currentStage.exp_required;
            character.cultivation_total_exp = currentStage.min_exp + currentStage.exp_required;
        }
    }
    character.updateCultivationProgress(cultivationStages);
}

void Simulator::handleSkillUpgrade(Skill& skill, 
                                  const std::map<std::string, SkillStage>& skillStages) const {
    if (skill.current_exp >= skill.max_stage_exp) {
        const std::string stages[] = {"入门", "熟练", "精通", "专家", "大师", "宗师"};
        bool foundCurrent = false;
        std::string nextStage;
        
        for (const auto& stage : stages) {
            if (foundCurrent) {
                if (skillStages.find(stage) != skillStages.end()) {
                    nextStage = stage;
                    break;
                }
            }
            if (stage == skill.stage) foundCurrent = true;
        }
        
        if (!nextStage.empty()) {
            int overflowExp = skill.current_exp - skill.max_stage_exp;
            skill.stage = nextStage;
            auto stageIt = skillStages.find(nextStage);
            if (stageIt != skillStages.end()) {
                skill.max_stage_exp = stageIt->second.stage_max_exp;
                skill.current_exp = overflowExp;
                std::cout << "技能 [" << skill.name << "] 提升至 " << nextStage << " 阶段！\n";
            } else {
                skill.current_exp = skill.max_stage_exp;
            }
        } else {
            skill.current_exp = skill.max_stage_exp;
        }
    }
}

void Simulator::simulate(Character& character, 
                        const std::map<std::string, SkillStage>& skillStages,
                        const std::map<std::string, CultivationStage>& cultivationStages,
                        const std::map<std::string, double>& skillMultipliers,
                        int days, 
                        const TimeAllocation& dailyAlloc) const {
    Character original = character;
    
    std::cout << "\n开始推演...\n";
    
    for (int day = 1; day <= days; day++) {
        if (day == 1 || day % 5 == 0 || day == days) {
            std::cout << "\n--- 第 " << day << " 天 ---\n";
            std::cout << "境界: " << character.cultivation_level 
                      << " (" << character.cultivation_progress << ")\n";
            std::cout << "修为总经验: " << character.cultivation_total_exp << '\n';
            for (const auto& skill : character.skills) {
                std::cout << skill.name << ": " << skill.stage << " " 
                         << skill.current_exp << "/" << skill.max_stage_exp;
                if (skill.name == character.cultivation_skill) {
                    auto multIt = skillMultipliers.find(skill.stage);
                    if (multIt != skillMultipliers.end()) {
                        std::cout << " (加成: " << multIt->second << "x)";
                    }
                }
                std::cout << '\n';
            }
        }
        
        dailyTraining(character, skillStages, cultivationStages, skillMultipliers, dailyAlloc);
        
        if (day % 10 == 0 || day == days) {
            std::cout << "第 " << day << " 天推演完成\n";
        }
    }
    
    std::cout << "\n推演完成！\n";
}

// UI 实现
void UI::displayCharacterTable(const std::vector<Character>& characters) const {
    if (characters.empty()) {
        std::cout << "\n没有可用的角色\n";
        return;
    }
    
    std::cout << "\n====== 角色属性表 ======\n";
    for (size_t i = 0; i < characters.size(); ++i) {
        const Character& ch = characters[i];
        std::cout << "[" << i+1 << "] " << ch.name << " (" << ch.race << ")\n";
        std::cout << "  年龄: " << ch.age << "岁\n";
        std::cout << "  实力: " << ch.power_level << "\n";
        std::cout << "  修为: " << ch.cultivation_level 
                  << " (" << ch.cultivation_progress << ")\n";
        
        if (!ch.talent.empty()) {
            std::cout << "  天赋: " << ch.talent << "\n";
        }
        
        if (!ch.comment.empty()) {
            std::cout << "  评论: " << ch.comment << "\n";
        }
        
        std::cout << "  技能:\n";
        for (const auto& skill : ch.skills) {
            std::cout << "    - " << skill.name << " (" << skill.stage << " "
                      << skill.current_exp << "/" << skill.max_stage_exp << ")\n";
        }
        std::cout << "------------------------\n";
    }
}

int UI::selectCharacter(const std::vector<Character>& characters) const {
    if (characters.empty()) return -1;
    
    int choice;
    while (true) {
        std::cout << "请选择角色 (1-" << characters.size() << "): ";
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "无效输入，请重新输入。\n";
            continue;
        }
        
        if (choice < 1 || choice > static_cast<int>(characters.size())) {
            std::cout << "无效选择，请重新输入。\n";
        } else {
            break;
        }
    }
    return choice - 1;
}

// ResultSaver 实现
void ResultSaver::saveSimulationResultText(const Character& character,
                                         const std::string& filename) const {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "无法保存文本结果到文件: " << filename << '\n';
        return;
    }
    
    // 格式示例：
    // 【姓名】：韩江（鳄族）
    // 【年龄】：0岁/19岁
    // 【实力】：黑铁
    // 【修为】：练气一层(171/1000)
    // 【技能】：微光星幕阵（精通125/400），阵星引气决（精通116/400）
    // 【天赋】：共生 Lv.1
    // 【评论】：籍籍无名的虫豸！
    
    outFile << "【姓名】：" << character.name << "（" << character.race << "）\n";
    outFile << "【年龄】：" << character.age << "岁\n";
    outFile << "【实力】：" << character.power_level << "\n";
    outFile << "【修为】：" << character.cultivation_level << "(" << character.cultivation_progress << ")\n";
    
    // 技能
    outFile << "【技能】：";
    for (size_t i = 0; i < character.skills.size(); ++i) {
        const Skill& skill = character.skills[i];
        outFile << skill.name << "（" << skill.stage << " " << skill.current_exp << "/" << skill.max_stage_exp << "）";
        if (i < character.skills.size() - 1) {
            outFile << "，";
        }
    }
    outFile << "\n";
    
    outFile << "【天赋】：" << character.talent << "\n";
    outFile << "【评论】：" << character.comment << "\n";
    
    outFile.close();
    std::cout << "文本格式推演结果已保存至: " << filename << '\n';
}

void ResultSaver::saveSimulationResult(const Character& before, 
                                      const Character& after,
                                      const std::string& filename) const {
    json result;
    std::time_t now = std::time(nullptr);
    result["timestamp"] = std::ctime(&now);
    
    json beforeJson;
    beforeJson["name"] = before.name;
    beforeJson["race"] = before.race;
    beforeJson["age"] = before.age;
    beforeJson["power_level"] = before.power_level;
    beforeJson["cultivation_level"] = before.cultivation_level;
    beforeJson["cultivation_progress"] = before.cultivation_progress;
    beforeJson["talent"] = before.talent;  // 新增
    beforeJson["comment"] = before.comment; // 新增
    
    json beforeSkills;
    for (const auto& skill : before.skills) {
        json s;
        s["name"] = skill.name;
        s["stage"] = skill.stage;
        s["current_exp"] = skill.current_exp;
        s["max_stage_exp"] = skill.max_stage_exp;
        beforeSkills.push_back(s);
    }
    beforeJson["skills"] = beforeSkills;
    result["before"] = beforeJson;
    
    json afterJson = beforeJson; // 复制基础信息
    afterJson["cultivation_level"] = after.cultivation_level;
    afterJson["cultivation_progress"] = after.cultivation_progress;
    afterJson["talent"] = after.talent;        // 新增
    afterJson["comment"] = after.comment;       // 新增
    
    json afterSkills;
    for (const auto& skill : after.skills) {
        json s;
        s["name"] = skill.name;
        s["stage"] = skill.stage;
        s["current_exp"] = skill.current_exp;
        s["max_stage_exp"] = skill.max_stage_exp;
        afterSkills.push_back(s);
    }
    afterJson["skills"] = afterSkills;
    result["after"] = afterJson;
    
    json changes;
    changes["cultivation_level"] = before.cultivation_level + " → " + after.cultivation_level;
    changes["cultivation_progress"] = before.cultivation_progress + " → " + after.cultivation_progress;
    
    json skillChanges;
    for (size_t i = 0; i < before.skills.size(); i++) {
        const auto& bSkill = before.skills[i];
        const auto& aSkill = after.skills[i];
        
        json change;
        change["name"] = bSkill.name;
        change["stage"] = bSkill.stage + " → " + aSkill.stage;
        change["progress"] = std::to_string(bSkill.current_exp) + "/" + std::to_string(bSkill.max_stage_exp) +
                             " → " + std::to_string(aSkill.current_exp) + "/" + std::to_string(aSkill.max_stage_exp);
        skillChanges.push_back(change);
    }
    changes["skills"] = skillChanges;
    result["changes"] = changes;
    
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        outFile << std::setw(4) << result << '\n';
        std::cout << "JSON格式推演结果已保存至: " << filename << '\n';
    } else {
        std::cerr << "无法保存JSON结果到文件: " << filename << '\n';
    }
    
    // 保存文本格式的结果
    std::string textFilename = filename.substr(0, filename.find_last_of('.')) + ".txt";
    saveSimulationResultText(after, textFilename);
}
// SimulatorApp.cpp
int UI::characterManagementMenu(std::vector<Character>& characters, DatabaseManager& db) {
    int choice;
    do {
        // 1. 显示所有角色属性表
        std::cout << "\n===== 角色列表 =====" << std::endl;
        displayCharacterTable(characters);
        
        // 2. 显示操作菜单
        std::cout << "\n===== 角色管理操作 =====" << std::endl;
        std::cout << "1. 新增角色" << std::endl;
        std::cout << "2. 删除角色" << std::endl;
        std::cout << "3. 修改角色" << std::endl;
        std::cout << "4. 推演角色" << std::endl;
        std::cout << "0. 返回主菜单" << std::endl;
        std::cout << "请选择操作: ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        switch (choice) {
            case 1: {
                // 新增角色
                Character newChar = createCharacterFromInput();
                if (db.addCharacter(newChar)) {
                    std::cout << "角色创建成功! ID: " << newChar.id << std::endl;
                    characters = db.loadCharacters(); // 刷新列表
                } else {
                    std::cout << "角色创建失败!" << std::endl;
                }
                break;
            }
            case 2: {
                // 删除角色
                if (characters.empty()) {
                    std::cout << "没有可删除的角色!" << std::endl;
                    break;
                }
                int index = selectCharacter(characters);
                if (index == -1) break;
                
                Character& selectedChar = characters[index];
                std::cout << "\n确定要删除角色: " << selectedChar.name 
                          << " (ID: " << selectedChar.id << ")? [y/N]: ";
                
                std::string confirm;
                std::getline(std::cin, confirm);
                
                if (confirm == "y" || confirm == "Y") {
                    if (db.deleteCharacter(selectedChar.id)) {
                        std::cout << "角色删除成功!" << std::endl;
                        characters = db.loadCharacters(); // 刷新列表
                    } else {
                        std::cout << "角色删除失败!" << std::endl;
                    }
                }
                break;
            }
            case 3: {
                // 修改角色
                if (characters.empty()) {
                    std::cout << "没有可编辑的角色!" << std::endl;
                    break;
                }
                int index = selectCharacter(characters);
                if (index == -1) break;
                
                Character selectedChar = characters[index];
                std::cout << "\n编辑角色: " << selectedChar.name << " (ID: " << selectedChar.id << ")" << std::endl;
                
                // 创建编辑表单
                Character updatedChar = createCharacterFromInput();
                updatedChar.id = selectedChar.id; // 保持相同ID
                
                if (db.updateCharacter(updatedChar)) {
                    std::cout << "角色更新成功!" << std::endl;
                    characters = db.loadCharacters(); // 刷新列表
                } else {
                    std::cout << "角色更新失败!" << std::endl;
                }
                break;
            }
            case 4: {
                // 推演角色
                if (characters.empty()) {
                    std::cout << "没有可推演的角色!" << std::endl;
                    break;
                }
                int index = selectCharacter(characters);
                if (index == -1) break;
                
                // 调用推演功能
                return index; // 返回选中的角色索引
            }
            case 0:
                std::cout << "返回主菜单..." << std::endl;
                break;
            default:
                std::cout << "无效选择，请重新输入!" << std::endl;
        }
    } while (choice != 0);
    
    return -1;
}
void UI::displayCreateCharacterForm(DatabaseManager& db) {
    std::cout << "\n===== 创建新角色 =====" << std::endl;
    Character newChar = createCharacterFromInput();
    
    if (db.addCharacter(newChar)) {
        std::cout << "角色创建成功! ID: " << newChar.id << std::endl;
    } else {
        std::cout << "角色创建失败!" << std::endl;
    }
}

void UI::displayEditCharacterForm(std::vector<Character>& characters, DatabaseManager& db) {
    if (characters.empty()) {
        std::cout << "没有可编辑的角色!" << std::endl;
        return;
    }
    
    displayCharacterTable(characters);
    int index = selectCharacter(characters);
    if (index == -1) return;
    
    Character& selectedChar = characters[index];
    std::cout << "\n编辑角色: " << selectedChar.name << " (ID: " << selectedChar.id << ")" << std::endl;
    
    // 显示当前信息并获取新输入
    Character updatedChar = createCharacterFromInput();
    updatedChar.id = selectedChar.id; // 保持相同ID
    
    if (db.updateCharacter(updatedChar)) {
        std::cout << "角色更新成功!" << std::endl;
    } else {
        std::cout << "角色更新失败!" << std::endl;
    }
}

void UI::displayDeleteCharacterDialog(std::vector<Character>& characters, DatabaseManager& db) {
    if (characters.empty()) {
        std::cout << "没有可删除的角色!" << std::endl;
        return;
    }
    
    displayCharacterTable(characters);
    int index = selectCharacter(characters);
    if (index == -1) return;
    
    Character& selectedChar = characters[index];
    std::cout << "\n确定要删除角色: " << selectedChar.name 
              << " (ID: " << selectedChar.id << ")? [y/N]: ";
    
    std::string confirm;
    std::getline(std::cin, confirm);
    
    if (confirm == "y" || confirm == "Y") {
        if (db.deleteCharacter(selectedChar.id)) {
            std::cout << "角色删除成功!" << std::endl;
        } else {
            std::cout << "角色删除失败!" << std::endl;
        }
    }
}

Character UI::createCharacterFromInput() {
    Character character;
    
    std::cout << "\n===== 角色信息输入 =====" << std::endl;
    std::cout << "角色名称: ";
    std::getline(std::cin, character.name);
    
    std::cout << "种族: ";
    std::getline(std::cin, character.race);
    
    std::cout << "年龄: ";
    std::cin >> character.age;
    std::cin.ignore();
    
    std::cout << "实力等级: ";
    std::getline(std::cin, character.power_level);
    
    std::cout << "修为等级: ";
    std::getline(std::cin, character.cultivation_level);
    
    std::cout << "修为进度 (格式: 当前值/最大值): ";
    std::getline(std::cin, character.cultivation_progress);

    std::cout << "天赋: ";
    std::getline(std::cin, character.talent);
    
    std::cout << "评论: ";
    std::getline(std::cin, character.comment);
    
    std::cout << "修为技能: ";
    std::getline(std::cin, character.cultivation_skill);
    
    // 添加技能
    std::cout << "\n===== 添加技能 =====" << std::endl;
    std::cout << "输入技能信息 (技能名称留空结束添加)" << std::endl;
    while (true) {
        Skill skill;
        
        std::cout << "技能名称 (留空结束): ";
        std::getline(std::cin, skill.name);
        if (skill.name.empty()) break;
        
        std::cout << "阶段: ";
        std::getline(std::cin, skill.stage);
        
        std::cout << "当前经验值: ";
        std::cin >> skill.current_exp;
        
        std::cout << "阶段最大经验值: ";
        std::cin >> skill.max_stage_exp;
        std::cin.ignore(); // 清除输入缓冲区
        
        character.skills.push_back(skill);
    }
    
    return character;
}

Skill UI::createSkillFromInput() {
    Skill skill;
    
    std::cout << "技能名称 (留空结束): ";
    std::getline(std::cin, skill.name);
    if (skill.name.empty()) return skill;
    
    std::cout << "阶段: ";
    std::getline(std::cin, skill.stage);
    
    std::cout << "当前经验值: ";
    std::cin >> skill.current_exp;
    
    std::cout << "阶段最大经验值: ";
    std::cin >> skill.max_stage_exp;
    
    std::cin.ignore(); // 清除输入缓冲区
    return skill;
}
