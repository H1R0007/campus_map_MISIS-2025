#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class AliasManager {
public:
    // загрузить из файла
    bool load(const std::string& path);

    // найти ID по алиасу (точное совпадение)
    std::string resolve(const std::string& alias) const;

    // подсказки (по подстроке, insensitively)
    std::vector<std::string> suggest(const std::string& partial, size_t limit = 3) const;

private:
    // alias → node_id
    std::unordered_map<std::string, std::string> aliasToId;

    // node_id → [aliases]
    std::unordered_map<std::string, std::vector<std::string>> idToAliases;

    // нормализация строки (в нижний регистр)
    static std::string normalize(const std::string& s);
};