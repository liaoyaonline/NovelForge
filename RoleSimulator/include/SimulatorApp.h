#ifndef SIMULATOR_APP_H
#define SIMULATOR_APP_H

#include "DatabaseManager.h"
#include <iostream>
#include <vector>
#include <map>

class InputHandler {
public:
    int getSimulationDays() const;
    TimeAllocation getDailyTimeAllocation(const Character& character) const;
};

class Simulator {
public:
    void simulate(Character& character, 
                 const std::map<std::string, SkillStage>& skillStages,
                 const std::map<std::string, CultivationStage>& cultivationStages,
                 const std::map<std::string, double>& skillMultipliers,
                 int days, 
                 const TimeAllocation& dailyAlloc) const;
    
private:
    void dailyTraining(Character& character, 
                      const std::map<std::string, SkillStage>& skillStages,
                      const std::map<std::string, CultivationStage>& cultivationStages,
                      const std::map<std::string, double>& skillMultipliers,
                      const TimeAllocation& alloc) const;
                      
    void handleSkillUpgrade(Skill& skill, 
                           const std::map<std::string, SkillStage>& skillStages) const;
                           
    void handleCultivationUpgrade(Character& character,
                                 const std::map<std::string, CultivationStage>& cultivationStages) const;
};

class UI {
public:
    void displayCharacterTable(const std::vector<Character>& characters) const;
    int selectCharacter(const std::vector<Character>& characters) const;
};

class ResultSaver {
public:
    void saveSimulationResult(const Character& before, 
                             const Character& after,
                             const std::string& filename) const;
};

#endif // SIMULATOR_APP_H
