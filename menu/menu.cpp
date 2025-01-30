#include <imgui.h>
#include <Windows.h>

// written by @sodareverse
// credits to stackoverflow posts 👍
// you need to have imgui linked to the project

class CheatMenu {
private:
    bool menuOpen = false;
    bool ragebotEnabled = false;
    bool miscEnabled = false;
    bool trollingEnabled = false;

    // misc settings
    bool bhopEnabled = false;
    bool noFlashEnabled = false;
    bool radarEnabled = false;
    bool flyEnabled = false;
    bool speedEnabled = false;
    bool godModeEnabled = false;
    bool noClipEnabled = false;
    float fovValue = 90.0f;
    float speedValue = 1.0f;

    // trolling settings 
    bool packetSpamEnabled = false;
    bool chatSpamEnabled = false;
    bool serverCrashEnabled = false;
    bool modelBreakEnabled = false;

public:
    void Render() {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menuOpen = !menuOpen;
        }

        if (!menuOpen) return;

        ImGui::Begin("cod-mwcheat", &menuOpen, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::BeginTabBar("CheatTabs")) {
            if (ImGui::BeginTabItem("Ragebot")) {
                ImGui::Checkbox("Enable Ragebot", &ragebotEnabled);
                if (ragebotEnabled) {
                    ImGui::SliderFloat("FOV", &fov, 0.0f, 180.0f);
                    ImGui::SliderFloat("Prediction Time", &predictionTime, 0.0f, 1.0f);
                    ImGui::SliderFloat("Max Distance", &maxDistance, 100.0f, 5000.0f);
                    
                    ImGui::Checkbox("Team Check", &teamCheck);
                    ImGui::Checkbox("Auto Wall", &autoWall);
                    ImGui::Checkbox("Silent Aim", &silentAim);
                    ImGui::Checkbox("Auto Shoot", &autoShoot);
                    ImGui::Checkbox("Auto Scope", &autoScope);
                    ImGui::Checkbox("Auto Reload", &autoReload);
                    
                    const char* priorities[] = {"Distance", "Health", "Threat"};
                    ImGui::Combo("Priority", &priorityMode, priorities, IM_ARRAYSIZE(priorities));
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Misc")) {
                ImGui::Checkbox("Enable Misc Features", &miscEnabled);
                if (miscEnabled) {
                    ImGui::Checkbox("Bunny Hop", &bhopEnabled);
                    if (bhopEnabled) SetBHop(true);
                    
                    ImGui::Checkbox("No Flash", &noFlashEnabled);
                    if (noFlashEnabled) SetNoFlash(true);
                    
                    ImGui::Checkbox("Radar Hack", &radarEnabled);
                    if (radarEnabled) SetRadar(true);

                    ImGui::Checkbox("Fly Hack", &flyEnabled);
                    if (flyEnabled) SetFly(true);

                    ImGui::Checkbox("Speed Hack", &speedEnabled);
                    if (speedEnabled) {
                        ImGui::SliderFloat("Speed Multiplier", &speedValue, 1.0f, 10.0f);
                        SetSpeed(speedValue);
                    }

                    ImGui::Checkbox("God Mode", &godModeEnabled);
                    if (godModeEnabled) SetGodMode(true);

                    ImGui::Checkbox("No Clip", &noClipEnabled);
                    if (noClipEnabled) SetNoClip(true);
                    
                    ImGui::SliderFloat("FOV", &fovValue, 60.0f, 120.0f);
                    SetFOV(fovValue);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Trolling")) {
                ImGui::Checkbox("Enable Trolling", &trollingEnabled);
                if (trollingEnabled) {
                    ImGui::Checkbox("Packet Spam", &packetSpamEnabled);
                    if (packetSpamEnabled) SetPacketSpam(true);
                    
                    ImGui::Checkbox("Chat Spam", &chatSpamEnabled);
                    if (chatSpamEnabled) SetChatSpam(true);
                    
                    ImGui::Checkbox("Server Crash", &serverCrashEnabled);
                    if (serverCrashEnabled) SetServerCrash(true);
                    
                    ImGui::Checkbox("Model Break", &modelBreakEnabled);
                    if (modelBreakEnabled) SetModelBreak(true);
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::End();
    }
};

// global instance
CheatMenu* g_CheatMenu = nullptr;

// exports
extern "C" {
    __declspec(dllexport) void InitializeMenu() {
        if (!g_CheatMenu) {
            g_CheatMenu = new CheatMenu();
        }
    }

    __declspec(dllexport) void RenderMenu() {
        if (g_CheatMenu) {
            g_CheatMenu->Render();
        }
    }
}
