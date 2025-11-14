#include "Character.h"
#include <cctype> // 用于 isdigit
#include <stdexcept> // 包含标准异常头文件
#include <iostream> // 添加用于调试输出

int Character::getCultivationValue() const {
    try {
        size_t pos = cultivation_progress.find('/');
        if(pos != std::string::npos) {
            std::string valueStr = cultivation_progress.substr(0, pos);
            if (!valueStr.empty() && isdigit(static_cast<unsigned char>(valueStr[0]))) {
                return std::stoi(valueStr);
            }
            return 0;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "解析修为值错误: " << cultivation_progress << " - " << e.what() << '\n';
        return 0;
    }
}

int Character::getCultivationMax() const {
    try {
        size_t pos = cultivation_progress.find('/');
        if(pos != std::string::npos) {
            std::string maxStr = cultivation_progress.substr(pos + 1);
            if (!maxStr.empty() && isdigit(static_cast<unsigned char>(maxStr[0]))) {
                return std::stoi(maxStr);
            }
            return 100;
        }
        return 100;
    } catch (const std::exception& e) {
        std::cerr << "解析修为上限错误: " << cultivation_progress << " - " << e.what() << '\n';
        return 100;
    }
}

int Character::getCurrentStageMinExp(const std::map<std::string, CultivationStage>& stages) const {
    auto it = stages.find(cultivation_level);
    if (it != stages.end()) {
        return it->second.min_exp;
    }
    
    // 如果找不到，尝试模糊匹配
    for (const auto& stage : stages) {
        if (stage.first.find(cultivation_level) != std::string::npos) {
            return stage.second.min_exp;
        }
    }
    
    return 0;
}

int Character::getCurrentStageExp(const std::map<std::string, CultivationStage>& stages) const {
    return cultivation_total_exp - getCurrentStageMinExp(stages);
}

void Character::updateCultivationProgress(const std::map<std::string, CultivationStage>& stages) {
    int minExp = getCurrentStageMinExp(stages);
    int currentStageExp = cultivation_total_exp - minExp;
    
    auto it = stages.find(cultivation_level);
    if (it == stages.end()) {
        // 尝试模糊匹配
        for (const auto& stage : stages) {
            if (stage.first.find(cultivation_level) != std::string::npos) {
                cultivation_level = stage.first;
                it = stages.find(cultivation_level);
                break;
            }
        }
        
        if (it == stages.end()) {
            throw std::runtime_error("未知的修为阶段: " + cultivation_level);
        }
    }
    
    const CultivationStage& stage = it->second;
    
    // 确保经验值在有效范围内
    if (currentStageExp < 0) currentStageExp = 0;
    if (currentStageExp > stage.exp_required) {
        // 如果超过当前阶段，只显示最大值
        currentStageExp = stage.exp_required;
    }
    
    cultivation_progress = std::to_string(currentStageExp) + "/" + std::to_string(stage.exp_required);
}

void Character::calculateTotalExp(const std::map<std::string, CultivationStage>& stages) {
    try {
        // 如果已经计算过总经验（非零值），并且进度与总经验一致，则跳过计算
        if (cultivation_total_exp > 0) {
            int currentValue = getCultivationValue();
            int minExp = getCurrentStageMinExp(stages);
            int calculatedTotalExp = minExp + currentValue;
            
            // 调试日志：检查一致性与差异
            std::cout << "检查修为总经验: 当前总经验=" << cultivation_total_exp 
                      << ", 计算值=" << calculatedTotalExp
                      << ", 差异=" << (cultivation_total_exp - calculatedTotalExp) << '\n';
            
            // 允许5点以内的误差，避免因四舍五入导致不一致
            if (std::abs(cultivation_total_exp - calculatedTotalExp) <= 5) {
                return;
            }
        }
        
        // 获取当前修为值
        int current_value = getCultivationValue();
        
        // 获取当前阶段最小经验
        int min_exp = getCurrentStageMinExp(stages);
        
        // 计算总经验 = 阶段最小经验 + 当前阶段获得经验
        cultivation_total_exp = min_exp + current_value;
        
        // 详细调试日志
        std::cout << "计算修为总经验: 阶段=" << cultivation_level
                  << ", 进度=" << cultivation_progress
                  << ", 当前值=" << current_value
                  << ", 最小经验=" << min_exp
                  << ", 总经验=" << cultivation_total_exp << '\n';
    } catch (const std::exception& e) {
        std::cerr << "计算修为总经验错误: " << e.what() << '\n';
        cultivation_total_exp = 0;
    }
}
