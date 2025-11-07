#include "AliasManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>

using json = nlohmann::json;

// === Helpers ===
std::string AliasManager::normalize(const std::string& s) {
    std::string res = s;
    std::transform(res.begin(), res.end(), res.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // обрезаем пробелы в начале/конце
    size_t start = res.find_first_not_of(" \t\n\r");
    size_t end = res.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return res.substr(start, end - start + 1);
}

// === Initialization / Loading ===
bool AliasManager::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "AliasManager: failed to open " << path << "\n";
        return false;
    }

    aliasToId.clear();
    idToAliases.clear();

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

    std::unordered_set<std::string> ids;

    for (auto& entry : j["aliases"]) {
        if (!entry.contains("id") || !entry.contains("name")) {
            std::cerr << "AliasManager: missing id or name\n";
            continue;
        }

        std::string id = entry.value("id", "");
        std::string name = entry.value("name", "");

        if (ids.count(id)) {
            std::cerr << "AliasManager: duplicate alias id " << id << "\n";
            continue;
        }
        ids.insert(id);

        if (id.empty() || name.empty()) continue;

        std::string norm = normalize(name);
        aliasToId[norm] = id;
        idToAliases[id].push_back(name);
    }

    std::cout << "AliasManager: loaded " << aliasToId.size() << " aliases\n";
    return true;
}

// === Core API ===
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
            auto it = idToAliases.find(id);
            if (it == idToAliases.end()) continue;

            for (auto& alias : it->second) {
                if (results.size() >= limit) break;
                results.push_back(alias);
            }
        }
    }
    return results;
}