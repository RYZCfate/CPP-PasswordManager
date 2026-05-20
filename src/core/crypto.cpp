#include "crypto.hpp"
#include <array>
#include <algorithm>
#include <mutex>
#include <utility>

namespace {
    // Argon2id/XChaCha20-Poly1305 固定参数，写成常量便于统一管理
    constexpr std::size_t ARGON2_SALT_LEN = crypto_pwhash_SALTBYTES;
    constexpr std::size_t XCHACHA20_NONCE_LEN = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    constexpr std::size_t XCHACHA20_MAC_LEN = crypto_aead_xchacha20poly1305_ietf_ABYTES;
    constexpr std::size_t KEY_LEN = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;

    // 文件魔法头：用于识别文件格式/版本
    constexpr std::string_view MAGIC_HEADER = "PWDG\x01";

    // 程序生命周期内只缓存 Key，不缓存明文密码
    static std::array<unsigned char, KEY_LEN> g_master_key{};
    static std::array<unsigned char, ARGON2_SALT_LEN> g_db_salt{};
    static bool g_has_master_key = false;
    static bool g_has_db_salt = false;
    static bool g_master_key_locked = false;
    static std::mutex g_crypto_state_mutex;

    void wipe_password(SecureString& pwd) {
        if (!pwd.empty()) {
            sodium_memzero(pwd.data(), pwd.size());
        }
        pwd.clear();
        pwd.shrink_to_fit();
    }

    void wipe_crypto_state_unlocked() {
        sodium_memzero(g_master_key.data(), g_master_key.size());
        sodium_memzero(g_db_salt.data(), g_db_salt.size());
        g_has_master_key = false;
        g_has_db_salt = false;
    }
}

// 初始化 libsodium（失败时返回 false）
bool init_crypto_env() {
    if (sodium_init() < 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_crypto_state_mutex);
    if (!g_master_key_locked) {
        if (sodium_mlock(g_master_key.data(), g_master_key.size()) != 0) {
            return false;
        }
        g_master_key_locked = true;
    }

    return true;
}

// 用主密码+Salt 派生主密钥，并立即擦除主密码
bool init_master_key(SecureString& pwd, const std::vector<unsigned char>& salt) {
    const auto wipePwd = [&]() { wipe_password(pwd); };
    std::lock_guard<std::mutex> lock(g_crypto_state_mutex);
    wipe_crypto_state_unlocked();

    // 1. 处理盐值 (Salt)
    if (salt.empty()) {
        // 全新数据库：生成固定 Salt
        randombytes_buf(g_db_salt.data(), g_db_salt.size());
    }
    else {
        // 现有数据库：复用读取出来的 Salt（长度必须严格匹配）
        if (salt.size() != ARGON2_SALT_LEN) {
            wipePwd();
            return false;
        }
        std::copy(salt.begin(), salt.end(), g_db_salt.begin());
    }
    g_has_db_salt = true;

    // 2. 派生主密钥（耗时操作，需避免重复）
    int res = crypto_pwhash(g_master_key.data(), g_master_key.size(),
        pwd.c_str(), pwd.length(),
        g_db_salt.data(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_ARGON2ID13);

    // 3. 立即安全擦除主密码内存
    wipePwd();

    if (res != 0) {
        wipe_crypto_state_unlocked();
        return false;
    }

    g_has_master_key = true;
    return true;
}

// [新增 API] 清除内存中的主密钥与盐值
void clear_master_key() {
    std::lock_guard<std::mutex> lock(g_crypto_state_mutex);
    wipe_crypto_state_unlocked();
}

// 使用主密钥进行认证加密（密文包含魔法头+Salt+Nonce+Ciphertext）
std::string encrypt(const std::string& plain) {
    std::lock_guard<std::mutex> lock(g_crypto_state_mutex);
    if (!g_has_master_key || !g_has_db_salt) return "";

    std::array<unsigned char, XCHACHA20_NONCE_LEN> nonce{};
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(plain.size() + XCHACHA20_MAC_LEN);
    unsigned long long ciphertext_len = 0;

    // 直接使用缓存好的 g_master_key 加密，避免极耗时的 Argon2 重复计算
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
        ciphertext.data(), &ciphertext_len,
        reinterpret_cast<const unsigned char*>(plain.data()), plain.size(),
        reinterpret_cast<const unsigned char*>(MAGIC_HEADER.data()), MAGIC_HEADER.size(),
        nullptr, nonce.data(), g_master_key.data()) != 0) {
        sodium_memzero(ciphertext.data(), ciphertext.size());
        sodium_memzero(nonce.data(), nonce.size());
        return "";
    }

    std::string out;
    out.reserve(MAGIC_HEADER.size() + g_db_salt.size() + nonce.size() + ciphertext_len);
    out.append(MAGIC_HEADER);
    out.append(reinterpret_cast<char*>(g_db_salt.data()), g_db_salt.size());
    out.append(reinterpret_cast<char*>(nonce.data()), nonce.size());
    out.append(reinterpret_cast<char*>(ciphertext.data()), ciphertext_len);

    sodium_memzero(ciphertext.data(), ciphertext.size());
    sodium_memzero(nonce.data(), nonce.size());

    return out;
}

// 解析密文并验证完整性，失败时给出明确的错误类型
DecryptResult decrypt(const std::string& cipher) {
    std::lock_guard<std::mutex> lock(g_crypto_state_mutex);
    if (!g_has_master_key || !g_has_db_salt) return { DecryptError::WrongPassword, "" };

    size_t min_len = MAGIC_HEADER.size() + ARGON2_SALT_LEN + XCHACHA20_NONCE_LEN + XCHACHA20_MAC_LEN;
    if (cipher.size() < min_len) return { DecryptError::CorruptedFile, "" };

    if (cipher.compare(0, MAGIC_HEADER.size(), MAGIC_HEADER) != 0) {
        return { DecryptError::VersionMismatch, "" };
    }

    const unsigned char* salt = reinterpret_cast<const unsigned char*>(cipher.data() + MAGIC_HEADER.size());
    const unsigned char* nonce = salt + ARGON2_SALT_LEN;
    const unsigned char* ciphertext = nonce + XCHACHA20_NONCE_LEN;
    unsigned long long ciphertext_len = cipher.size() - MAGIC_HEADER.size() - ARGON2_SALT_LEN - XCHACHA20_NONCE_LEN;

    // 解密前先确保文件中的 Salt 与当前内存中用于派生主密钥的 Salt 一致
    if (sodium_memcmp(salt, g_db_salt.data(), ARGON2_SALT_LEN) != 0) {
        return { DecryptError::WrongPassword, "" };
    }

    std::vector<unsigned char> plain(ciphertext_len - XCHACHA20_MAC_LEN);
    unsigned long long plain_len = 0;

    // 直接使用缓存好的 g_master_key 解密
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
        plain.data(), &plain_len,
        nullptr,
        ciphertext, ciphertext_len,
        reinterpret_cast<const unsigned char*>(MAGIC_HEADER.data()), MAGIC_HEADER.size(),
        nonce, g_master_key.data()) != 0) {
        if (!plain.empty()) {
            sodium_memzero(plain.data(), plain.size());
        }
        return { DecryptError::WrongPassword, "" };
    }

    if (plain_len > plain.size()) {
        if (!plain.empty()) {
            sodium_memzero(plain.data(), plain.size());
        }
        return { DecryptError::CorruptedFile, "" };
    }

    std::string plain_text(reinterpret_cast<char*>(plain.data()), plain_len);
    if (!plain.empty()) {
        sodium_memzero(plain.data(), plain.size());
    }
    return { DecryptError::None, std::move(plain_text) };
}
