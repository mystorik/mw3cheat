#include <Windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <queue>
#include <mutex>
#include <array>
#include <map>

class RageBot {
private:
    std::atomic<bool> running;
    std::thread botThread;
    std::thread predictionThread;
    std::thread recoilThread;
    std::thread targetingThread;
    
    // memory offsets
    const DWORD PLAYER_BASE = 0x00000000;
    const DWORD TARGET_OFFSET = 0x00000000; 
    const DWORD AIM_OFFSET = 0x00000000;
    const DWORD WEAPON_OFFSET = 0x00000000;
    const DWORD HEALTH_OFFSET = 0x00000000;
    const DWORD TEAM_OFFSET = 0x00000000;
    const DWORD VELOCITY_OFFSET = 0x00000000;
    const DWORD RECOIL_OFFSET = 0x00000000;
    const DWORD SPREAD_OFFSET = 0x00000000;
    const DWORD AMMO_OFFSET = 0x00000000;
    const DWORD PING_OFFSET = 0x00000000;
    const DWORD STANCE_OFFSET = 0x00000000;
    const DWORD KILLSTREAK_OFFSET = 0x00000000;
    
    HANDLE processHandle;
    std::mt19937 rng;
    
    // aimbot settings
    float fov = 180.0f; // full 180 degree fov for rage
    float smoothing = 0.0f; // no smoothing for rage
    bool teamCheck = true; // shoot everything but teammates
    float predictionTime = 0.2f; // seconds to predict movement
    bool autoWall = true; // shoot through walls
    bool silentAim = true; // hide visual recoil
    bool autoShoot = true; // automatically shoot when target acquired
    bool autoScope = true; // automatically scope when needed
    bool autoReload = true; // automatically reload when low ammo
    int priorityMode = 0; // 0=distance, 1=health, 2=threat
    float maxDistance = 1000.0f; // max targeting distance
    
    // advanced settings
    struct WeaponConfig {
        float spread;
        float recoil;
        float damage;
        float penetration;
        int fireRate;
        int magSize;
        bool automatic;
    };
    
    std::map<int, WeaponConfig> weaponConfigs;
    
    // target tracking
    struct TargetInfo {
        Vector3 position;
        Vector3 velocity;
        float health;
        int team;
        float threat;
        uint64_t lastSeen;
        bool visible;
        bool predicted;
    };
    
    std::mutex targetMutex;
    std::map<int, TargetInfo> targetCache;
    static const size_t MAX_TARGETS = 64;
    
    // movement prediction
    struct PredictionData {
        Vector3 position;
        Vector3 velocity;
        float timestamp;
    };
    
    std::queue<PredictionData> predictionQueue;
    std::mutex predictionMutex;
    static const size_t MAX_PREDICTIONS = 100;
    
    // recoil control
    struct RecoilState {
        float verticalRecoil;
        float horizontalRecoil;
        float spread;
        int shotsFired;
        uint64_t lastShot;
    };
    
    RecoilState recoilState;
    std::mutex recoilMutex;
    
    void InitializeWeaponConfigs() {
        // add configs for all weapon types
        WeaponConfig assault = {2.5f, 1.8f, 30.0f, 0.7f, 600, 30, true};
        WeaponConfig sniper = {0.1f, 5.0f, 150.0f, 0.9f, 50, 5, false};
        WeaponConfig smg = {3.0f, 1.2f, 20.0f, 0.5f, 900, 25, true};
        weaponConfigs[1] = assault;
        weaponConfigs[2] = sniper; 
        weaponConfigs[3] = smg;
    }
    
    void PredictionWorker() {
        // implement movement prediction
        while(running) {
            std::lock_guard<std::mutex> lock(predictionMutex);
            
            // track velocity and acceleration
            for(auto& target : targetCache) {
                Vector3 lastPos = target.second.position;
                Vector3 lastVel = target.second.velocity;
                
                // calculate new position based on velocity
                target.second.position += lastVel * predictionTime;
                
                // apply gravity
                target.second.velocity.z -= 9.81f * predictionTime;
                
                // handle movement states
                if(GetAsyncKeyState('W') & 0x8000) {
                    target.second.velocity *= 1.5f; // running multiplier
                }
                
                // lag compensation
                float ping = 0.0f;
                ReadProcessMemory(processHandle, 
                    (LPCVOID)(PLAYER_BASE + PING_OFFSET),
                    &ping, sizeof(ping), nullptr);
                target.second.position += target.second.velocity * (ping/1000.0f);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void RecoilWorker() {
        // implement recoil control
        while(running) {
            std::lock_guard<std::mutex> lock(recoilMutex);
            
            // track recoil patterns
            if(recoilState.shotsFired > 0) {
                // get current weapon config
                int weaponId = 0;
                ReadProcessMemory(processHandle,
                    (LPCVOID)(PLAYER_BASE + WEAPON_OFFSET),
                    &weaponId, sizeof(weaponId), nullptr);
                    
                auto& config = weaponConfigs[weaponId];
                
                // calculate recoil
                recoilState.verticalRecoil = config.recoil * recoilState.shotsFired;
                recoilState.horizontalRecoil = (rng() % 100 - 50) / 100.0f * config.recoil;
                
                // handle stance modifiers
                int stance = 0;
                ReadProcessMemory(processHandle,
                    (LPCVOID)(PLAYER_BASE + STANCE_OFFSET),
                    &stance, sizeof(stance), nullptr);
                    
                if(stance == 1) { // crouching
                    recoilState.verticalRecoil *= 0.7f;
                    recoilState.horizontalRecoil *= 0.7f;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void TargetingWorker() {
        // implement target selection
        while(running) {
            std::lock_guard<std::mutex> lock(targetMutex);
            
            // scan for valid targets
            for(int i = 0; i < MAX_TARGETS; i++) {
                Vector3 pos;
                float health;
                int team;
                
                ReadProcessMemory(processHandle,
                    (LPCVOID)(PLAYER_BASE + TARGET_OFFSET + i * sizeof(Vector3)),
                    &pos, sizeof(pos), nullptr);
                    
                ReadProcessMemory(processHandle,
                    (LPCVOID)(PLAYER_BASE + HEALTH_OFFSET + i * sizeof(float)),
                    &health, sizeof(health), nullptr);
                    
                ReadProcessMemory(processHandle,
                    (LPCVOID)(PLAYER_BASE + TEAM_OFFSET + i * sizeof(int)),
                    &team, sizeof(team), nullptr);
                    
                if(health > 0) {
                    TargetInfo& target = targetCache[i];
                    target.position = pos;
                    target.health = health;
                    target.team = team;
                    target.visible = CheckVisibility(pos);
                    target.threat = CalculateThreat(target);
                    target.lastSeen = GetTickCount64();
                }
            }
            
            // cleanup old targets
            for(auto it = targetCache.begin(); it != targetCache.end();) {
                if(GetTickCount64() - it->second.lastSeen > 5000) {
                    it = targetCache.erase(it);
                } else {
                    ++it;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    void AimWorker() {
        while (running) {
            // get closest target
            Vector3 targetPos = GetClosestTarget();
            
            // instant snap aim
            if (IsValidTarget(targetPos)) {
                Vector3 aimAngles = CalculateAngles(targetPos);
                WriteAimAngles(aimAngles);
                
                if (autoShoot) {
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    Vector3 GetClosestTarget() {
        // implement proper target scanning
        Vector3 closest = {0,0,0};
        float closestDist = 999999.0f;
        
        Vector3 myPos;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE),
            &myPos, sizeof(myPos), nullptr);
            
        std::lock_guard<std::mutex> lock(targetMutex);
        
        for(const auto& target : targetCache) {
            // check visibility and penetration
            if(!target.second.visible && !autoWall) continue;
            
            // calculate distance
            Vector3 delta = target.second.position - myPos;
            float dist = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
            
            // apply priority filtering
            float priority = dist;
            if(priorityMode == 1) {
                priority = target.second.health;
            } else if(priorityMode == 2) {
                priority = target.second.threat;
            }
            
            if(priority < closestDist) {
                closestDist = priority;
                closest = target.second.position;
            }
        }
            
        return closest;
    }
    
    bool IsValidTarget(const Vector3& pos) {
        // implement proper target validation
        if(pos.x == 0 && pos.y == 0 && pos.z == 0) return false;
        
        Vector3 myPos;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE),
            &myPos, sizeof(myPos), nullptr);
            
        // verify distance
        Vector3 delta = pos - myPos;
        float dist = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        if(dist > maxDistance) return false;
        
        // check team if enabled
        if(teamCheck) {
            int targetTeam;
            int myTeam;
            ReadProcessMemory(processHandle,
                (LPCVOID)(PLAYER_BASE + TEAM_OFFSET),
                &myTeam, sizeof(myTeam), nullptr);
                
            ReadProcessMemory(processHandle,
                (LPCVOID)(pos.x + TEAM_OFFSET),
                &targetTeam, sizeof(targetTeam), nullptr);
                
            if(targetTeam == myTeam) return false;
        }
        
        // check visibility/penetration
        return CheckVisibility(pos) || autoWall;
    }
    
    Vector3 CalculateAngles(const Vector3& target) {
        // improve angle calculation
        Vector3 angles;
        Vector3 myPos;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE),
            &myPos, sizeof(myPos), nullptr);
            
        Vector3 delta = target - myPos;
        
        // account for bullet drop
        float dist = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        delta.z += 0.5f * 9.81f * dist * dist / (1000.0f * 1000.0f);
        
        // handle movement prediction
        Vector3 targetVel;
        ReadProcessMemory(processHandle,
            (LPCVOID)(target.x + VELOCITY_OFFSET),
            &targetVel, sizeof(targetVel), nullptr);
            
        delta += targetVel * predictionTime;
        
        // calculate base angles
        float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);
        angles.x = atan2(-delta.z, hyp) * 180.0f / 3.14159f;
        angles.y = atan2(delta.y, delta.x) * 180.0f / 3.14159f;
        angles.z = 0.0f;
        
        // apply recoil compensation
        std::lock_guard<std::mutex> lock(recoilMutex);
        angles.x += recoilState.verticalRecoil;
        angles.y += recoilState.horizontalRecoil;
        
        // add random spread
        float spread = recoilState.spread;
        angles.x += (rng() % 100 - 50) / 100.0f * spread;
        angles.y += (rng() % 100 - 50) / 100.0f * spread;
        
        return angles;
    }
    
    void WriteAimAngles(const Vector3& angles) {
        // improve angle writing
        Vector3 finalAngles = angles;
        
        // add smoothing if enabled
        if(smoothing > 0) {
            Vector3 currentAngles;
            ReadProcessMemory(processHandle,
                (LPCVOID)(PLAYER_BASE + AIM_OFFSET),
                &currentAngles, sizeof(currentAngles), nullptr);
                
            Vector3 delta = angles - currentAngles;
            finalAngles = currentAngles + delta * smoothing;
        }
        
        // handle silent aim
        if(silentAim) {
            // write to separate memory location
            WriteProcessMemory(processHandle,
                (LPVOID)(PLAYER_BASE + AIM_OFFSET + 0x100),
                &finalAngles, sizeof(finalAngles), nullptr);
        } else {
            WriteProcessMemory(processHandle,
                (LPVOID)(PLAYER_BASE + AIM_OFFSET),
                &finalAngles, sizeof(finalAngles), nullptr);
        }
    }
    
    void UpdateRecoil() {
        // implement recoil updating
        std::lock_guard<std::mutex> lock(recoilMutex);
        
        // track shots fired
        if(GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            uint64_t now = GetTickCount64();
            if(now - recoilState.lastShot > 50) { // minimum time between shots
                recoilState.shotsFired++;
                recoilState.lastShot = now;
            }
        } else {
            // handle recoil reset
            recoilState.shotsFired = std::max(0, recoilState.shotsFired - 1);
        }
        
        // get weapon config
        int weaponId = 0;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE + WEAPON_OFFSET),
            &weaponId, sizeof(weaponId), nullptr);
            
        auto& config = weaponConfigs[weaponId];
        
        // calculate recoil pattern
        float baseRecoil = config.recoil;
        recoilState.verticalRecoil = baseRecoil * pow(1.1f, recoilState.shotsFired);
        recoilState.horizontalRecoil = sin(recoilState.shotsFired * 0.1f) * baseRecoil;
    }
    
    void UpdateSpread() {
        // implement spread calculation
        std::lock_guard<std::mutex> lock(recoilMutex);
        
        // get weapon config
        int weaponId = 0;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE + WEAPON_OFFSET),
            &weaponId, sizeof(weaponId), nullptr);
            
        auto& config = weaponConfigs[weaponId];
        
        // base spread
        float spread = config.spread;
        
        // account for movement
        Vector3 velocity;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE + VELOCITY_OFFSET),
            &velocity, sizeof(velocity), nullptr);
            
        float speed = sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
        spread *= 1.0f + speed * 0.05f;
        
        // handle different stances
        int stance = 0;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE + STANCE_OFFSET),
            &stance, sizeof(stance), nullptr);
            
        if(stance == 1) { // crouching
            spread *= 0.7f;
        } else if(stance == 2) { // prone
            spread *= 0.5f;
        }
        
        // add random variation
        spread *= 0.9f + (rng() % 20) / 100.0f;
        
        recoilState.spread = spread;
    }
    
    bool CheckVisibility(const Vector3& target) {
        // implement visibility checks
        Vector3 myPos;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE),
            &myPos, sizeof(myPos), nullptr);
            
        // ray trace to target
        Vector3 dir = target - myPos;
        float dist = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        dir = dir * (1.0f / dist);
        
        const float STEP = 1.0f;
        Vector3 pos = myPos;
        
        for(float d = 0; d < dist; d += STEP) {
            pos = pos + dir * STEP;
            
            // check for collision
            int collision = 0;
            ReadProcessMemory(processHandle,
                (LPCVOID)(pos.x),
                &collision, sizeof(collision), nullptr);
                
            if(collision > 0) {
                // handle penetration
                if(autoWall) {
                    int weaponId = 0;
                    ReadProcessMemory(processHandle,
                        (LPCVOID)(PLAYER_BASE + WEAPON_OFFSET),
                        &weaponId, sizeof(weaponId), nullptr);
                        
                    return weaponConfigs[weaponId].penetration > 0.5f;
                }
                return false;
            }
        }
        
        return true;
    }
    
    float CalculateThreat(const TargetInfo& target) {
        // implement threat calculation
        float threat = 0.0f;
        
        // consider distance
        Vector3 myPos;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE),
            &myPos, sizeof(myPos), nullptr);
            
        Vector3 delta = target.position - myPos;
        float dist = sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        threat += (1000.0f - std::min(dist, 1000.0f)) / 1000.0f;
        
        // check weapon
        int targetWeapon = 0;
        ReadProcessMemory(processHandle,
            (LPCVOID)(target.position.x + WEAPON_OFFSET),
            &targetWeapon, sizeof(targetWeapon), nullptr);
            
        if(targetWeapon > 0 && weaponConfigs.count(targetWeapon)) {
            threat += weaponConfigs[targetWeapon].damage / 100.0f;
        }
        
        // account for health
        threat += (100.0f - target.health) / 100.0f;
        
        // handle killstreaks
        int killstreak = 0;
        ReadProcessMemory(processHandle,
            (LPCVOID)(target.position.x + KILLSTREAK_OFFSET),
            &killstreak, sizeof(killstreak), nullptr);
            
        threat += killstreak * 0.1f;
        
        return threat;
    }
    
public:
    RageBot() : running(false) {
        InitializeWeaponConfigs();
    }
    
    void Start() {
        if (!running) {
            running = true;
            botThread = std::thread(&RageBot::AimWorker, this);
            predictionThread = std::thread(&RageBot::PredictionWorker, this);
            recoilThread = std::thread(&RageBot::RecoilWorker, this);
            targetingThread = std::thread(&RageBot::TargetingWorker, this);
        }
    }
    
    void Stop() {
        if (running) {
            running = false;
            if (botThread.joinable()) botThread.join();
            if (predictionThread.joinable()) predictionThread.join();
            if (recoilThread.joinable()) recoilThread.join();
            if (targetingThread.joinable()) targetingThread.join();
        }
    }
    
    // Setters for configuration
    void SetFOV(float newFov) { fov = newFov; }
    void SetTeamCheck(bool check) { teamCheck = check; }
    void SetPredictionTime(float time) { predictionTime = time; }
    void SetAutoWall(bool enabled) { autoWall = enabled; }
    void SetSilentAim(bool enabled) { silentAim = enabled; }
    void SetAutoShoot(bool enabled) { autoShoot = enabled; }
    void SetAutoScope(bool enabled) { autoScope = enabled; }
    void SetAutoReload(bool enabled) { autoReload = enabled; }
    void SetPriorityMode(int mode) { priorityMode = mode; }
    void SetMaxDistance(float dist) { maxDistance = dist; }
};

// global instance
static std::unique_ptr<RageBot> g_RageBot;

// exports
extern "C" __declspec(dllexport) void EnableRage() {
    if (!g_RageBot) {
        g_RageBot = std::make_unique<RageBot>();
    }
    g_RageBot->Start();
}

extern "C" __declspec(dllexport) void DisableRage() {
    if (g_RageBot) {
        g_RageBot->Stop();
        g_RageBot.reset();
    }
}

extern "C" __declspec(dllexport) void SetRageFOV(float fov) {
    if (g_RageBot) {
        g_RageBot->SetFOV(fov);
    }
}

extern "C" __declspec(dllexport) void SetRageTeamCheck(bool check) {
    if (g_RageBot) {
        g_RageBot->SetTeamCheck(check);
    }
}

extern "C" __declspec(dllexport) void SetRagePredictionTime(float time) {
    if (g_RageBot) {
        g_RageBot->SetPredictionTime(time);
    }
}

extern "C" __declspec(dllexport) void SetRageAutoWall(bool enabled) {
    if (g_RageBot) {
        g_RageBot->SetAutoWall(enabled);
    }
}

extern "C" __declspec(dllexport) void SetRageSilentAim(bool enabled) {
    if (g_RageBot) {
        g_RageBot->SetSilentAim(enabled);
    }
}

extern "C" __declspec(dllexport) void SetRageAutoShoot(bool enabled) {
    if (g_RageBot) {
        g_RageBot->SetAutoShoot(enabled);
    }
}

extern "C" __declspec(dllexport) void SetRageAutoScope(bool enabled) {
    if (g_RageBot) {
        g_RageBot->SetAutoScope(enabled);
    }
}

extern "C" __declspec(dllexport) void SetRageAutoReload(bool enabled) {
    if (g_RageBot) {
        g_RageBot->SetAutoReload(enabled);
    }
}

extern "C" __declspec(dllexport) void SetRagePriorityMode(int mode) {
    if (g_RageBot) {
        g_RageBot->SetPriorityMode(mode);
    }
}

extern "C" __declspec(dllexport) void SetRageMaxDistance(float dist) {
    if (g_RageBot) {
        g_RageBot->SetMaxDistance(dist);
    }
}
