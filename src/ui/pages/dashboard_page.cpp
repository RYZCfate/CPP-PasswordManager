#include "dashboard_page.hpp"
#include "../../app/app_state.hpp"
#include "../../core/storage.hpp"
#include "../layout/sidebar.hpp"
#include "imgui.h"
#include <string>
#include <vector>
#include <algorithm>

void RenderDashboardPage()
{
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);

    ImGui::Begin("MainWindow", nullptr, 
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    RenderSidebar();

    ImGui::SameLine();

    ImGui::BeginChild("Content", ImVec2(0, 0), ImGuiChildFlags_None);

    ImGui::Text("Vault Management");
    ImGui::Separator();
    ImGui::Spacing();

    // Search Bar
    static char search_buf[128] = "";
    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint("##Search", "Search sites...", search_buf, IM_ARRAYSIZE(search_buf));
    ImGui::SameLine();
    if (ImGui::Button("Clear")) { search_buf[0] = '\0'; }

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 120);
    if (ImGui::Button("Add Credential", ImVec2(120, 0)))
    {
        g_state.currentState = AppState::AddEntry;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    static std::string last_copied = "";
    static float copy_feedback_timer = 0.0f;
    if (copy_feedback_timer > 0) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Copied %s to clipboard!", last_copied.c_str());
        copy_feedback_timer -= ImGui::GetIO().DeltaTime;
    } else {
        ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
    }

    ImGui::BeginChild("ScrollArea");
    std::string filter = search_buf;
    std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

    for (size_t i = 0; i < g_state.entries.size(); ++i)
    {
        auto &entry = g_state.entries[i];
        
        // Filter logic
        std::string site_lower = entry.site;
        std::transform(site_lower.begin(), site_lower.end(), site_lower.begin(), ::tolower);
        if (!filter.empty() && site_lower.find(filter) == std::string::npos) continue;

        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginChild("EntryCard", ImVec2(0, 80), true);
        
        // Site Info
        ImGui::SetCursorPos(ImVec2(10, 15));
        ImGui::BeginGroup();
        // Circular-ish icon placeholder
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::Button(entry.site.substr(0,1).c_str(), ImVec2(50, 50));
        ImGui::PopStyleVar();
        ImGui::SameLine(70);
        
        ImGui::BeginGroup();
        ImGui::Text("%s", entry.site.c_str());
        ImGui::TextDisabled("%s", entry.username.c_str());
        ImGui::EndGroup();
        ImGui::EndGroup();

        // Actions
        float available_x = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(available_x - 190);
        
        ImGui::BeginGroup();
        ImGui::SetCursorPosY(25);
        if (ImGui::Button("Copy", ImVec2(55, 30))) {
            ImGui::SetClipboardText(entry.password.c_str());
            last_copied = "password";
            copy_feedback_timer = 2.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("2FA", ImVec2(55, 30))) {
            g_state.selectedCredential = &entry;
            g_state.currentState = AppState::TwoFactorAuth;
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("Del", ImVec2(55, 30))) {
            ImGui::OpenPopup("Delete Confirmation");
        }
        ImGui::PopStyleColor();
        
        if (ImGui::BeginPopupModal("Delete Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete '%s'?", entry.site.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                std::string site_to_del = entry.site;
                delete_entry(g_state.entries, site_to_del);
                save_entries(g_state.entries);
                g_state.selectedCredential = nullptr;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                ImGui::EndGroup();
                ImGui::EndChild();
                ImGui::PopID();
                break; // Break the loop because we modified the vector
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        
        ImGui::EndGroup();

        ImGui::EndChild();
        ImGui::PopID();
        ImGui::Spacing();
    }
    ImGui::EndChild(); // ScrollArea

    ImGui::EndChild(); // Content

    ImGui::End();
}