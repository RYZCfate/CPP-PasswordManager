#include "2fa.hpp"
#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <regex>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <openssl/hmac.h>// 包含 OpenSSL 用于 HMAC-SHA1计算 (请确保项目中链接了 libcrypto)
#include <openssl/evp.h>
#include <ZXing/ReadBarcode.h>// 包含 ZXing 用于二维码解码
#include <ZXing/TextUtfEncoding.h>
#include <opencv2/opencv.hpp> // 包含 OpenCV 用于摄像头调用和图像容器
#ifdef _WIN32 // Windows API 用于屏幕截图
#include <windows.h>
#endif

namespace TwoFactorAuth {

    // --- 内部辅助函数：Base32 解码 ---
    static std::vector<uint8_t> base32_decode(const std::string& base32_str) {
        std::vector<uint8_t> result;
        int buffer = 0;
        int bitsLeft = 0;
        for (char c : base32_str) {
            int val = 0;
            if (c >= 'A' && c <= 'Z') val = c - 'A';
            else if (c >= 'a' && c <= 'z') val = c - 'a';
            else if (c >= '2' && c <= '7') val = c - '2' + 26;
            else if (c == '=' || c == ' ') continue; // 忽略填充和空格
            else throw std::invalid_argument("Invalid Base32 character");

            buffer = (buffer << 5) | val;
            bitsLeft += 5;
            if (bitsLeft >= 8) {
                result.push_back(static_cast<uint8_t>((buffer >> (bitsLeft - 8)) & 0xFF));
                bitsLeft -= 8;
            }
        }
        return result;
    }

    // --- 内部辅助函数：生成 TOTP ---
    std::string generate_totp(const std::string& secret_base32) {
        // 1. 获取当前 Unix 时间戳，除以 30 秒 (TOTP 默认时间步长)
        auto now = std::chrono::system_clock::now();
        uint64_t time_step = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() / 30;

        // 2. 将时间步长转换为 8 字节大端序数组
        uint8_t time_bytes[8];
        for (int i = 7; i >= 0; --i) {
            time_bytes[i] = time_step & 0xFF;
            time_step >>= 8;
        }

        // 3. 解码 Base32 Secret
        std::vector<uint8_t> key = base32_decode(secret_base32);
        if (key.empty()) {
            throw std::invalid_argument("TOTP secret cannot be empty");
        }

        // 4. 计算 HMAC-SHA1
        unsigned int md_len = 0;
        unsigned char hmac_buffer[EVP_MAX_MD_SIZE] = {};
        unsigned char* hmac_result = HMAC(
            EVP_sha1(),
            key.data(),
            static_cast<int>(key.size()),
            time_bytes,
            sizeof(time_bytes),
            hmac_buffer,
            &md_len);
        if (hmac_result == nullptr || md_len < 20) {
            throw std::runtime_error("HMAC-SHA1 computation failed");
        }

        // 5. 动态截断 (Dynamic Truncation) 取出 4 个字节
        int offset = hmac_result[md_len - 1] & 0x0F;
        if (offset + 3 >= static_cast<int>(md_len)) {
            throw std::runtime_error("Invalid HMAC result length");
        }
        uint32_t binary =
            ((hmac_result[offset] & 0x7F) << 24) |
            ((hmac_result[offset + 1] & 0xFF) << 16) |
            ((hmac_result[offset + 2] & 0xFF) << 8) |
            (hmac_result[offset + 3] & 0xFF);

        // 6. 对 10^6 取模得到 6 位数字
        uint32_t otp = binary % 1000000;

        // 7. 格式化为 6 位字符串，不足补零
        std::stringstream ss;
        ss << std::setw(6) << std::setfill('0') << otp;
        return ss.str();
    }

    bool verify_totp(const std::string& secret_base32, const std::string& user_code) {
        if (user_code.length() != 6) return false;
        try {
            // 在实际生产中，为了容错，通常会验证当前时间步长以及前一个和后一个时间步长
            std::string expected_code = generate_totp(secret_base32);
            return expected_code == user_code;
        }
        catch (const std::exception&) {
            return false;
        }
    }

    std::string parse_otpauth_uri(const std::string& uri) {
        // 简单的正则表达式提取 secret 参数
        // 示例: otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example
        std::regex secret_regex("secret=([A-Z2-7a-z]+)");
        std::smatch match;
        if (std::regex_search(uri, match, secret_regex)) {
            return setup_from_manual(match[1].str());
        }
        throw std::runtime_error("URI 中未找到合法的 secret 参数");
    }

    // ---------------------------------------------------------
    // 方式 1: 手动输入
    // ---------------------------------------------------------
    std::string setup_from_manual(const std::string& input_secret) {
        std::string cleaned_secret;
        // 清理空格并转为大写
        for (char c : input_secret) {
            if (c != ' ') {
                cleaned_secret += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
        }
        // 验证是否能成功解码，不能则抛出异常
        base32_decode(cleaned_secret);
        return cleaned_secret;
    }

    // ---------------------------------------------------------
    // 方式 2: 屏幕截图扫描
    // ---------------------------------------------------------
    std::string setup_from_screen_scan() {
#ifdef _WIN32
        // 使用 Windows GDI 进行全屏截图
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);

        HDC hScreen = GetDC(NULL);
        HDC hDC = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
        HGDIOBJ old_obj = SelectObject(hDC, hBitmap);
        BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);

        // 转换为 OpenCV Mat
        cv::Mat src;
        src.create(height, width, CV_8UC4);
        BITMAPINFOHEADER bi = { sizeof(bi), width, -height, 1, 32, BI_RGB, 0, 0, 0, 0, 0 };
        GetDIBits(hDC, hBitmap, 0, height, src.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        // 清理 GDI 资源
        SelectObject(hDC, old_obj);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        DeleteObject(hBitmap);

        // 转换为灰度图供 ZXing 使用
        cv::Mat gray;
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);

        // 使用 ZXing 进行识别
        ZXing::ImageView image(gray.data, gray.cols, gray.rows, ZXing::ImageFormat::Lum);
        ZXing::ReaderOptions options;
        options.setFormats(ZXing::BarcodeFormat::QRCode);

        auto result = ZXing::ReadBarcode(image, options);
        if (result.isValid()) {
            std::string uri = result.text();
            return parse_otpauth_uri(uri);
        }
        else {
            throw std::runtime_error("在屏幕上未检测到有效的二维码。");
        }
#else
        throw std::runtime_error("屏幕截图功能目前仅支持 Windows。");
#endif
    }

    // ---------------------------------------------------------
    // 方式 3: 摄像头扫描
    // ---------------------------------------------------------
    std::string setup_from_camera_scan() {
        cv::VideoCapture cap(0); // 打开默认摄像头
        if (!cap.isOpened()) {
            throw std::runtime_error("无法打开摄像头。请检查权限或设备连接。");
        }

        cv::Mat frame, gray;
        ZXing::ReaderOptions options;
        options.setFormats(ZXing::BarcodeFormat::QRCode);

        std::string found_secret = "";

        // 此处可以加入一个 UI 窗口显示摄像头画面，按 ESC 退出
        // 为了方便集成到你的 GUI 框架（比如 ImGui 或 Qt），这里做了一个简单的循环
        // 在实际应用中，你可能需要将这一部分提取为异步任务或绑定到界面的视频流组件
        int max_attempts = 100; // 设置超时机制
        while (max_attempts-- > 0) {
            cap >> frame;
            if (frame.empty()) continue;

            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
            ZXing::ImageView image(gray.data, gray.cols, gray.rows, ZXing::ImageFormat::Lum);

            auto result = ZXing::ReadBarcode(image, options);
            if (result.isValid()) {
                std::string uri = result.text();
                found_secret = parse_otpauth_uri(uri);
                break; // 识别成功，跳出循环
            }

            // 可选: 显示摄像头画面 (若在无头环境或自定义GUI中，可注释掉这两行)
            // cv::imshow("2FA QR Scanner - Press ESC to cancel", frame);
            // if (cv::waitKey(30) == 27) break; 
        }

        cap.release();
        cv::destroyAllWindows();

        if (found_secret.empty()) {
            throw std::runtime_error("扫描超时或未检测到二维码。");
        }

        return found_secret;
    }
}
