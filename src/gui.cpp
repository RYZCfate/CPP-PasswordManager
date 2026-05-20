#include "hello_imgui/hello_imgui.h"

#include "ui/theme/theme.hpp"

#include "ui/pages/login_page.hpp"
#include "ui/pages/dashboard_page.hpp"
#include "ui/pages/add_entry_page.hpp"
#include "ui/pages/totp_page.hpp"

#include "app/app_state.hpp"

void RenderUI()
{
    switch (g_state.currentState)
    {
    case AppState::Login:
        RenderLoginPage();
        break;

    case AppState::Dashboard:
        RenderDashboardPage();
        break;

    case AppState::AddEntry:
        RenderAddEntryPage();
        break;

    case AppState::TwoFactorAuth:
        RenderTOTPPage();
        break;
    }
}

int run_passwordguard_gui()
{
    HelloImGui::RunnerParams params;

    params.appWindowParams.windowGeometry.size =
        {1400, 900};

    params.callbacks.SetupImGuiStyle =
        []()
    {
        Theme::Apply();
    };

    params.callbacks.ShowGui =
        []()
    {
        RenderUI();
    };

    HelloImGui::Run(params);

    return 0;
}