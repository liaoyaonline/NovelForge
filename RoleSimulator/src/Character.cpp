#include "Character.h"
#include <cctype> // 用于 isdigit
#include <stdexcept> // 包含标准异常头文件

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

void Character::updateCultivationProgress(const std::map<std::string, CultivationStage>& stages) {
    auto it = stages.find(cultivation_level);
    if (it == stages.end()) {
        throw std::runtime_error("未知的修为阶段: " + cultivation_level);
    }
    
    const CultivationStage& stage = it->second;
    int currentExpInStage = cultivation_total_exp - stage.min_exp;
    
    if (currentExpInStage < 0) currentExpInStage = 0;
    if (currentExpInStage > stage.exp_required) currentExpInStage = stage.exp_required;
    
    cultivation_progress = std::to_string(currentExpInStage) + "/" + std::to_string(stage.exp_required);
}


void Character::calculateTotalExp(const std::map<std::string, CultivationStage>& stages) {
    try {
        // 获取当前修为值
        int current_value = getCultivationValue();
        
        // 查找当前修为阶段
        auto it = stages.find(cultivation_level);
        if (it == stages.end()) {
            throw std::runtime_error("未知的修为阶段: " + cultivation_level);
        }
        
        // 计算总经验 = 阶段最小经验 + 当前阶段获得经验
        cultivation_total_exp = it->second.min_exp + current_value;
    } catch (const std::exception& e) {
        std::cerr << "计算修为总经验错误: " << e.what() << '\n';
        cultivation_total_exp = 0;
    }
}