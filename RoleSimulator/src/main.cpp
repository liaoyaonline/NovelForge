#include "DatabaseManager.h"
#include "SimulatorApp.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    DatabaseManager db;
    if (!db.connect("config/db_config.json")) {
        std::cerr << "无法连接数据库，程序退出。\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "成功连接数据库！\n";
    std::vector<Character> characters = db.loadCharacters();
    
    UI ui;
    ui.displayCharacterTable(characters);
    
    int selectedIndex = ui.selectCharacter(characters);
    if (selectedIndex == -1) {
        std::cerr << "未选择角色，程序退出。\n";
        return EXIT_FAILURE;
    }
    
    Character selectedChar = characters[selectedIndex];
    std::cout << "\n已选择角色: " << selectedChar.name << '\n';
    
    auto skillStages = db.loadSkillStages();
    auto cultivationStages = db.loadCultivationStages();

    // 为每个角色计算修为总经验
    for (Character& character : characters) {
        character.calculateTotalExp(cultivationStages);
    }
    auto skillMultipliers = db.loadSkillMultipliers();
    
    if (skillStages.empty() || cultivationStages.empty() || skillMultipliers.empty()) {
        std::cerr << "错误: 配置加载失败，程序退出。\n";
        return EXIT_FAILURE;
    }
    
    if (selectedChar.cultivation_skill.empty()) {
        std::cerr << "错误: 该角色未指定修为技能!\n";
        return EXIT_FAILURE;
    }
    
    bool cultivationSkillFound = false;
    for (const auto& skill : selectedChar.skills) {
        if (skill.name == selectedChar.cultivation_skill) {
            cultivationSkillFound = true;
            auto multIt = skillMultipliers.find(skill.stage);
            if (multIt != skillMultipliers.end()) {
                std::cout << "  当前熟练度等级: " << skill.stage 
                          << " (加成: " << multIt->second << "x)\n";
            }
            break;
        }
    }
    
    if (!cultivationSkillFound) {
        std::cerr << "错误: 指定的修为技能 '" << selectedChar.cultivation_skill 
                  << "' 不在角色技能列表中!\n";
        return EXIT_FAILURE;
    }
    
    auto stageIt = cultivationStages.find(selectedChar.cultivation_level);
    if (stageIt == cultivationStages.end()) {
        std::cerr << "警告: 未知的修为阶段 '" << selectedChar.cultivation_level << "'\n";
    }
    
    InputHandler input;
    int days = input.getSimulationDays();
    TimeAllocation dailyAlloc = input.getDailyTimeAllocation(selectedChar);
    
    Character resultChar = selectedChar;
    Simulator simulator;
    simulator.simulate(resultChar, skillStages, cultivationStages, skillMultipliers, days, dailyAlloc);
    
    std::cout << "\n===== 推演结果对比 =====\n";
    std::cout << "修为: " << selectedChar.cultivation_level << " (" 
              << selectedChar.cultivation_progress << ") → "
              << resultChar.cultivation_level << " (" 
              << resultChar.cultivation_progress << ")" 
              << " [总经验: " << selectedChar.cultivation_total_exp 
              << " → " << resultChar.cultivation_total_exp << "]\n";
    
    for (size_t i = 0; i < selectedChar.skills.size(); i++) {
        const auto& origSkill = selectedChar.skills[i];
        const auto& newSkill = resultChar.skills[i];
        
        std::cout << "技能 [" << origSkill.name << "]: "
                  << origSkill.stage << " " << origSkill.current_exp << "/" << origSkill.max_stage_exp
                  << " → " << newSkill.stage << " " << newSkill.current_exp << "/" << newSkill.max_stage_exp
                  << '\n';
    }
    
    ResultSaver saver;
    fs::path resultDir = "results";
    fs::create_directories(resultDir);
    
    std::time_t now = std::time(nullptr);
    char timeBuf[20];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", std::localtime(&now));
    
    std::string filename = resultDir.string() + "/" + 
                           selectedChar.name + "_" + 
                           std::string(timeBuf) + ".json";
    
    saver.saveSimulationResult(selectedChar, resultChar, filename);
    std::cout << "\n推演完成！结果已保存至: " << filename << '\n';
    
    // 询问用户是否更新到数据库
    std::string choice;
    while (true) {
        std::cout << "\n是否将推演结果更新到数据库? (y/n): ";
        std::cin >> choice;
        
        if (choice == "y" || choice == "Y") {
            // 更新角色信息
            if (db.updateCharacter(resultChar)) {
                std::cout << "角色信息更新成功!" << std::endl;
                
                // 保存历史记录
                if (db.saveSimulationHistory(resultChar.id, days, dailyAlloc, selectedChar, resultChar)) {
                    std::cout << "历史记录保存成功!" << std::endl;
                } else {
                    std::cerr << "保存历史记录失败!" << std::endl;
                }
            } else {
                std::cerr << "更新角色信息失败!" << std::endl;
            }
            break;
        } else if (choice == "n" || choice == "N") {
            std::cout << "跳过数据库更新." << std::endl;
            break;
        } else {
            std::cout << "无效输入，请输入 y 或 n." << std::endl;
        }
    }
    
    return EXIT_SUCCESS;
}
