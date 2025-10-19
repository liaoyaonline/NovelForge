// ====== Config.cpp ======
#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

// 辅助函数：去除字符串两端空白
static std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    
    return str.substr(start, end - start + 1);
}

// 辅助函数：转换为小写
static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
        [](unsigned char c){ return std::tolower(c); });
    return result;
}

Config::Config(const std::string& filePath) : configFilePath(filePath) {
    reload();
}

void Config::reload() {
    configData.clear();
    
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开配置文件: " + configFilePath);
    }
    
    std::string currentSection = "default";
    std::string line;
    
    while (std::getline(file, line)) {
        // 跳过空行和注释
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        
        // 处理节 [section]
        if (line[0] == '[' && line.back() == ']') {
            currentSection = toLower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        
        // 处理键值对
        parseLine(line);
    }
}

void Config::parseLine(const std::string& line) {
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return; // 无效行
    }
    
    std::string key = toLower(trim(line.substr(0, pos)));
    std::string value = trim(line.substr(pos + 1));
    
    // 确定节（这里简化处理，实际应跟踪当前节）
    std::string section = "database"; // 默认数据库节
    
    // 如果检测到应用配置项
    if (key == "log_level" || key == "page_size" || key == "log_file") {
        section = "application";
    }
    
    configData[section][key] = value;
}

void Config::save() {
    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        throw std::runtime_error("无法写入配置文件: " + configFilePath);
    }
    
    // 写入数据库配置
    file << "[database]\n";
    file << "host = " << getString("database", "host", "127.0.0.1") << "\n";
    file << "port = " << getInt("database", "port", 3306) << "\n";
    file << "username = " << getString("database", "username", "") << "\n";
    file << "password = " << getString("database", "password", "") << "\n";
    file << "database = " << getString("database", "database", "geartracker") << "\n\n";
    
    // 写入应用配置
    file << "[application]\n";
    file << "log_level = " << getString("application", "log_level", "info") << "\n";
    file << "page_size = " << getInt("application", "page_size", 10) << "\n";
    file << "log_file = " << getString("application", "log_file", "geartracker.log") << "\n";
    
    file.close();
}

std::string Config::getString(const std::string& section, const std::string& key, const std::string& defaultValue) {
    std::string sec = toLower(section);
    std::string k = toLower(key);
    
    if (configData.find(sec) != configData.end()) {
        auto& sectionMap = configData[sec];
        if (sectionMap.find(k) != sectionMap.end()) {
            return sectionMap[k];
        }
    }
    return defaultValue;
}

int Config::getInt(const std::string& section, const std::string& key, int defaultValue) {
    std::string value = getString(section, key, "");
    if (value.empty()) return defaultValue;
    
    try {
        return std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

bool Config::getBool(const std::string& section, const std::string& key, bool defaultValue) {
    std::string value = toLower(getString(section, key, ""));
    
    if (value == "true" || value == "yes" || value == "1") {
        return true;
    } else if (value == "false" || value == "no" || value == "0") {
        return false;
    }
    return defaultValue;
}

void Config::setString(const std::string& section, const std::string& key, const std::string& value) {
    configData[toLower(section)][toLower(key)] = value;
}

void Config::setInt(const std::string& section, const std::string& key, int value) {
    setString(section, key, std::to_string(value));
}

void Config::setBool(const std::string& section, const std::string& key, bool value) {
    setString(section, key, value ? "true" : "false");
}
