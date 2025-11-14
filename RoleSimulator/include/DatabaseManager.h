#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "Character.h"
#include <cppconn/driver.h>
#include <vector>
#include <map>
#include <string>

class DatabaseManager {
private:
    sql::Connection* conn;
    void calculateStageMinExp(std::map<std::string, CultivationStage>& stages);

public:
    DatabaseManager();
    ~DatabaseManager();
    struct SimulationHistory {
        int id;
        int character_id;
        int simulation_days;
        std::string time_allocation;
        std::string before_snapshot;
        std::string after_snapshot;
        std::string created_at;
    };
    
    std::vector<SimulationHistory> loadSimulationHistory(int characterId);
    bool connect(const std::string& configPath);
    std::vector<Character> loadCharacters();
    std::map<std::string, SkillStage> loadSkillStages();
    std::map<std::string, CultivationStage> loadCultivationStages();
    std::map<std::string, double> loadSkillMultipliers();
    bool saveCharacter(const Character& character);
    bool addCharacter(Character& character);
    bool deleteCharacter(int characterId);
    // 更新方法
    void validateStageContinuity(std::map<std::string, CultivationStage>& stages);
    bool updateCharacter(const Character& character);
    bool saveSimulationHistory(int characterId, 
                              int days, 
                              const TimeAllocation& alloc, 
                              const Character& before, 
                              const Character& after);
};

#endif // DATABASE_MANAGER_H
