#include "add_entry_page.hpp"
#include "imgui.h"
#include "../../app/app_state.hpp"
#include "../../core/storage.hpp"
#include "../../core/2fa.hpp"
#include "../../core/security.hpp"
#include <cstring>

void RenderAddEntryPage() {
    ImGui::Begin("Add Credential", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    static char site[128] = "";
    static char user[128] = "";
    static char pass[128] = "";
    static char totp[128] = "";
    static std::string error_msg = "";
    static std::string verify_code = "";

    ImGui::TextDisabled("IDENTIFICATION");
    ImGui::InputTextWithHint("Site Name", "e.g. GitHub", site, 128);
    ImGui::InputTextWithHint("Username", "e.g. user@example.com", user, 128);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextDisabled("SECURITY");
    
    // Password with strength indicator
    ImGui::InputText("Password", pass, 128, ImGuiInputTextFlags_Password);
    if (pass[0] != '\0') {
        bool strong = is_strong_password(pass);
        float strength = strong ? 1.0f : 0.4f;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, strong ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::ProgressBar(strength, ImVec2(-FLT_MIN, 5), "");
        ImGui::PopStyleColor();
        ImGui::TextColored(strong ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.8f, 0.3f, 0.3f, 1.0f), 
                           strong ? "Strong Password" : "Weak Password (min 8 chars, mix letters/digits)");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("TWO-FACTOR AUTH (OPTIONAL)");
    ImGui::InputText("TOTP Secret", totp, 128);
    
    // Bridge to Core 2FA Scanning
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
    if (ImGui::Button("Scan Screen")) {
        try {
            std::string secret = TwoFactorAuth::setup_from_screen_scan();
            strncpy(totp, secret.c_str(), 127);
            error_msg = "";
            verify_code = "";
        } catch (const std::exception& e) {
            error_msg = e.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Scan Camera")) {
        try {
            std::string secret = TwoFactorAuth::setup_from_camera_scan();
            strncpy(totp, secret.c_str(), 127);
            error_msg = "";
            verify_code = "";
        } catch (const std::exception& e) {
            error_msg = e.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Verify Secret")) {
        if (totp[0] != '\0') {
            try {
                verify_code = TwoFactorAuth::generate_totp(totp);
                error_msg = "";
            } catch (const std::exception& e) {
                error_msg = "Invalid Secret";
                verify_code = "";
            }
        }
    }
    ImGui::PopStyleVar();

    if (!verify_code.empty()) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Generated Code: %s", verify_code.c_str());
    }

    if (!error_msg.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: %s", error_msg.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Save Credential", ImVec2(150, 40))) {
        if (site[0] == '\0' || user[0] == '\0' || pass[0] == '\0') {
            error_msg = "Site, Username, and Password are required.";
        } else {
            Credential new_entry;
            new_entry.site = site;
            new_entry.username = user;
            new_entry.password = pass;
            new_entry.totp_secret = totp;
            
            g_state.entries.push_back(new_entry);
            if (save_entries(g_state.entries)) {
                g_state.currentState = AppState::Dashboard;
                // Clear all
                site[0] = user[0] = '\0';
                sodium_memzero(pass, sizeof(pass)); pass[0] = '\0';
                sodium_memzero(totp, sizeof(totp)); totp[0] = '\0';
                error_msg = "";
                verify_code = "";
            } else {
                ImGui::OpenPopup("Save Error");
            }
        }
    }
    
    if (ImGui::BeginPopupModal("Save Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Failed to save to disk.");
        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 40))) {
        g_state.currentState = AppState::Dashboard;
        site[0] = user[0] = '\0';
        sodium_memzero(pass, sizeof(pass)); pass[0] = '\0';
        sodium_memzero(totp, sizeof(totp)); totp[0] = '\0';
        error_msg = "";
        verify_code = "";
    }
    ImGui::End();
}
