# PasswordGuard 工程化重构实施计划

## 当前架构分析

当前项目是一个可工作的密码管理器原型，具备加密、存储、TOTP、多页面 UI 等功能。但存在以下架构问题：

| 问题 | 现状 | 风险等级 |
|------|------|----------|
| 全局可变状态 | `extern GlobalState g_state` 被所有模块直接读写 | 高 |
| 裸指针生命周期 | `Credential* selectedCredential` 指向 vector 元素 | **严重** |
| 无命名空间隔离 | 所有符号暴露在全局命名空间 | 中 |
| UI 与业务耦合 | 页面直接调用 `save_entries`、`delete_entry` 等 | 高 |
| `static` 缓冲区泄露 | 页面内 `static char pass[128]` 生命周期跨越锁库 | **严重** |
| 明文在 `std::string` 中残留 | `DecryptResult.plain` 是普通 `std::string` | **严重** |
| 剪贴板未清理 | 复制密码后永久留在系统剪贴板 | 高 |
| 硬编码样式值 | 颜色、间距散落在各页面 | 低 |
| Theme 系统空壳 | `theme.hpp/cpp` 仅有 `#pragma once` 和 `#include` | 低 |

---

## User Review Required

> [!IMPORTANT]
> **裸指针替换策略**：当前 `g_state.selectedCredential` 是指向 `std::vector<Credential>` 元素的裸指针。任何 vector 的增删操作都会导致它变成悬空指针（Dangling Pointer），这是 **未定义行为**。计划将其替换为 `std::optional<std::string> selectedSiteId`，通过站点名称进行稳定索引查找。

> [!IMPORTANT]
> **`encrypt` / `decrypt` API 签名变更**：`DecryptResult.plain` 将从 `std::string` 改为 `SecureString`（由 `sodium_malloc` 管理的安全内存）。`encrypt` 的参数也将改为 `const SecureString&`。这会影响 `storage.cpp` 中的调用方式。

> [!WARNING]
> **目录结构重组**：`core/` 下的文件将拆分为 `core/crypto/`、`core/storage/`、`core/totp/` 子目录，`2fa.hpp/cpp` 将重命名为 `totp.hpp/cpp`。新增 `app/services/`、`app/session/`、`ui/components/` 目录。所有 `#include` 路径和 `CMakeLists.txt` 将同步更新。

> [!NOTE]
> **不改变用户交互行为**：所有重构保持页面行为、状态流转、功能结构完全一致。唯一的用户可感知变化是：新 Vault 时增加密码确认输入框、Dashboard 增加 Show/Hide 密码和 Edit 按钮、剪贴板 15 秒后自动清空、5 分钟无操作自动锁库。

---

## 目标目录结构

```
src/
├── core/                          # Domain/Core Layer
│   ├── crypto/
│   │   ├── crypto.hpp
│   │   └── crypto.cpp
│   ├── storage/
│   │   ├── storage.hpp
│   │   └── storage.cpp
│   ├── totp/                      # renamed from 2fa
│   │   ├── totp.hpp
│   │   └── totp.cpp
│   ├── security.hpp
│   ├── security.cpp
│   ├── password_input.hpp
│   └── password_input.cpp
├── app/                           # Application Layer
│   ├── services/
│   │   ├── vault_service.hpp      # [NEW] 业务逻辑封装
│   │   ├── vault_service.cpp
│   │   ├── clipboard_service.hpp  # [NEW] 剪贴板安全管理
│   │   └── clipboard_service.cpp
│   ├── session/
│   │   ├── session_context.hpp    # [NEW] 会话状态
│   │   └── session_context.cpp
│   ├── app_context.hpp            # [NEW] 顶层 Context，替代 GlobalState
│   └── app_context.cpp
├── ui/                            # Presentation Layer
│   ├── pages/
│   │   ├── login_page.hpp
│   │   ├── login_page.cpp
│   │   ├── dashboard_page.hpp
│   │   ├── dashboard_page.cpp
│   │   ├── add_entry_page.hpp
│   │   ├── add_entry_page.cpp
│   │   ├── totp_page.hpp
│   │   └── totp_page.cpp
│   ├── layout/
│   │   ├── sidebar.hpp
│   │   └── sidebar.cpp
│   ├── components/                # [NEW] 可复用 UI 组件
│   │   ├── credential_card.hpp
│   │   ├── credential_card.cpp
│   │   ├── search_bar.hpp
│   │   ├── search_bar.cpp
│   │   └── password_field.hpp
│   │   └── password_field.cpp
│   ├── theme/
│   │   ├── tokens.hpp             # [NEW] Design Tokens
│   │   ├── theme.hpp
│   │   └── theme.cpp
│   └── ui_state.hpp               # [NEW] 所有页面状态的外部化定义
├── gui.hpp
├── gui.cpp
└── main.cpp
```

---

## Proposed Changes

按依赖方向从底层到顶层，分 6 个阶段执行。

---

### Phase 1: Core Layer 重构（基础类型 + 命名空间 + 安全修复）

#### [MODIFY] [crypto.hpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/crypto.hpp) → `src/core/crypto/crypto.hpp`

- 包裹进 `namespace PasswordGuard::Core`。
- `SodiumAllocator`、`SecureString`、`SecureVector` 类型不变，纳入命名空间。
- `DecryptResult.plain` 类型从 `std::string` 改为 `SecureString`。
- `encrypt` 签名改为 `SecureString encrypt(const SecureString& plain)`——返回值也改为 `SecureString` 以保持整个加密管线使用安全内存。
- 新增 `bool is_vault_initialized()` 查询接口（返回 `g_has_master_key` 状态），避免外部直接访问内部状态。

#### [MODIFY] [crypto.cpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/crypto.cpp) → `src/core/crypto/crypto.cpp`

- 包裹进 `namespace PasswordGuard::Core`。
- `encrypt` 内部：输入 `SecureString`，输出 `SecureString`。使用安全分配器构建输出。
- `decrypt` 内部：输出的 `plain` 使用 `SecureString` 构建，中间 `std::vector<unsigned char>` 在使用后 `sodium_memzero`。
- 所有已有安全措施（nonce 擦除、ciphertext 擦除）保持不变。

---

#### [MODIFY] [storage.hpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/storage.hpp) → `src/core/storage/storage.hpp`

- 包裹进 `namespace PasswordGuard::Core`。
- `Credential` 结构体保持使用 `SecureString` 存储 `password` 和 `totp_secret`。
- 移除 `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` 宏。手写 `to_json` / `from_json`，避免 `SecureString` 到 `std::string` 的隐式转换导致明文残留。

#### [MODIFY] [storage.cpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/storage.cpp) → `src/core/storage/storage.cpp`

- 包裹进 `namespace PasswordGuard::Core`。
- 手写 `to_json` 和 `from_json`，在 `from_json` 中直接构造 `SecureString`，在 `to_json` 中从 `SecureString` 提取为 `std::string` 后立即擦除。
- `load_entries`：`result.plain` 现在是 `SecureString`，需要构造临时 `std::string` 给 `json::parse`（因为 nlohmann::json 不支持自定义分配器的 `basic_string` 作为 parse 输入）。解析后立即擦除该临时字符串。
- `save_entries`：`json::dump()` 产生的 `std::string` 在传入 `encrypt` 前先转为 `SecureString`，然后立即擦除原始 `std::string`。

---

#### [MODIFY] [security.hpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/security.hpp) → `src/core/security.hpp`

- 包裹进 `namespace PasswordGuard::Core`。
- `is_strong_password` 签名改为 `bool is_strong_password(std::string_view pwd)`。
- 新增 `PasswordStrength get_password_strength(std::string_view pwd)` 返回枚举 `{Weak, Fair, Strong}`，供 UI 层显示更细致的进度条。

#### [MODIFY] [security.cpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/security.cpp) → `src/core/security.cpp`

- 包裹进 `namespace PasswordGuard::Core`。
- 增强规则：长度 ≥ 8 且含字母+数字 → `Fair`；长度 ≥ 12 且含大小写+数字+特殊字符 → `Strong`；否则 `Weak`。
- `is_strong_password` 返回 `strength >= Fair`。

---

#### [MODIFY] [2fa.hpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/2fa.hpp) → `src/core/totp/totp.hpp`

- 文件重命名 `2fa` → `totp`（避免以数字开头的文件名）。
- 命名空间从 `TwoFactorAuth` 改为 `PasswordGuard::Core::TOTP`。
- 接口签名不变。

#### [MODIFY] [2fa.cpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/2fa.cpp) → `src/core/totp/totp.cpp`

- 命名空间改为 `PasswordGuard::Core::TOTP`。
- 实现逻辑完全保留不变。

---

#### [MODIFY] [password_input.hpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/password_input.hpp) → `src/core/password_input.hpp`

- 包裹进 `namespace PasswordGuard::Core`。

#### [MODIFY] [password_input.cpp](file:///c:/Users/RYZC/Desktop/code/oro/src/core/password_input.cpp) → `src/core/password_input.cpp`

- 包裹进 `namespace PasswordGuard::Core`。

---

### Phase 2: Application Layer（Context + Services）

#### [NEW] `src/app/app_context.hpp`

```cpp
namespace PasswordGuard::App {

enum class PageId {
    Login,
    Dashboard,
    AddEntry,
    TwoFactorAuth
};

struct AppContext {
    PageId currentPage = PageId::Login;
    SessionContext session;       // 会话状态（entries、选中项等）
    ClipboardState clipboard;     // 剪贴板追踪
    UIState ui;                   // 所有页面的 UI 状态
    double lastActivityTime = 0.0; // 用于自动锁定
    
    // 重置所有状态（锁库时调用）
    void reset();
};

} // namespace PasswordGuard::App
```

#### [NEW] `src/app/app_context.cpp`
- 实现 `AppContext::reset()` —— 清空 entries、擦除所有 UI 缓冲区、重置页面状态。

---

#### [NEW] `src/app/session/session_context.hpp`

```cpp
namespace PasswordGuard::App {

struct SessionContext {
    std::vector<PasswordGuard::Core::Credential> entries;
    std::optional<std::string> selectedSiteId;  // 替代 Credential* 裸指针
    bool isEditMode = false;
    bool isVaultUnlocked = false;
    
    // 根据 selectedSiteId 查找 Credential，返回指针（仅在当前帧使用）
    Core::Credential* getSelectedCredential();
    const Core::Credential* getSelectedCredential() const;
};

} // namespace PasswordGuard::App
```

#### [NEW] `src/app/session/session_context.cpp`
- `getSelectedCredential()` 通过 `std::ranges::find_if` 按 site 名查找，找不到返回 `nullptr`。每帧重新查找，不缓存指针。

---

#### [NEW] `src/app/services/vault_service.hpp`

```cpp
namespace PasswordGuard::App {

// 封装所有 Vault 业务操作，UI 层不直接接触 Core API
class VaultService {
public:
    explicit VaultService(SessionContext& session);
    
    bool isNewVault() const;           // 检测 secrets.enc 是否存在
    bool unlock(Core::SecureString& password);
    void lock();
    
    bool addCredential(Core::Credential entry);
    bool updateCredential(const std::string& site, Core::Credential updated);
    bool deleteCredential(const std::string& site);
    Core::Credential* findBySite(const std::string& site);
    
    bool save();                       // 加密写盘
    
private:
    SessionContext& session_;
};

} // namespace PasswordGuard::App
```

#### [NEW] `src/app/services/vault_service.cpp`
- `unlock()` 内部调用 `Core::extract_salt_from_db()` + `Core::init_master_key()` + `Core::load_entries()`。
- `lock()` 内部调用 `Core::clear_master_key()`，擦除 `session_.entries`，重置所有状态。
- `addCredential()` 检查是否已存在同名站点，push_back 后调用 `save()`。
- `updateCredential()` 按 site 查找并覆盖字段。
- `deleteCredential()` 调用 `Core::delete_entry()` 后 `save()`。

---

#### [NEW] `src/app/services/clipboard_service.hpp`

```cpp
namespace PasswordGuard::App {

struct ClipboardState {
    bool hasPendingClear = false;
    double copyTimestamp = 0.0;
    std::string copiedContentHash;  // 不存原始密码，存 hash 用于比对
    
    static constexpr double kClearTimeoutSeconds = 15.0;
};

class ClipboardService {
public:
    static void copyToClipboard(const std::string& text, ClipboardState& state, double currentTime);
    static void updateAutoClean(ClipboardState& state, double currentTime);
};

} // namespace PasswordGuard::App
```

#### [NEW] `src/app/services/clipboard_service.cpp`
- `copyToClipboard()` 调用 `ImGui::SetClipboardText()`，记录时间戳，计算内容 hash。
- `updateAutoClean()` 检查是否超过 15 秒，如果是则 `ImGui::SetClipboardText("")` 并重置状态。

---

### Phase 3: UI State 外部化 + Theme Tokens

#### [NEW] `src/ui/ui_state.hpp`

将所有页面的 `static` 缓冲区集中管理：

```cpp
namespace PasswordGuard::UI {

struct LoginPageState {
    char passwordBuf[128] = {};
    char confirmBuf[128] = {};
    std::string errorMessage;
    
    void wipeSensitive();  // sodium_memzero 所有缓冲区
};

struct AddEntryPageState {
    char siteBuf[128] = {};
    char userBuf[128] = {};
    char passBuf[128] = {};
    char totpBuf[128] = {};
    std::string errorMsg;
    std::string verifyCode;
    
    void wipeSensitive();
    void clear();  // wipeSensitive + 清空非敏感字段
};

struct DashboardPageState {
    char searchBuf[128] = {};
    std::unordered_set<std::string> revealedSites;  // Show/Hide 切换追踪
    std::string lastCopiedLabel;
    float copyFeedbackTimer = 0.0f;
    
    void clearRevealed();  // 锁库时清空
};

struct TOTPPageState {
    // 当前无需持久状态，TOTP 每帧重新生成
};

struct UIState {
    LoginPageState login;
    AddEntryPageState addEntry;
    DashboardPageState dashboard;
    TOTPPageState totp;
    
    void wipeAll();  // 锁库时调用
};

} // namespace PasswordGuard::UI
```

---

#### [NEW] `src/ui/theme/tokens.hpp`

```cpp
namespace PasswordGuard::UI::Tokens {

// ── Color Palette ──
constexpr ImVec4 Success       = {0.3f, 0.8f, 0.3f, 1.0f};
constexpr ImVec4 Danger        = {1.0f, 0.4f, 0.4f, 1.0f};
constexpr ImVec4 DangerBg      = {0.7f, 0.1f, 0.1f, 1.0f};
constexpr ImVec4 DangerHover   = {0.9f, 0.2f, 0.2f, 1.0f};
constexpr ImVec4 Info          = {0.3f, 0.7f, 0.9f, 1.0f};
constexpr ImVec4 WarningText   = {0.8f, 0.3f, 0.3f, 1.0f};

// ── Layout ──
constexpr float SidebarWidth       = 180.0f;
constexpr float CardHeight         = 80.0f;
constexpr float ButtonHeightSmall  = 30.0f;
constexpr float ButtonHeightMedium = 35.0f;
constexpr float ButtonHeightLarge  = 40.0f;
constexpr float IconRounding       = 20.0f;
constexpr float ProgressBarHeight  = 5.0f;
constexpr float SearchBarWidth     = 300.0f;

// ── Timing ──
constexpr float CopyFeedbackDuration = 2.0f;
constexpr double AutoLockTimeout     = 300.0;  // 5 分钟
constexpr double ClipboardTimeout    = 15.0;   // 15 秒

} // namespace PasswordGuard::UI::Tokens
```

---

#### [MODIFY] `src/ui/theme/theme.hpp` + `theme.cpp`

- 包裹进 `namespace PasswordGuard::UI::Theme`。
- 实现 `void Apply()` 函数，设置 ImGui 全局样式（暗色主题、字体、圆角、间距等），统一引用 `Tokens` 中的常量。

---

### Phase 4: UI Components 组件化

#### [NEW] `src/ui/components/credential_card.hpp` + `.cpp`

```cpp
namespace PasswordGuard::UI::Components {

struct CredentialCardAction {
    enum Type { None, Copy, Edit, Delete, View2FA, ToggleReveal };
    Type type = None;
};

// 渲染单条凭据卡片，返回用户执行的操作
CredentialCardAction RenderCredentialCard(
    const Core::Credential& entry,
    int index,
    bool isRevealed
);

} // namespace PasswordGuard::UI::Components
```

- 从 `dashboard_page.cpp` 提取卡片渲染逻辑。
- 使用 `ImGui::PushID(entry.site.c_str())` 作为稳定 ID（基于站点名称，非索引）。
- 返回 Action 枚举，由 Dashboard 页面处理后续操作。

#### [NEW] `src/ui/components/search_bar.hpp` + `.cpp`

```cpp
namespace PasswordGuard::UI::Components {
bool RenderSearchBar(char* buf, size_t bufSize, float width);
}
```

- 从 `dashboard_page.cpp` 提取搜索框 + Clear 按钮。

#### [NEW] `src/ui/components/password_field.hpp` + `.cpp`

```cpp
namespace PasswordGuard::UI::Components {

// 渲染带强度指示器的密码输入框
bool RenderPasswordField(
    const char* label, 
    char* buf, 
    size_t bufSize,
    bool showStrengthBar = true
);

} // namespace PasswordGuard::UI::Components
```

- 从 `add_entry_page.cpp` 提取密码输入 + 强度进度条。
- 内部调用 `Core::get_password_strength()`。

---

### Phase 5: Pages 重构（使用 Context + Services + Components）

所有页面渲染函数签名改为接收 `AppContext&` 和 `VaultService&` 引用：

```cpp
// 统一签名模式
void RenderLoginPage(App::AppContext& ctx, App::VaultService& vault);
void RenderDashboardPage(App::AppContext& ctx, App::VaultService& vault);
void RenderAddEntryPage(App::AppContext& ctx, App::VaultService& vault);
void RenderTOTPPage(App::AppContext& ctx, App::VaultService& vault);
void RenderSidebar(App::AppContext& ctx, App::VaultService& vault);
```

#### [MODIFY] `src/ui/pages/login_page.cpp`

- 移除文件级 `static` 变量，改用 `ctx.ui.login` 中的状态。
- 通过 `vault.isNewVault()` 检测是否为新 Vault：
  - 新 Vault：显示 "Create New Vault" 标题、两次密码输入、强度校验。
  - 已有 Vault：显示 "Unlock Vault" 标题、单密码输入。
- 调用 `vault.unlock()` 替代直接调用 `init_master_key` + `load_entries`。
- 解锁/创建完成后调用 `ctx.ui.login.wipeSensitive()`。

#### [MODIFY] `src/ui/pages/dashboard_page.cpp`

- 移除所有文件级 `static` 变量，改用 `ctx.ui.dashboard`。
- 使用 `Components::RenderSearchBar()` 替代内联搜索框。
- 使用 `Components::RenderCredentialCard()` 替代内联卡片渲染。
- 根据 `CredentialCardAction` 处理 Copy / Edit / Delete / ToggleReveal / View2FA：
  - **Copy**：调用 `ClipboardService::copyToClipboard()`。
  - **Edit**：设置 `ctx.session.selectedSiteId` 和 `ctx.session.isEditMode = true`，切换到 `AddEntry` 页面。
  - **Delete**：通过确认弹窗后调用 `vault.deleteCredential()`。
  - **ToggleReveal**：在 `ctx.ui.dashboard.revealedSites` 中添加/移除站点名。
  - **View2FA**：设置 `ctx.session.selectedSiteId`，切换到 `TwoFactorAuth` 页面。

#### [MODIFY] `src/ui/pages/add_entry_page.cpp`

- 移除文件级 `static` 变量，改用 `ctx.ui.addEntry`。
- 进入页面时，如果 `ctx.session.isEditMode`，从 `vault.findBySite(selectedSiteId)` 预填数据，Site Name 设为只读。
- 保存时根据 `isEditMode` 调用 `vault.addCredential()` 或 `vault.updateCredential()`。
- 离开页面（Save / Cancel / 导航切换）时调用 `ctx.ui.addEntry.wipeSensitive()`。

#### [MODIFY] `src/ui/pages/totp_page.cpp`

- 使用 `ctx.session.getSelectedCredential()` 替代 `g_state.selectedCredential`。
- 命名空间引用 `Core::TOTP::generate_totp()`。
- Copy Code 调用 `ClipboardService::copyToClipboard()`。

#### [MODIFY] `src/ui/layout/sidebar.cpp`

- Lock Vault：调用 `vault.lock()` + `ctx.reset()`（内部擦除所有 UI 缓冲区）。
- 导航切换时，如果从 `AddEntry` 离开，调用 `ctx.ui.addEntry.wipeSensitive()`。

---

### Phase 6: 顶层入口 + 自动锁定 + CMake

#### [MODIFY] `src/gui.cpp`

- 创建 `AppContext` 和 `VaultService` 实例，并传入各渲染函数。
- 在 `RenderUI()` 中增加：
  - **自动锁定逻辑**：检测 ImGui IO 的键盘/鼠标事件更新 `ctx.lastActivityTime`。超过 `Tokens::AutoLockTimeout` 则触发 `vault.lock()` + `ctx.reset()`。
  - **剪贴板自动清理**：每帧调用 `ClipboardService::updateAutoClean()`。

#### [MODIFY] `src/main.cpp`

- 移除 CLI 模式代码中对旧全局符号的引用（如果保留 CLI 模式的话）。
- 移除 `main.cpp` 中的密码到 `std::string` 的拷贝（行 29：`std::string plain_pwd(...)`），改为直接使用 `string_view`。

#### [MODIFY] [CMakeLists.txt](file:///c:/Users/RYZC/Desktop/code/oro/CMakeLists.txt)

- 更新 `SOURCES` 列表以反映新的文件路径和新增文件。
- 更新 `target_include_directories` 以包含 `src`、`src/core`、`src/core/crypto`、`src/core/storage`、`src/core/totp`。

#### [DELETE] `src/app/app_state.hpp` + `src/app/app_state.cpp`

- 被 `app_context.hpp` + `session_context.hpp` + `ui_state.hpp` 完全替代。

---

## 安全改进汇总

| # | 修复项 | 文件 | 严重性 |
|---|--------|------|--------|
| 1 | `DecryptResult.plain` 改为 `SecureString` | `crypto.hpp/cpp` | 严重 |
| 2 | `encrypt` 参数改为 `SecureString` | `crypto.hpp/cpp` | 严重 |
| 3 | JSON 序列化中间字符串擦除 | `storage.cpp` | 严重 |
| 4 | 消除 `Credential*` 悬空指针 | `app_state.hpp` → `session_context.hpp` | 严重 |
| 5 | 页面 `static char[]` 缓冲区集中管理 + 擦除 | `ui_state.hpp` + 各页面 | 严重 |
| 6 | 手写 JSON `to_json`/`from_json` 避免隐式拷贝 | `storage.hpp` | 高 |
| 7 | 剪贴板 15 秒自动清空 | `clipboard_service` | 高 |
| 8 | 5 分钟无操作自动锁库 | `gui.cpp` | 高 |
| 9 | 新 Vault 密码确认 + 强度校验 | `login_page.cpp` | 中 |
| 10 | `is_strong_password` 改为 `string_view` 避免拷贝 | `security.hpp/cpp` | 中 |
| 11 | Lock Vault 时全量擦除所有 UI 状态 | `sidebar.cpp` + `AppContext::reset()` | 高 |

---

## Verification Plan

### 编译验证
- 更新 `CMakeLists.txt` 后执行 CMake 配置 + 构建，确保零错误零警告。
- 检查所有 `#include` 路径正确，无循环依赖。

### 功能自检
1. **新建 Vault**：移除 `secrets.enc`，启动程序，验证密码确认流程。
2. **解锁 Vault**：正确/错误密码解锁测试。
3. **增删改查**：Add → Dashboard 列表验证 → Edit 修改 → Delete 确认弹窗。
4. **2FA 流程**：从 Dashboard 进入 TOTP 页面，验证 Code 生成和倒计时。
5. **Show/Hide 密码**：Dashboard 中切换显示/隐藏。
6. **剪贴板清理**：复制密码 → 等待 15 秒 → 验证剪贴板已清空。
7. **自动锁定**：静置 5 分钟 → 验证自动回到登录页。
8. **Lock Vault**：手动锁库 → 验证所有状态清空。

### 安全自检
- 确认所有 `char[]` 缓冲区在页面离开时被 `sodium_memzero`。
- 确认 `DecryptResult.plain` 使用 `SecureString`（sodium 管理内存）。
- 确认无 `std::string` 长期持有解密后的明文。
- 确认 `selectedSiteId` 替代了所有裸指针。
- 确认无遗漏的敏感数据日志输出（`std::cout`/`std::cerr` 不输出密码或密钥）。
