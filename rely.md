对当前 PasswordGuard 项目进行工程化重构，重点提升架构一致性、可维护性、长期扩展能力以及敏感数据安全性。

核心 C++ 代码位于 core 文件夹中。

项目基于：

* C++
* Dear ImGui
* HelloImGui

当前已经具备：

* 多页面 UI
* Theme 系统
* Sidebar Layout
* App State
* Credential 管理

本次任务目标不是增加功能，而是进行：

* 工程结构优化
* 架构层次整理
* 数据安全性审查
* 敏感信息生命周期优化
* 长期维护能力提升

优先保持现有功能行为一致。

除非存在明确安全风险或严重架构问题，否则不要随意改变：

* 用户交互逻辑
* 页面行为
* 状态流转
* 现有功能结构

避免过度工程化与无意义抽象。

优先选择：

* 清晰
* 稳定
* 可维护
* 易理解

的结构。

不要为了使用设计模式而引入额外复杂性。

优先使用现代 C++17/20 风格：

* RAII
* smart pointer
* enum class
* std::optional
* constexpr
* string_view
* filesystem
* scoped object lifetime

避免：

* 裸 new/delete
* 宏滥用
* C 风格内存管理
* 全局 mutable singleton
* 不必要的 dynamic allocation

保持 Dear ImGui Immediate Mode UI 风格。

不要强行套用传统 retained-mode GUI 架构。

允许：

* 简洁 UI 逻辑
* Immediate Mode 状态驱动

但需要：

* 明确状态边界
* UI 与业务逻辑解耦
* 页面职责清晰
* UI 组件化

请遵循以下要求：

1. 收敛 Global Mutable State

当前项目大量依赖全局状态对象。

需要逐步从：

* Global Shared State

过渡到：

* Context-Based State Management

目标：

* 降低模块耦合
* 消除隐式依赖
* 提高可测试性
* 明确状态所有权

建议建立：

* AppContext
* SessionContext
* Scoped UI State

避免任意模块直接修改全局对象。

2. 建立明确 Layered Architecture

当前 UI 与业务逻辑耦合仍然较高。

需要形成：

Presentation Layer
→ Application Layer
→ Domain/Core Layer
→ Infrastructure Layer

要求：

UI 层仅负责：

* 渲染
* 用户交互
* 发起动作

业务逻辑统一进入：

* Service
* Manager
* Repository
* Command Handler

禁止 UI 直接操作：

* 数据容器
* 存储逻辑
* 加密逻辑
* 核心业务规则

3. 消除容器元素生命周期风险

当前存在：

* vector element reference
* pointer persistence
* iterator invalidation risk

需要统一：

UI 层仅保存：

* ID
* Handle
* UUID
* Stable Index

禁止长期保存：

* vector element pointer
* container reference
* iterator

避免：

* Dangling Pointer
* Undefined Behavior

4. 统一 ImGui ID 策略

当前 ImGui ID 风格不统一。

需要统一采用：

Stable String-Based ID Strategy

避免：

* pointer cast ID
* address-based ID
* unstable index ID

保证：

* UI 状态稳定
* Debug 可读性
* Reorder 安全性
* Component 可维护性

5. 规范 Immediate Mode UI State 管理

当前存在大量：

* local static UI state

需要建立：

Explicit UI State Management

将页面状态：

* 外部化
* 结构化
* 集中管理

形成：

* PageState
* ViewState
* UI Context

避免页面内部 static 状态泛滥。

6. 提升 UI Componentization

当前页面仍偏向：

* Monolithic UI Composition

需要逐步形成：

Component-Oriented UI Architecture

目标包括：

* CredentialCard
* SidebarSection
* ModalComponent
* SearchBar
* FormComponent

等独立可复用组件。

要求：

* UI 组件职责单一
* 页面仅负责布局组合
* 提高复用性
* 提高可维护性

7. 建立 Design Token Theme System

当前 Theme 大量使用：

* hardcoded style values
* magic numbers

需要建立：

Design Token System

包括：

* Color Tokens
* Radius Tokens
* Typography Tokens
* Spacing Tokens

形成：

* Theme Abstraction Layer
* Centralized Style System

避免颜色、间距、圆角散落在 UI 中。

8. 统一 Namespace Architecture

当前 namespace 使用不统一。

需要建立：

Project Namespace Hierarchy

例如：

* PasswordGuard::UI
* PasswordGuard::Core
* PasswordGuard::App
* PasswordGuard::Storage

目标：

* 消除 symbol pollution
* 明确模块边界
* 提高大型项目可维护性

9. 保持 Platform Isolation

当前项目使用：

* HelloImGui
* GLFW backend

禁止继续引入：

* Win32 WndProc Hook
* HWND lifecycle manipulation
* platform-specific window management

要求：

坚持：

* Platform Abstraction Boundary
* Framework Ownership Principle

窗口生命周期、事件循环、平台交互应由：

* HelloImGui
* GLFW backend

自身管理。

应用层仅处理：

* Business UI
* App Logic

10. 目录结构工程化

目标结构：

core/
crypto/
storage/
totp/

app/
services/
session/
commands/

ui/
pages/
layout/
components/
theme/

要求：

* 模块职责清晰
* include 依赖方向明确
* 避免循环依赖
* 减少跨层访问

11. 代码风格统一

统一以下工程规范：

* include 顺序规范化
* namespace 风格统一
* 文件命名统一
* ImGui ID 风格统一
* Component 命名统一
* 禁止魔法数字散落
* 减少超长 UI 函数
* 页面与组件职责分离

12. 重点进行敏感数据安全审查

PasswordGuard 属于密码管理器项目。

重点审查以下敏感数据生命周期：

* master password
* decrypted credential
* TOTP secret
* clipboard password data
* session key
* temporary decrypted buffers

避免：

* 明文长期驻留内存
* std::string 不必要复制
* secret 数据日志泄露
* UI state 持久化泄露
* 调试输出泄露
* 未清理缓冲区
* unsafe serialization

必要时引入：

* secure memory wipe
* zeroization
* scoped secret container
* secure allocator
* clipboard timeout cleanup

同时检查：

* 异常路径
* early return
* move/copy 行为
* serialization 边界
* memory ownership

避免敏感数据在异常情况下泄露。

13. 最终验证与交付要求

在完成所有重构后：

* 自我检查是否引入新增 bug
* 自我检查是否引入新的安全漏洞
* 检查原有功能是否被破坏
* 检查 UI 状态行为是否一致
* 检查 include dependency 是否正确
* 检查是否存在循环依赖
* 检查是否存在 dangling reference
* 检查是否存在 memory leak

确保项目：

* 可以正常编译
* CMakeLists 正确更新
* include 路径正确
* 模块依赖正确
* UI 正常运行
* 原有功能完整保留

最终目标：

将项目从：

* Feature-Oriented Toy Project

提升为：

* Maintainable Desktop Application
* Structured C++ Project
* Secure Password Manager Architecture
* Layered UI Architecture
* Resume-Grade Engineering Project
