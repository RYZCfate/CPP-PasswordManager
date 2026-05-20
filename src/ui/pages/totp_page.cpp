#include "totp_page.hpp"
#include "imgui.h"
#include "../../app/app_state.hpp"
#include "../../core/2fa.hpp"
#include <chrono>

void RenderTOTPPage() {
    ImGui::Begin("Two-Factor Authentication", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    if (g_state.selectedCredential) {
        ImGui::TextDisabled("SERVICE");
        ImGui::Text("%s", g_state.selectedCredential->site.c_str());
        ImGui::TextDisabled("ACCOUNT");
        ImGui::Text("%s", g_state.selectedCredential->username.c_str());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (!g_state.selectedCredential->totp_secret.empty()) {
            try {
                std::string secret(g_state.selectedCredential->totp_secret.data(), g_state.selectedCredential->totp_secret.length());
                std::string code = TwoFactorAuth::generate_totp(secret);
                
                // Calculate time remaining in 30s window
                auto now = std::chrono::system_clock::now();
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                float remaining = 30.0f - (seconds % 30);
                float progress = remaining / 30.0f;

                ImGui::TextDisabled("CURRENT CODE");
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Assume first font is default
                ImGui::SetWindowFontScale(2.5f);
                ImGui::Text("%s", code.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::PopFont();

                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progress > 0.2f ? ImVec4(0.3f, 0.7f, 0.9f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 10), "");
                ImGui::PopStyleColor();
                ImGui::TextDisabled("Refreshing in %.0f seconds...", remaining);

                ImGui::Spacing();
                if (ImGui::Button("Copy Code", ImVec2(-FLT_MIN, 40))) {
                    ImGui::SetClipboardText(code.c_str());
                }
            } catch (const std::exception& e) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: Invalid TOTP Secret");
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No 2FA secret configured.");
        }
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Back to Vault", ImVec2(-FLT_MIN, 35))) {
        g_state.currentState = AppState::Dashboard;
    }
    ImGui::End();
}
