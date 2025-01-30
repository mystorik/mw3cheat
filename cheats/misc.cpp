#include <Windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

class MiscFeatures {
private:
    std::atomic<bool> running;
    std::thread miscThread;
    HANDLE processHandle;
    
    // memory offsets (fix please)
    const DWORD PLAYER_BASE = 0x00000000;
    const DWORD BHOP_OFFSET = 0x00000000;
    const DWORD FOV_OFFSET = 0x00000000;
    const DWORD FLASH_OFFSET = 0x00000000;
    const DWORD RADAR_OFFSET = 0x00000000;
    
    // misc settings
    bool bhopEnabled = false;
    bool noFlashEnabled = false;
    bool radarEnabled = false;
    float customFOV = 90.0f;
    
    void BunnyHop() {
        if (!bhopEnabled) return;
        
        // auto jump when spacebar held
        bool onGround;
        ReadProcessMemory(processHandle, 
            (LPVOID)(PLAYER_BASE + BHOP_OFFSET),
            &onGround, sizeof(onGround), nullptr);
            
        if (onGround && GetAsyncKeyState(VK_SPACE)) {
            // simulate jump
            bool jump = true;
            WriteProcessMemory(processHandle,
                (LPVOID)(PLAYER_BASE + BHOP_OFFSET),
                &jump, sizeof(jump), nullptr);
        }
    }
    
    void NoFlash() {
        if (!noFlashEnabled) return;
        
        // remove flash effect
        float flashAlpha = 0.0f;
        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + FLASH_OFFSET), 
            &flashAlpha, sizeof(flashAlpha), nullptr);
    }
    
    void RadarHack() {
        if (!radarEnabled) return;
        
        // force show enemies on radar
        bool visible = true;
        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + RADAR_OFFSET),
            &visible, sizeof(visible), nullptr);
    }
    
    void FOVChanger() {
        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + FOV_OFFSET),
            &customFOV, sizeof(customFOV), nullptr);
    }
    
    void MiscWorker() {
        while (running) {
            BunnyHop();
            NoFlash();
            RadarHack();
            FOVChanger();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

public:
    MiscFeatures() : running(false) {
        processHandle = GetCurrentProcess();
    }
    
    void Start() {
        if (!running) {
            running = true;
            miscThread = std::thread(&MiscFeatures::MiscWorker, this);
        }
    }
    
    void Stop() {
        if (running) {
            running = false;
            if (miscThread.joinable()) {
                miscThread.join();
            }
        }
    }
    
    void SetBHop(bool enabled) { bhopEnabled = enabled; }
    void SetNoFlash(bool enabled) { noFlashEnabled = enabled; }
    void SetRadar(bool enabled) { radarEnabled = enabled; }
    void SetFOV(float fov) { customFOV = fov; }
};

// global instance
MiscFeatures* g_MiscFeatures = nullptr;

// exports
extern "C" {
    __declspec(dllexport) void InitializeMisc() {
        if (!g_MiscFeatures) {
            g_MiscFeatures = new MiscFeatures();
        }
    }
    
    __declspec(dllexport) void StartMisc() {
        if (g_MiscFeatures) {
            g_MiscFeatures->Start();
        }
    }
    
    __declspec(dllexport) void StopMisc() {
        if (g_MiscFeatures) {
            g_MiscFeatures->Stop();
        }
    }
    
    __declspec(dllexport) void SetBHop(bool enabled) {
        if (g_MiscFeatures) {
            g_MiscFeatures->SetBHop(enabled);
        }
    }
    
    __declspec(dllexport) void SetNoFlash(bool enabled) {
        if (g_MiscFeatures) {
            g_MiscFeatures->SetNoFlash(enabled);
        }
    }
    
    __declspec(dllexport) void SetRadar(bool enabled) {
        if (g_MiscFeatures) {
            g_MiscFeatures->SetRadar(enabled);
        }
    }
    
    __declspec(dllexport) void SetFOV(float fov) {
        if (g_MiscFeatures) {
            g_MiscFeatures->SetFOV(fov);
        }
    }
}
