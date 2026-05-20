# 项目代码审计报告 (oro 密码管理器)

## 1. UI 调用接口与 Core 接口一致性及逻辑缺陷

### 1.1 缺失的生命周期管理
- **Vault 锁定缺失**: 目前 UI (gui.cpp) 缺乏“锁定保险库”的功能。虽然 `crypto.hpp` 提供了 `clear_master_key()`，但 UI 中没有任何按钮或自动超时机制来调用它。这意味着一旦解锁，主密钥将永久驻留在内存中直到程序关闭。
- **登录逻辑缺陷**: `login_page.cpp` 在调用 `init_master_key` 后，虽然切换到了 `Dashboard`，但没有调用 `init_crypto_env()`。根据 `main.cpp` 的逻辑，如果不先初始化 sodium 环境，加密功能可能无法正常工作。

### 1.2 数据持久化与同步
- **删除操作不一致**: `dashboard_page.cpp` 的删除操作已初步修复（加入了 `save_entries`），但这种模式会导致每次删除都进行一次完整的磁盘写操作。在大数据量下，UI 可能会出现卡顿，且缺乏撤销（Undo）机制。

## 2. 严重的数据安全性漏洞 (UI & Core 交互)

### 2.1 内存明文泄露 (Critical)
- **静态缓冲区泄露**: `login_page.cpp` 和 `add_entry_page.cpp` 使用 `static char password[128]` 存储密码。
    - **漏洞**: `static` 变量分配在数据段（Data Segment），其生命周期贯穿程序始终。即使切换了页面，密码明文仍永久保留在内存中。
    - **风险**: 攻击者通过内存转储（Memory Dump）可以轻易获取主密码和站点密码。
- **std::string 的滥用**: `SecureString` 目前只是 `std::string` 的别名。
    - **漏洞**: `std::string` 不保证在析构时擦除内存，且可能因为扩容（Reallocation）在堆内存的不同位置留下多个明文副本。
    - **风险**: `Credential` 结构体在 `g_state.entries` 中长期持有所有站点的明文密码，且未受 `sodium_mlock` 保护。

### 2.2 UI 层面的泄露风险
- **密码回显漏洞**: `add_entry_page.cpp` 中的 `ImGui::InputText("Password", ...)` 未设置 `ImGuiInputTextFlags_Password` 标志，导致用户在输入站点密码时直接在屏幕上以明文显示。
- **剪贴板风险**: `dashboard_page.cpp` 仍使用系统剪贴板传递明文密码。
    - **漏洞**: 剪贴板内容可被系统中任何具有读取权限的第三方程序监控。
    - **风险**: 密码一旦复制，其生命周期便脱离了本程序的安全控制。

### 2.3 核心接口调用不当
- **敏感数据转换**: `login_page.cpp` 中 `SecureString masterPassword = password;` 发生了一次从 C 风格字符串到 `std::string` 的拷贝。
    - **风险**: 这在内存中创建了主密码的新副本，而旧的 `password` 缓冲区未被擦除。
- **错误处理不足**: `add_entry_page.cpp` 在保存新条目时未检查 `save_entries` 的返回值。如果磁盘空间不足或权限受限，用户会以为保存成功，但实际数据已丢失。

## 3. 改进建议
- **使用安全分配器**: 将 `SecureString` 更改为使用 `sodium_malloc` 的自定义容器，确保内存被锁定（mlock）且在释放时自动擦除（memzero）。
- **立即擦除缓冲区**: UI 层面的 `char[]` 缓冲区在点击“保存”或“解锁”后，应立即调用 `sodium_memzero`。
- **引入掩码显示**: 在 UI 列表界面，默认应隐藏密码，仅在用户明确点击“显示”时才通过掩码控件呈现。
- **增强剪贴板安全**: 引入计时器功能，在复制密码 30 秒后自动清空剪贴板。

