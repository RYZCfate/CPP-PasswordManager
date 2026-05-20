#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "crypto.hpp"

// 密码凭据数据结构：一个站点对应一条记录
struct Credential {
    std::string site;
    std::string username;
    SecureString password;
    SecureString totp_secret = "";

    // 注意：nlohmann::json 对自定义分配器的 basic_string 支持良好
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Credential, site, username, password, totp_secret)
};

// 从文件中提前提取 Salt（供初始化密钥时使用）
std::vector<unsigned char> extract_salt_from_db();

// 读取并解密所有条目，失败返回 false
bool load_entries(std::vector<Credential>& entries);
// 加密并保存所有条目，失败返回 false
bool save_entries(const std::vector<Credential>& entries);
// 按站点查找条目，找不到返回 nullptr
Credential* find_entry(std::vector<Credential>& entries, const std::string& site);
// 删除指定站点条目，返回是否删除成功
bool delete_entry(std::vector<Credential>& entries, const std::string& site);