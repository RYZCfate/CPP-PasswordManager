#include "login_page.hpp"
#include "imgui.h"
#include "../../app/app_state.hpp"
#include "../../core/crypto.hpp"
#include "../../core/storage.hpp"
#include "../../core/password_input.hpp"

void RenderLoginPage() {
    static bool env_init = false;
    if (!env_init) {
        init_crypto_env();
        env_init = true;
    }

    ImGui::Begin("Unlock Vault", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    // UI state for error messaging
    static std::string error_message = "";
    
    // Use static for ImGui persistence across frames, but we MUST zero it after processing.
    static char password_buf[128] = ""; 
    bool trigger_unlock = false;

    ImGui::Text("Please enter your master password to decrypt the vault.");
    ImGui::Spacing();

    if (ImGui::InputText("Master Password", password_buf, IM_ARRAYSIZE(password_buf), 
        ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue)) {
        trigger_unlock = true;
    }
    
    ImGui::Spacing();
    if (ImGui::Button("Unlock", ImVec2(120, 0))) {
        trigger_unlock = true;
    }

    if (!error_message.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", error_message.c_str());
    }

    if (trigger_unlock) {
        SecureString masterPassword = password_buf;
        auto salt = extract_salt_from_db();
        
        if (init_master_key(masterPassword, salt)) {
            if (load_entries(g_state.entries)) {
                g_state.currentState = AppState::Dashboard;
                error_message = "";
            } else {
                error_message = "Failed to decrypt vault. Wrong password?";
            }
        } else {
            error_message = "KDF Initialization failed.";
        }
        
        // CRITICAL: Always wipe the buffer after moving data to SecureString
        sodium_memzero(password_buf, sizeof(password_buf));
        password_buf[0] = '\0';
    }

    ImGui::End();
}
