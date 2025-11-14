#include "DatabaseManager.h"
#include "SimulatorApp.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <limits>

namespace fs = std::filesystem;

// 修改后的运行模拟器函数
void runSimulator(DatabaseManager& db, const Character& selectedChar) {
    std::cout << "\n已选择角色: " << selectedChar.name << '\n';
    
    auto skillStages = db.loadSkillStages();
    auto cultivationStages = db.loadCultivationStages();
    auto skillMultipliers = db.loadSkillMultipliers();
    
    if (skillStages.empty() || cultivationStages.empty() || skillMultipliers.empty()) {
        std::cerr << "错误: 配置加载失败，请检查数据库连接。\n";
        return;
    }
    
    // 创建临时角色副本并计算初始总经验
    Character startChar = selectedChar;
    startChar.calculateTotalExp(cultivationStages);
    
    // 显示初始修为信息
    std::cout << "初始修为: " << startChar.cultivation_level 
              << " (" << startChar.cultivation_progress << ")"
              << " [总经验: " << startChar.cultivation_total_exp << "]\n";
    
    if (startChar.cultivation_skill.empty()) {
        std::cerr << "错误: 该角色未指定修为技能!\n";
        return;
    }
    
    bool cultivationSkillFound = false;
    for (const auto& skill : startChar.skills) {
        if (skill.name == startChar.cultivation_skill) {
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
        std::cerr << "错误: 指定的修为技能 '" << startChar.cultivation_skill 
                  << "' 不在角色技能列表中!\n";
        return;
    }
    
    auto stageIt = cultivationStages.find(startChar.cultivation_level);
    if (stageIt == cultivationStages.end()) {
        std::cerr << "警告: 未知的修为阶段 '" << startChar.cultivation_level << "'\n";
    }
    
    InputHandler input;
    int days = input.getSimulationDays();
    TimeAllocation dailyAlloc = input.getDailyTimeAllocation(startChar);
    
    // 使用正确的初始经验创建结果角色
    Character resultChar = startChar;
    
    Simulator simulator;
    simulator.simulate(resultChar, skillStages, cultivationStages, skillMultipliers, days, dailyAlloc);
    
    // 确保天赋和评论也被传递
    resultChar.talent = startChar.talent;
    resultChar.comment = startChar.comment;
    
    // 更新修为进度显示
    resultChar.updateCultivationProgress(cultivationStages);
    
    std::cout << "\n===== 推演结果对比 =====\n";
    std::cout << "修为: " << startChar.cultivation_level << " (" 
              << startChar.cultivation_progress << ") → "
              << resultChar.cultivation_level << " (" 
              << resultChar.cultivation_progress << ")" 
              << " [总经验: " << startChar.cultivation_total_exp 
              << " → " << resultChar.cultivation_total_exp << "]\n";
    
    for (size_t i = 0; i < startChar.skills.size(); i++) {
        const auto& origSkill = startChar.skills[i];
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
                           startChar.name + "_" + 
                           std::string(timeBuf) + ".json";
    
    saver.saveSimulationResult(startChar, resultChar, filename);
    std::cout << "\n推演完成！结果已保存至: " << filename << '\n';
    
    // 询问用户是否更新到数据库
    std::string choice;
    while (true) {
        std::cout << "\n是否将推演结果更新到数据库? (y/n): ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 清除输入缓冲区
        
        if (choice == "y" || choice == "Y") {
            // 更新角色信息
            if (db.updateCharacter(resultChar)) {
                std::cout << "角色信息更新成功!" << std::endl;
                
                // 保存历史记录
                if (db.saveSimulationHistory(resultChar.id, days, dailyAlloc, startChar, resultChar)) {
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
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    DatabaseManager db;
    if (!db.connect("config/db_config.json")) {
        std::cerr << "无法连接数据库，程序退出。\n";
        return EXIT_FAILURE;
    }
    
    std::cout << "成功连接数据库！\n";
    
    int mainChoice;
    UI ui;
    
    do {
        std::cout << "\n===== 角色模拟器主菜单 =====" << std::endl;
        std::cout << "1. 角色管理" << std::endl;
        std::cout << "0. 退出程序" << std::endl;
        std::cout << "请选择操作: ";
        std::cin >> mainChoice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        
        if (mainChoice == 1) {
            // 加载角色列表
            std::vector<Character> characters = db.loadCharacters();
            
            // 进入角色管理菜单
            int selectedIndex = ui.characterManagementMenu(characters, db);
            
            // 如果用户选择了推演角色
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(characters.size())) {
                Character selectedChar = characters[selectedIndex];
                runSimulator(db, selectedChar);
            }
        } else if (mainChoice != 0) {
            std::cout << "无效选择，请重新输入!" << std::endl;
        }
    } while (mainChoice != 0);
    
    std::cout << "退出程序..." << std::endl;
    return EXIT_SUCCESS;
}
