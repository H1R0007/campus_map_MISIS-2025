#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

// === AliasManager ===
// Manages human-readable names of nodes, resolution and ranked suggestions.
class AliasManager {
public:
    AliasManager() = default;

    // === Initialization / Loading ===
    // JSON supports:
    // {
    //   "aliases": [
    //     { "id": "A_101", "name": "Аудитория 101" },
    //     { "id": "A_101", "names": ["Room 101", "Лекционная 101"] }
    //   ]
    // }
    bool load(const std::string& path);

    // === Core API ===
    // Resolve alias (normalized, locale-aware lower if enabled) to node id.
    std::string resolve(const std::string& alias) const;

    // Simple suggestions (display strings) up to limit.
    std::vector<std::string> suggest(const std::string& partial, size_t limit = 3) const;

    // Rich suggestions with id and score.
    struct Suggestion {
        std::string alias;
        std::string id;
        int score = 0; // higher is better
    };
    std::vector<Suggestion> suggestDetailed(const std::string& partial, size_t limit = 5) const;

private:
    // alias(normalized) -> node_id
    std::unordered_map<std::string, std::string> aliasToId;
    // node_id -> [display aliases]
    std::unordered_map<std::string, std::vector<std::string>> idToAliases;

    // Searchable index (one record per alias)
    struct AliasRecord {
        std::string id;
        std::string display;
        std::string norm;
        std::vector<std::string> tokens;
        std::string acronym;
    };
    std::vector<AliasRecord> index;

    // === Helpers ===
    static std::string normalize(const std::string& s);               // locale-aware lower (optional) + simplify
    static std::vector<std::string> tokenize(const std::string& s);
    static std::string makeAcronym(const std::vector<std::string>& tokens);
    static std::string swapKeyboardLayout(const std::string& s);      // basic EN->RU
    static bool isSubsequence(const std::string& sub, const std::string& str, int& gaps);
    static int levenshtein(const std::string& a, const std::string& b, int cutoff);

    int scoreMatch(const std::string& qNorm, const std::string& qAlt, const AliasRecord& rec) const;

    // === Cache ===
    struct CacheEntry {
        std::vector<Suggestion> results;
        std::chrono::steady_clock::time_point ts;
    };
    // mutable: we cache in const method
    mutable std::unordered_map<std::string, CacheEntry> cache;
    static constexpr int CACHE_TTL_MS = 400;
    static constexpr size_t CACHE_MAX = 128;
};