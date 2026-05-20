#include "storage.hpp"
#include "crypto.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sodium.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

// 加密文件的默认路径与文件头
static const fs::path SECRETS_FILE = "secrets.enc";//加密文件路径
static constexpr std::string_view MAGIC_HEADER = "PWDG\x01";

// 只读取文件头与 Salt，用于初始化主密钥
std::vector<unsigned char> extract_salt_from_db() {
    if (!fs::exists(SECRETS_FILE)) return {};

    std::ifstream in(SECRETS_FILE, std::ios::binary);
    if (!in) return {};

    std::string header(MAGIC_HEADER.size(), '\0');
    in.read(header.data(), header.size());
    if (header != MAGIC_HEADER) return {}; // 校验失败

    std::vector<unsigned char> salt(crypto_pwhash_SALTBYTES);
    in.read(reinterpret_cast<char*>(salt.data()), salt.size());
    if (in.gcount() != static_cast<std::streamsize>(salt.size())) return {};

    return salt;
}

// 从磁盘读取密文 -> 解密 -> 解析 JSON -> 生成内存中的条目
bool load_entries(std::vector<Credential>& entries) {
    if (!fs::exists(SECRETS_FILE)) {
        return true; // 新数据库
    }

    std::ifstream in(SECRETS_FILE, std::ios::binary);
    if (!in) return false;

    std::string cipher((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // 调用底层解密函数获取明文
    DecryptResult result = decrypt(cipher);
    if (!cipher.empty()) {
        sodium_memzero(cipher.data(), cipher.size());
        cipher.clear();
    }

    // 精细化错误处理
    if (result.error != DecryptError::None) {
        switch (result.error) {
        case DecryptError::WrongPassword:
            std::cerr << "[错误] 密码错误或密钥被篡改，无法解密。\n";
            break;
        case DecryptError::CorruptedFile:
            std::cerr << "[错误] 密码库文件损坏或格式不完整。\n";
            break;
        case DecryptError::VersionMismatch:
            std::cerr << "[错误] 无法识别的数据库格式，或版本不兼容。\n";
            break;
        default:
            std::cerr << "[错误] 未知的解密错误。\n";
            break;
        }
        if (!result.plain.empty()) {
            sodium_memzero(result.plain.data(), result.plain.size());
            result.plain.clear();
        }
        return false;
    }

    // 明文是 JSON，反序列化为结构体列表
    try {
        auto j = json::parse(result.plain);
        entries = j.get<std::vector<Credential>>();
        if (!result.plain.empty()) {
            sodium_memzero(result.plain.data(), result.plain.size());
            result.plain.clear();
        }
        return true;
    }
    catch (const json::exception&) {
        std::cerr << "[错误] JSON 解析失败，数据文件可能已被篡改。\n";
        if (!result.plain.empty()) {
            sodium_memzero(result.plain.data(), result.plain.size());
            result.plain.clear();
        }
        return false;
    }
}

// 把内存条目序列化为 JSON -> 加密 -> 写回磁盘
bool save_entries(const std::vector<Credential>& entries) {
    json j = entries;
    std::string plain = j.dump();
    std::string cipher = encrypt(plain);
    if (!plain.empty()) {
        sodium_memzero(plain.data(), plain.size());
    }
    plain.clear();

    if (cipher.empty()) return false;

    // 先写入临时文件，再安全替换，避免覆盖失败导致的数据丢失
    fs::path temp_file = SECRETS_FILE.string() + ".tmp";
    fs::path backup_file = SECRETS_FILE.string() + ".bak";
    std::ofstream out(temp_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        sodium_memzero(cipher.data(), cipher.size());
        cipher.clear();
        return false;
    }

    out.write(cipher.data(), cipher.size());
    sodium_memzero(cipher.data(), cipher.size());
    cipher.clear();
    if (!out.good()) {
        out.close();
        std::error_code cleanup_ec;
        fs::remove(temp_file, cleanup_ec);
        return false;
    }
    out.close();
    if (!out.good()) {
        std::error_code cleanup_ec;
        fs::remove(temp_file, cleanup_ec);
        return false;
    }

    std::error_code ec;
    const bool secrets_exists = fs::exists(SECRETS_FILE, ec);
    if (ec) {
        std::cerr << "[系统错误] 检查数据库文件状态失败: " << ec.message() << "\n";
        std::error_code cleanup_ec;
        fs::remove(temp_file, cleanup_ec);
        return false;
    }

    if (!secrets_exists) {
        fs::rename(temp_file, SECRETS_FILE, ec);
        if (ec) {
            std::cerr << "[系统错误] 归档数据文件失败: " << ec.message() << "\n";
            std::error_code cleanup_ec;
            fs::remove(temp_file, cleanup_ec);
            return false;
        }
#ifndef _WIN32
        std::error_code perm_ec;
        fs::permissions(SECRETS_FILE, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, perm_ec);
        if (perm_ec) {
            std::cerr << "[系统错误] 无法收紧数据库文件权限: " << perm_ec.message() << "\n";
            return false;
        }
#endif
        return true;
    }

    // Windows 下 rename 不能覆盖存在文件；使用备份回滚策略保证失败时不丢库
    fs::remove(backup_file, ec);
    ec.clear();
    fs::rename(SECRETS_FILE, backup_file, ec);
    if (ec) {
        std::cerr << "[系统错误] 创建数据库备份失败: " << ec.message() << "\n";
        std::error_code cleanup_ec;
        fs::remove(temp_file, cleanup_ec);
        return false;
    }

    fs::rename(temp_file, SECRETS_FILE, ec);
    if (ec) {
        std::cerr << "[系统错误] 写入新数据库失败，尝试回滚: " << ec.message() << "\n";
        std::error_code rollback_ec;
        fs::rename(backup_file, SECRETS_FILE, rollback_ec);
        if (rollback_ec) {
            std::cerr << "[系统错误] 回滚数据库失败: " << rollback_ec.message() << "\n";
        }
        std::error_code cleanup_ec;
        fs::remove(temp_file, cleanup_ec);
        return false;
    }

    fs::remove(backup_file, ec);

#ifndef _WIN32
    std::error_code perm_ec;
    fs::permissions(SECRETS_FILE, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, perm_ec);
    if (perm_ec) {
        std::cerr << "[系统错误] 无法收紧数据库文件权限: " << perm_ec.message() << "\n";
        return false;
    }
#endif

    return true;
}

// 在内存中按站点查找条目
Credential* find_entry(std::vector<Credential>& entries, const std::string& site) {
    auto it = std::ranges::find_if(entries, [&site](const Credential& c) {
        return c.site == site;
        });
    return (it != entries.end()) ? &(*it) : nullptr;
}

// 删除指定站点条目
bool delete_entry(std::vector<Credential>& entries, const std::string& site) {
    auto [first, last] = std::ranges::remove_if(entries, [&site](const Credential& c) {
        return c.site == site;
        });
    if (first == entries.end()) return false;
    entries.erase(first, last);
    return true;
}
