#include "security.hpp"
#include <cctype>

// 简单规则：长度>=8，且同时包含字母和数字
bool is_strong_password(const std::string& pwd) {
    if (pwd.length() < 8) return false;
    bool has_digit = false, has_alpha = false;
    for (char ch : pwd) {
        if (std::isdigit(static_cast<unsigned char>(ch))) has_digit = true;
        if (std::isalpha(static_cast<unsigned char>(ch))) has_alpha = true;
    }
    return has_digit && has_alpha;
}