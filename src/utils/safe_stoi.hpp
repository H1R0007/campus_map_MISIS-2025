#pragma once
#include <string>
#include <climits>

// === safeStoi ===
// Без исключений, чистая проверка числового содержимого строки.
inline int safeStoi(const std::string& s, int fallback = 0) {
    if (s.empty()) return fallback;

    bool negative = false;
    size_t i = 0;

    if (s[0] == '-') {
        negative = true;
        i = 1;
    }

    if (i >= s.size()) return fallback;

    long long result = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c < '0' || c > '9') return fallback;
        result = result * 10 + (c - '0');
        if (result > INT_MAX) return fallback;
    }

    return negative ? -static_cast<int>(result) : static_cast<int>(result);
}