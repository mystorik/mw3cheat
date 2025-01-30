#include <Windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>

// wroten by @sodareverse
// credits to stackoverflow posts 👍

class TrollingFeatures {
private:
    std::atomic<bool> running;
    std::thread trollThread;
    HANDLE processHandle;
    std::mt19937 rng;
    
    // memory offsets
    const DWORD PLAYER_BASE = 0x00000000;
    const DWORD POSITION_OFFSET = 0x00000000;
    const DWORD VELOCITY_OFFSET = 0x00000000;
    const DWORD CHAT_OFFSET = 0x00000000;
    const DWORD PACKET_OFFSET = 0x00000000;
    const DWORD TEAM_OFFSET = 0x00000000;

    // troll settings
    bool packetSpam = false;
    bool chatSpam = false;
    bool serverCrash = false;
    bool modelBreak = false;
    int spamDelay = 10;
    int crashMethod = 0;

    void PacketFlooding() {
        if (!packetSpam) return;

        // flood server with malformed packets
        char junkData[1024];
        for (int i = 0; i < 1024; i++) {
            junkData[i] = rng() % 256;
        }

        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + PACKET_OFFSET),
            junkData, sizeof(junkData), nullptr);
    }

    void ChatFlooding() {
        if (!chatSpam) return;

        static const char* spamMessages[] = {
            "gg ez", "?????", "mad cuz bad", 
            "get good", "uninstall", ":)"
        };

        static int msgIndex = 0;
        const char* msg = spamMessages[msgIndex++ % 6];

        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + CHAT_OFFSET),
            msg, strlen(msg), nullptr);
    }

    void CrashServer() {
        if (!serverCrash) return;

        switch(crashMethod) {
            case 0: { // stack overflow
                char buffer[INT_MAX];
                memset(buffer, 0xFF, sizeof(buffer));
                WriteProcessMemory(processHandle,
                    (LPVOID)(PLAYER_BASE),
                    buffer, sizeof(buffer), nullptr);
                break;
            }
            case 1: { // memory leak
                while(true) {
                    malloc(1024 * 1024);
                }
                break;
            }
            case 2: { // infinite loop
                while(true) {
                    WriteProcessMemory(processHandle,
                        (LPVOID)(PLAYER_BASE),
                        &crashMethod, sizeof(crashMethod), nullptr);
                }
                break;
            }
        }
    }

    void BreakModels() {
        if (!modelBreak) return;

        // corrupt model data
        float scale = INFINITY;
        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + POSITION_OFFSET),
            &scale, sizeof(scale), nullptr);
    }

    void TrollWorker() {
        while (running) {
            PacketFlooding();
            ChatFlooding();
            CrashServer(); 
            BreakModels();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(spamDelay));
        }
    }

public:
    TrollingFeatures() : running(false) {
        processHandle = GetCurrentProcess();
        rng.seed(std::random_device()());
    }

    void Start() {
        if (!running) {
            running = true;
            trollThread = std::thread(&TrollingFeatures::TrollWorker, this);
        }
    }

    void Stop() {
        if (running) {
            running = false;
            if (trollThread.joinable()) {
                trollThread.join();
            }
        }
    }

    void SetPacketSpam(bool enabled) { packetSpam = enabled; }
    void SetChatSpam(bool enabled) { chatSpam = enabled; }
    void SetServerCrash(bool enabled) { serverCrash = enabled; }
    void SetModelBreak(bool enabled) { modelBreak = enabled; }
    void SetSpamDelay(int delay) { spamDelay = delay; }
    void SetCrashMethod(int method) { crashMethod = method; }
};

// global instance
TrollingFeatures* g_TrollFeatures = nullptr;

// exports
extern "C" {
    __declspec(dllexport) void InitializeTrolling() {
        if (!g_TrollFeatures) {
            g_TrollFeatures = new TrollingFeatures();
        }
    }

    __declspec(dllexport) void StartTrolling() {
        if (g_TrollFeatures) {
            g_TrollFeatures->Start();
        }
    }

    __declspec(dllexport) void StopTrolling() {
        if (g_TrollFeatures) {
            g_TrollFeatures->Stop();
        }
    }

    __declspec(dllexport) void SetPacketSpam(bool enabled) {
        if (g_TrollFeatures) {
            g_TrollFeatures->SetPacketSpam(enabled);
        }
    }

    __declspec(dllexport) void SetChatSpam(bool enabled) {
        if (g_TrollFeatures) {
            g_TrollFeatures->SetChatSpam(enabled);
        }
    }

    __declspec(dllexport) void SetServerCrash(bool enabled) {
        if (g_TrollFeatures) {
            g_TrollFeatures->SetServerCrash(enabled);
        }
    }

    __declspec(dllexport) void SetModelBreak(bool enabled) {
        if (g_TrollFeatures) {
            g_TrollFeatures->SetModelBreak(enabled);
        }
    }
}
