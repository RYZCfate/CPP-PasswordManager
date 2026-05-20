#pragma once
#include <vector>
#include "core/storage.hpp"

enum class AppState {
    Login,
    Dashboard,
    AddEntry,
    TwoFactorAuth
};

struct GlobalState {
    AppState currentState = AppState::Login;
    std::vector<Credential> entries;
    Credential* selectedCredential = nullptr;
};

extern GlobalState g_state;
