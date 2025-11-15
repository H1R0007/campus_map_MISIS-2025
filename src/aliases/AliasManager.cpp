#include "AliasManager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#ifdef ENABLE_UNICODE_FOLD
#include <boost/locale.hpp>
#endif

using json = nlohmann::json;

// Small UTF‑8 helper
static inline int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// === Helpers ===
std::string AliasManager::normalize(const std::string& s) {
    // 1) Lowercase (Unicode-aware if enabled)
    std::string lowered = s;
#ifdef ENABLE_UNICODE_FOLD
    try {
        static boost::locale::generator gen;
        static std::locale loc = gen("ru_RU.UTF-8"); // универсально для латиницы/кириллицы
        lowered = boost::locale::to_lower(s, loc);
    }
    catch (...) {
        lowered = s; // fallback
    }
#endif

    // 2) Replace ASCII punctuation to spaces; keep UTF‑8 runes intact
    std::string out;
    out.reserve(lowered.size());
    bool prevSpace = true;

    for (size_t i = 0; i < lowered.size(); ) {
        unsigned char c = static_cast<unsigned char>(lowered[i]);
        int l = utf8_char_len(c);

        if (l == 1) {
            // ASCII branch — ВАЖНО: во всех ветках продвигаем i
            if (std::isalpha(c) || std::isdigit(c)) {
                out.push_back(static_cast<char>(c));
                prevSpace = false;
                ++i; // FIX: не забываем продвигать i
            }
            else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                c == '_' || c == '-' || c == '/' || c == '\\') {
                if (!prevSpace) { out.push_back(' '); prevSpace = true; }
                ++i;
            }
            else {
                if (!prevSpace) { out.push_back(' '); prevSpace = true; }
                ++i;
            }
        }
        else {
            // UTF‑8: копируем руны как есть
            for (int k = 0; k < l && i < lowered.size(); ++k, ++i)
                out.push_back(lowered[i]);
            prevSpace = false;
        }
    }

    if (!out.empty() && out.back() == ' ') out.pop_back();
    size_t start = out.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    if (start > 0) out.erase(0, start);
    return out;
}

std::vector<std::string> AliasManager::tokenize(const std::string& s) {
    std::vector<std::string> tokens;
    std::string cur;
    cur.reserve(s.size());

    auto flush = [&] {
        if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
        };

    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int l = utf8_char_len(c);
        if (l == 1) {
            if (std::isalnum(c)) cur.push_back(static_cast<char>(c));
            else flush();
            ++i;
        }
        else {
            for (int k = 0; k < l && i < s.size(); ++k, ++i)
                cur.push_back(s[i]);
        }
    }
    flush();
    return tokens;
}

std::string AliasManager::makeAcronym(const std::vector<std::string>& tokens) {
    std::string a;
    a.reserve(tokens.size());
    for (const auto& t : tokens) if (!t.empty()) a.push_back(t[0]);
    return a;
}

// EN->RU keyboard layout swap (basic, letters и часть знаков)
std::string AliasManager::swapKeyboardLayout(const std::string& s) {
    static const std::unordered_map<char, const char*> map = {
        {'q', u8"й"}, {'w', u8"ц"}, {'e', u8"у"}, {'r', u8"к"}, {'t', u8"е"},
        {'y', u8"н"}, {'u', u8"г"}, {'i', u8"ш"}, {'o', u8"щ"}, {'p', u8"з"},
        {'[', u8"х"}, {']', u8"ъ"},
        {'a', u8"ф"}, {'s', u8"ы"}, {'d', u8"в"}, {'f', u8"а"}, {'g', u8"п"},
        {'h', u8"р"}, {'j', u8"о"}, {'k', u8"л"}, {'l', u8"д"}, {';', u8"ж"}, {'\'', u8"э"},
        {'z', u8"я"}, {'x', u8"ч"}, {'c', u8"с"}, {'v', u8"м"}, {'b', u8"и"},
        {'n', u8"т"}, {'m', u8"ь"}, {',', u8"б"}, {'.', u8"ю"}, {'`', u8"ё"},
    };
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        char low = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = map.find(low);
        if (it != map.end()) out += it->second;
        else out.push_back(c);
    }
    return out;
}

bool AliasManager::isSubsequence(const std::string& sub, const std::string& str, int& gaps) {
    size_t i = 0, j = 0;
    gaps = 0;
    while (i < sub.size() && j < str.size()) {
        if (sub[i] == str[j]) { ++i; ++j; }
        else { ++j; ++gaps; }
    }
    return i == sub.size();
}

int AliasManager::levenshtein(const std::string& a, const std::string& b, int cutoff) {
    const int n = (int)a.size(), m = (int)b.size();
    if (std::abs(n - m) > cutoff) return cutoff + 1;
    std::vector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i; int best = cur[0];
        char ca = a[i - 1];
        for (int j = 1; j <= m; ++j) {
            char cb = b[j - 1];
            int cost = (ca == cb) ? 0 : 1;
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
            if (cur[j] < best) best = cur[j];
        }
        if (best > cutoff) return cutoff + 1;
        std::swap(prev, cur);
    }
    return prev[m];
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
    index.clear();
    cache.clear();

    json j;
    try { file >> j; }
    catch (std::exception& e) {
        std::cerr << "AliasManager: parse error: " << e.what() << "\n";
        return false;
    }

    if (!j.contains("aliases") || !j["aliases"].is_array()) {
        std::cerr << "AliasManager: invalid structure, expected {\"aliases\": [...]}\n";
        return false;
    }

    std::unordered_set<std::string> uniq;
    auto makeKey = [](const std::string& id, const std::string& norm) { return id + "\n" + norm; };

    size_t totalAliases = 0;
    for (auto& entry : j["aliases"]) {
        if (!entry.contains("id")) continue;
        std::string id = entry.value("id", "");
        if (id.empty()) continue;

        std::vector<std::string> names;
        if (entry.contains("name") && entry["name"].is_string())
            names.push_back(entry["name"].get<std::string>());
        if (entry.contains("names") && entry["names"].is_array())
            for (auto& v : entry["names"]) if (v.is_string()) names.push_back(v.get<std::string>());

        if (names.empty()) continue;

        for (auto& disp : names) {
            if (disp.empty()) continue;
            std::string norm = normalize(disp);
            if (norm.empty()) continue;
            std::string key = makeKey(id, norm);
            if (!uniq.insert(key).second) continue;

            aliasToId[norm] = id;
            idToAliases[id].push_back(disp);

            AliasRecord rec;
            rec.id = id; rec.display = disp; rec.norm = norm;
            rec.tokens = tokenize(norm);
            rec.acronym = makeAcronym(rec.tokens);
            index.push_back(std::move(rec));
            ++totalAliases;
        }
    }

    std::cout << "AliasManager: loaded " << totalAliases
        << " aliases for " << idToAliases.size() << " nodes\n";
    return true;
}

// === Core API ===
std::string AliasManager::resolve(const std::string& alias) const {
    std::string norm = normalize(alias);
    auto it = aliasToId.find(norm);
    if (it != aliasToId.end()) return it->second;
    return "";
}

int AliasManager::scoreMatch(const std::string& qNorm, const std::string& qAlt, const AliasRecord& rec) const {
    if (qNorm.empty()) return -1;

    auto scoreOne = [&](const std::string& q)->int {
        if (q.empty()) return -1;
        if (q == rec.norm) return 100000;
        if (rec.norm.rfind(q, 0) == 0) return 90000 - (int)(rec.norm.size() - q.size());
        for (const auto& t : rec.tokens)
            if (t.rfind(q, 0) == 0) return 85000 - (int)rec.display.size();
        size_t pos = rec.norm.find(q);
        if (pos != std::string::npos) return 80000 - (int)pos * 2 - (int)rec.display.size();
        std::string qAcr = makeAcronym(tokenize(q));
        if (!qAcr.empty() && rec.acronym.rfind(qAcr, 0) == 0) return 78000 - (int)rec.acronym.size();
        int gaps = 0;
        if (isSubsequence(q, rec.norm, gaps)) return 70000 - gaps * 5 - (int)rec.display.size();
        int best = 1e9;
        for (const auto& t : rec.tokens) {
            int d = levenshtein(q, t, 2);
            if (d <= 2 && d < best) best = d;
        }
        if (best != 1e9) return 65000 - best * 200 - (int)rec.display.size();
        return -1;
        };

    int s1 = scoreOne(qNorm);
    int s2 = -1;
    if (!qAlt.empty() && qAlt != qNorm) s2 = scoreOne(qAlt);
    return std::max(s1, s2);
}

std::vector<AliasManager::Suggestion> AliasManager::suggestDetailed(const std::string& partial, size_t limit) const {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();

    if (partial.empty()) return {};
    // Cache hit?
    auto itc = cache.find(partial);
    if (itc != cache.end()) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - itc->second.ts).count();
        if (ms < CACHE_TTL_MS) return itc->second.results;
    }

    std::string qNorm = normalize(partial);
    if (qNorm.empty()) return {};

    std::string qAlt = normalize(swapKeyboardLayout(partial));

    std::vector<Suggestion> out;
    out.reserve(32);
    for (const auto& rec : index) {
        int sc = scoreMatch(qNorm, qAlt, rec);
        if (sc > 0) out.push_back(Suggestion{ rec.display, rec.id, sc });
    }

    std::sort(out.begin(), out.end(), [](const Suggestion& a, const Suggestion& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.alias.size() != b.alias.size()) return a.alias.size() < b.alias.size();
        return a.alias < b.alias;
        });

    // Dedup by alias text
    std::vector<Suggestion> dedup;
    dedup.reserve(limit);
    std::unordered_set<std::string> seen;
    for (const auto& s : out) {
        if (seen.insert(s.alias).second) {
            dedup.push_back(s);
            if (dedup.size() >= limit) break;
        }
    }

    // Store to cache
    if (cache.size() > CACHE_MAX) cache.clear();
    cache[partial] = CacheEntry{ dedup, now };

    return dedup;
}

std::vector<std::string> AliasManager::suggest(const std::string& partial, size_t limit) const {
    auto rich = suggestDetailed(partial, std::max<size_t>(limit, 5));
    std::vector<std::string> simple;
    simple.reserve(limit);
    for (const auto& s : rich) {
        if (simple.size() >= limit) break;
        simple.push_back(s.alias);
    }
    return simple;
}