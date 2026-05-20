#include "crypto.hpp"
#include "storage.hpp"
#include "password_input.hpp"
#include "security.hpp"
#include <iostream>
#include <exception>
#include <sodium.h>

// Forward declaration of the GUI entry point defined in gui.cpp
int run_passwordguard_gui();

namespace
{
    int run_cli_mode()
    {
        try
        {
            // 1. 初始化 libsodium 加密环境
            if (!init_crypto_env())
            {
                std::cerr << "[错误] 无法初始化底层的加密模块 (libsodium)。\n";
                return 1;
            }

            // 2. 隐式要求输入主密码
            SecureString masterPassword = prompt_for_password("Master Password: ");

            // 由于安全策略被我们拆分独立了，这里判断一下（主密码需要校验弱口令提醒）
            std::string plain_pwd(masterPassword.data(), masterPassword.length());
            if (!is_strong_password(plain_pwd))
            {
                std::cout << "[警告] 密码较弱！建议使用至少8个字符，包含字母和数字。\n";
            }
            // 清理刚刚转换出来的临时普通字符串内存
            sodium_memzero(plain_pwd.data(), plain_pwd.size());

            // 3. 提取现存的 Salt，或者触发生成全新 Salt
            auto salt = extract_salt_from_db();

            // 4. 派生主密钥
            // 【重要安全升级】：这一步之后 masterPassword 里面的密码内容会被立刻擦除掉！
            if (!init_master_key(masterPassword, salt))
            {
                std::cerr << "[错误] 密钥生成失败。\n";
                return 1;
            }

            // 5. 进入命令行交互主循环
        }
        catch (const std::exception &e)
        {
            std::cerr << "[致命错误] 程序抛出异常: " << e.what() << '\n';
            return 1;
        }
        catch (...)
        {
            std::cerr << "[致命错误] 发生未知异常。\n";
            return 1;
        }

        return 0;
    }
} // namespace

int main(int argc, char *argv[])
{
#ifdef _WIN32
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
    return run_passwordguard_gui();
#else
    return run_cli_mode();
#endif
}
