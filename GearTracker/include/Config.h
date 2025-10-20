// ====== Config.h ======
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <stdexcept>

class Config {
private:
    std::map<std::string, std::map<std::string, std::string>> configData;
    std::string configFilePath;
    void parseLine(const std::string& line);
public:    
    Config(); 
    Config(const std::string& filePath); 
    std::string getConfigFilePath();
    void reload();
    void save();
    
    std::string getString(const std::string& section, const std::string& key, const std::string& defaultValue = "");
    int getInt(const std::string& section, const std::string& key, int defaultValue = 0);
    bool getBool(const std::string& section, const std::string& key, bool defaultValue = false);
    
    void setString(const std::string& section, const std::string& key, const std::string& value);
    void setInt(const std::string& section, const std::string& key, int value);
    void setBool(const std::string& section, const std::string& key, bool value);
};

#endif // CONFIG_H
