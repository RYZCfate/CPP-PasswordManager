#include "password_input.hpp"
#include <iostream>
#include <sodium.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

// 跨平台读取密码：Windows 使用 _getch，Linux/macOS 关闭终端回显
SecureString prompt_for_password(const std::string& prompt_msg) {
    SecureString pwd;

#if defined(CODESPACES) || defined(VSCODE) || defined(__CODESPACES)
    std::cout << prompt_msg << "(input may be visible in this environment): ";
#else
    std::cout << prompt_msg;
#endif

#ifdef _WIN32
    // Windows：逐字符读取并用 * 回显
    char ch;
    while ((ch = _getch()) != '\r') {
        if (ch == '\b' && !pwd.empty()) {
            pwd.pop_back();
            std::cout << "\b \b";
        }
        else if (ch != '\b' && ch != '\r') {
            pwd.push_back(ch);
            std::cout << '*';
        }
    }
#else
    // *nix：临时关闭终端回显
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::string temp;
    std::getline(std::cin, temp);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    // 拷贝到安全内存
    pwd.assign(temp.data(), temp.size());
    // 尽量清除普通 string 遗留的内存
    if (!temp.empty()) {
        sodium_memzero(temp.data(), temp.size());
    }
    temp.clear();
#endif
    std::cout << std::endl;

    return pwd;
}
