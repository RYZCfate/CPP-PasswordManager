# PasswordGuard API 与 ImGui 对接接口清单

本文件基于当前所有 `*.cpp` 实现整理，目标是给你后续使用 **ImGui** 做 UI 交互时，直接作为调用参考。

## 1. 代码文件与职责

| 文件                 | 主要职责                                    |
| -------------------- | ------------------------------------------- |
| `passwordguard.cpp`  | 程序入口，初始化加密环境与主密钥            |
| `crypto.cpp`         | 主密钥派生、加解密、密钥清理                |
| `storage.cpp`        | 凭据读写、JSON 序列化、站点查询与删除       |
| `password_input.cpp` | 安全密码输入（CLI）                         |
| `security.cpp`       | 密码强度检查                                |
| `2fa.cpp`            | TOTP 生成/校验、二维码解析、屏幕/摄像头扫码 |

## 2. 项目可直接调用的接口（建议 UI 层优先使用）

### 2.1 加密与主密钥（`crypto.hpp`）

```cpp
bool init_crypto_env();
bool init_master_key(SecureString& pwd, const std::vector<unsigned char>& salt);
std::string encrypt(const std::string& plain);
DecryptResult decrypt(const std::string& cipher);
void clear_master_key();
```

相关类型：

```cpp
using SecureString = std::string;
using SecureVector = std::vector<unsigned char>;

enum class DecryptError {
    None,
    WrongPassword,
    CorruptedFile,
    VersionMismatch
};

struct DecryptResult {
    DecryptError error;
    std::string plain;
};
```

### 2.2 数据存储与凭据管理（`storage.hpp`）

```cpp
struct Credential {
    std::string site;
    std::string username;
    std::string password;
    std::string totp_secret;
};

std::vector<unsigned char> extract_salt_from_db();
bool load_entries(std::vector<Credential>& entries);
bool save_entries(const std::vector<Credential>& entries);
Credential* find_entry(std::vector<Credential>& entries, const std::string& site);
bool delete_entry(std::vector<Credential>& entries, const std::string& site);
```

### 2.3 输入与安全检查

```cpp
SecureString prompt_for_password(const std::string& prompt_msg); // password_input.hpp
bool is_strong_password(const std::string& pwd);                  // security.hpp
```

### 2.4 二次验证 2FA（`2fa.hpp`）

```cpp
bool verify_totp(const std::string& secret_base32, const std::string& user_code);
std::string generate_totp(const std::string& secret_base32);
std::string setup_from_manual(const std::string& input_secret);
std::string setup_from_screen_scan();
std::string setup_from_camera_scan();
std::string parse_otpauth_uri(const std::string& uri);
```

## 3. 从 `cpp` 中识别出的关键内部实现点（UI 设计时会用到）

1. `crypto.cpp`
   1. 文件格式：`MAGIC_HEADER("PWDG\\x01") + salt + nonce + ciphertext`
   2. 加密算法：`XChaCha20-Poly1305`
   3. KDF：`Argon2id (crypto_pwhash_ALG_ARGON2ID13)`
   4. 主密钥与 salt 在进程内缓存，退出/锁库时应调用 `clear_master_key()`
2. `storage.cpp`
   1. 数据文件：`secrets.enc`
   2. 采用临时文件 + rename 覆盖，降低写入损坏风险
3. `2fa.cpp`
   1. 手动、屏幕截图、摄像头三种 secret 导入方式
   2. `setup_from_*` 与 `parse_otpauth_uri` 失败时会抛异常（UI 需捕获并提示）
4. `passwordguard.cpp`
   1. 当前主流程已经包含：初始化 libsodium -> 输入主密码 -> 强度检查 -> 提取 salt -> 初始化主密钥

## 4. 外部库 API（ImGui 集成时可能直接或间接依赖）

### 4.1 libsodium（`crypto.cpp`, `password_input.cpp`, `passwordguard.cpp`）

```cpp
sodium_init();
randombytes_buf(...);
crypto_pwhash(..., crypto_pwhash_ALG_ARGON2ID13);
crypto_aead_xchacha20poly1305_ietf_encrypt(...);
crypto_aead_xchacha20poly1305_ietf_decrypt(...);
sodium_memzero(...);
```

### 4.2 OpenSSL（`2fa.cpp`）

```cpp
HMAC(EVP_sha1(), ...);
```

### 4.3 ZXing（`2fa.cpp`）

```cpp
ZXing::ImageView(...);
ZXing::ReaderOptions options;
options.setFormats(ZXing::BarcodeFormat::QRCode);
ZXing::ReadBarcode(image, options);
```

### 4.4 OpenCV（`2fa.cpp`）

```cpp
cv::VideoCapture cap(0);
cap.isOpened();
cap >> frame;
cv::cvtColor(...);
cv::destroyAllWindows();
```

### 4.5 Win32 GDI（仅 Windows，`2fa.cpp`）

```cpp
GetSystemMetrics(...);
GetDC(...);
CreateCompatibleDC(...);
CreateCompatibleBitmap(...);
BitBlt(...);
GetDIBits(...);
DeleteDC(...);
ReleaseDC(...);
DeleteObject(...);
```

## 5. ImGui 层建议调用流程（可直接按这个流程接线）

```cpp
// App 启动
init_crypto_env();

// 登录窗口
SecureString pwd = /* ImGui 输入框内容 */;
auto salt = extract_salt_from_db();
init_master_key(pwd, salt);

// 主界面加载
std::vector<Credential> entries;
load_entries(entries);

// 新增/编辑条目后
save_entries(entries);

// 查找/删除
find_entry(entries, site);
delete_entry(entries, site);

// 2FA
setup_from_manual(secret);
setup_from_screen_scan();
setup_from_camera_scan();
verify_totp(secret, userCode);

// 锁屏/退出
clear_master_key();
```

## 6. 建议你在 ImGui 层封装的接口（便于后续维护）

```cpp
bool ui_login_with_master_password(const std::string& input_pwd);
bool ui_load_vault(std::vector<Credential>& entries);
bool ui_add_or_update_entry(std::vector<Credential>& entries, const Credential& c);
bool ui_remove_entry(std::vector<Credential>& entries, const std::string& site);
bool ui_bind_2fa_secret(Credential& c, const std::string& method); // manual/screen/camera
bool ui_verify_2fa_code(const Credential& c, const std::string& code);
void ui_lock_vault();
```

以上接口是 UI 层包装建议，底层调用仍对应本项目现有 API，不影响现有加密逻辑。
