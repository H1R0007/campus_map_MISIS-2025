#pragma once
#include <string>
#include <unordered_map>
#include <vector>


// === AliasManager ===
// Responsible for managing human-readable names of nodes.
// Provides resolution (alias -> id) and autocompletion suggestions.
class AliasManager {
public:
    AliasManager() = default;

    // === Initialization / Loading ===
    bool load(const std::string& path);

    // === Core API ===
    // Resolve alias (case-insensitive) to its node id. Returns "" if not found.
    std::string resolve(const std::string& alias) const;

    // Suggest aliases matching a partial string (case-insensitive).
    // Returns up to 'limit' suggestions.
    std::vector<std::string> suggest(const std::string& partial, size_t limit = 3) const;

private:
    // === Internal data ===
    // alias(lowercase) → node_id
    std::unordered_map<std::string, std::string> aliasToId;

    // node_id → [aliases]
    std::unordered_map<std::string, std::vector<std::string>> idToAliases;

    // === Helpers ===
    static std::string normalize(const std::string& s);
};