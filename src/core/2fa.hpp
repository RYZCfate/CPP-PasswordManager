#pragma once
#include <string>
#include <stdexcept>
namespace TwoFactorAuth {

    // ---------------------------------------------------------
    // 核心 TOTP 功能
    // ---------------------------------------------------------

    // 验证用户输入的 6 位动态码是否正确
    // 参数: secret_base32 (Base32格式的密钥), user_code (用户输入的6位数字)
    // 返回: 验证成功返回 true，否则返回 false
    bool verify_totp(const std::string& secret_base32, const std::string& user_code);

    // 生成当前的 6 位动态码 (主要用于测试或展示)
    std::string generate_totp(const std::string& secret_base32);

    // ---------------------------------------------------------
    // 三种获取 2FA 密钥 (Secret) 的方式
    // ---------------------------------------------------------

    // 方式 1: 手动输入
    // 验证手动输入的 Base32 密钥格式是否合法，并进行清理(如去除空格)
    std::string setup_from_manual(const std::string& input_secret);

    // 方式 2: 屏幕截图扫描
    // 截取当前主屏幕，查找二维码并解析出 2FA Secret
    // 返回: 成功解析出的 Base32 Secret。如果未找到二维码或解析失败，抛出异常。
    std::string setup_from_screen_scan();

    // 方式 3: 摄像头扫描
    // 打开默认摄像头，持续扫描直到发现二维码或超时/取消
    // 返回: 成功解析出的 Base32 Secret。如果失败或取消，抛出异常。
    std::string setup_from_camera_scan();

    // ---------------------------------------------------------
    // 辅助工具
    // ---------------------------------------------------------

    // 从 otpauth:// URI 中提取 secret
    // 二维码扫描出的通常是完整的 URI，需要提取其中的 secret 参数
    std::string parse_otpauth_uri(const std::string& uri);
}
