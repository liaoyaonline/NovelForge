#include "db_config.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

DBConfig loadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("无法打开配置文件");
    json data = json::parse(f);
    return {
        data["host"].get<std::string>(),
        data["user"].get<std::string>(),
        data["password"].get<std::string>(),
        data["database"].get<std::string>(),
        data["port"].get<int>()
    };
}
