#include "sidebar.hpp"
#include "imgui.h"

#include "../../core/crypto.hpp"
#include "../../app/app_state.hpp"

void RenderSidebar() {
    ImGui::BeginChild("Sidebar", ImVec2(180, 0), true, ImGuiWindowFlags_NoScrollbar);
    
    ImGui::Spacing();
    ImGui::TextDisabled("NAVIGATOR");
    ImGui::Separator();
    ImGui::Spacing();

    auto render_nav_item = [](const char* label, AppState target) {
        bool is_selected = (g_state.currentState == target);
        if (is_selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        
        if (ImGui::Button(label, ImVec2(-FLT_MIN, 40))) {
            g_state.currentState = target;
        }
        
        if (is_selected) ImGui::PopStyleColor();
    };

    render_nav_item("Vault", AppState::Dashboard);
    render_nav_item("Add New", AppState::AddEntry);
    
    // Fill space
    float footer_height = 60.0f;
    ImGui::Dummy(ImVec2(0, ImGui::GetContentRegionAvail().y - footer_height));
    
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Lock Vault", ImVec2(-FLT_MIN, 35))) {
        clear_master_key();
        g_state.entries.clear();
        g_state.currentState = AppState::Login;
    }
    ImGui::PopStyleColor(2);
    
    ImGui::EndChild();
}
