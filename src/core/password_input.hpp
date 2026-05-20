#pragma once
#include <string>
#include "crypto.hpp" // 为了使用 SecureString

// 提示用户输入密码（不回显），返回安全内存中的 SecureString
// prompt_msg 用于显示提示语，支持多种场景复用（主密码/站点密码）
SecureString prompt_for_password(const std::string& prompt_msg);
