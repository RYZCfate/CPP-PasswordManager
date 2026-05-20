#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <sodium.h>

// 安全内存分配器：利用 libsodium 确保内存被锁定且释放时自动擦除
template <typename T>
struct SodiumAllocator {
    using value_type = T;
    SodiumAllocator() noexcept {}
    template <typename U> SodiumAllocator(const SodiumAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::size_t(-1) / sizeof(T)) throw std::bad_alloc();
        void* p = sodium_malloc(n * sizeof(T));
        if (!p) throw std::bad_alloc();
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t) noexcept {
        sodium_free(p);
    }
};

// 类型别名：使用安全分配器的字符串
using SecureString = std::basic_string<char, std::char_traits<char>, SodiumAllocator<char>>;
using SecureVector = std::vector<unsigned char, SodiumAllocator<unsigned char>>;

// 解密错误状态枚举
enum class DecryptError {
    None,               // 成功
    WrongPassword,      // 密码错误/密钥被篡改/MAC校验失败
    CorruptedFile,      // 文件损坏（长度不足）
    VersionMismatch     // 版本不匹配（魔法头校验失败）
};

// 解密结果结构体
struct DecryptResult {
    DecryptError error;
    std::string plain;
};

// ==========================================
// 核心加密层 API
// ==========================================

// 初始化 libsodium（失败时返回 false）
bool init_crypto_env();

// 用主密码+Salt 派生主密钥，并立即擦除传入的主密码
bool init_master_key(SecureString& pwd, const std::vector<unsigned char>& salt);

// 使用主密钥进行认证加密
std::string encrypt(const std::string& plain);

// 解析密文并验证完整性，失败时给出明确的错误类型
DecryptResult decrypt(const std::string& cipher);

// [新增 API] 安全清除内存中的主密钥和盐值（用于锁定保险库）
void clear_master_key();