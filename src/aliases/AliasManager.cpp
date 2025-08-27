#include "AliasManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

std::string AliasManager::normalize(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return res;
}

bool AliasManager::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "AliasManager: failed to open " << path << "\n";
        return false;
    }

    json j;
    try {
        file >> j;
    }
    catch (std::exception& e) {
        std::cerr << "AliasManager: parse error: " << e.what() << "\n";
        return false;
    }

    if (!j.contains("aliases") || !j["aliases"].is_array()) {
        std::cerr << "AliasManager: invalid structure, expected {\"aliases\": [...]}\n";
        return false;
    }

    aliasToId.clear();
    idToAliases.clear();

    for (auto& entry : j["aliases"]) {
        std::string id = entry.value("id", "");
        std::string name = entry.value("name", "");
        if (id.empty() || name.empty()) continue;

        std::string norm = normalize(name);
        aliasToId[norm] = id;
        idToAliases[id].push_back(name);
    }

    std::cout << "AliasManager: loaded " << aliasToId.size() << " aliases\n";
    return true;
}

std::string AliasManager::resolve(const std::string& alias) const {
    std::string norm = normalize(alias);
    auto it = aliasToId.find(norm);
    if (it != aliasToId.end()) return it->second;
    return "";
}

std::vector<std::string> AliasManager::suggest(const std::string& partial, size_t limit) const {
    std::string norm = normalize(partial);
    std::vector<std::string> results;

    for (auto& [aliasNorm, id] : aliasToId) {
        if (aliasNorm.find(norm) != std::string::npos) {
            // вернуть оригинал (как он был записан в idToAliases)
            for (auto& alias : idToAliases.at(id)) {
                if (results.size() < limit)
                    results.push_back(alias);
            }
        }
    }
    return results;
}