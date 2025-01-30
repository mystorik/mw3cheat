#include <Windows.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cmath>
#include <queue>
#include <mutex>

// wroten by @de-virtualized
class FlyHack {
private:
    std::atomic<bool> running;
    std::thread flyThread;
    std::thread telemetryThread;
    float flySpeed = 5.0f;
    float maxSpeed = 20.0f;
    float acceleration = 0.5f;
    float currentSpeed = 0.0f;
    
    // needs upd
    const DWORD PLAYER_BASE = 0x00000000;
    const DWORD POSITION_OFFSET = 0x00000000;
    const DWORD FLAGS_OFFSET = 0x00000000;
    const DWORD VELOCITY_OFFSET = 0x00000000;
    const DWORD ANIMATION_OFFSET = 0x00000000;
    const DWORD STAMINA_OFFSET = 0x00000000;
    const DWORD COLLISION_OFFSET = 0x00000000;
    const DWORD CAMERA_OFFSET = 0x00000000;
    const DWORD NETWORK_OFFSET = 0x00000000;

    HANDLE processHandle;
    std::mt19937 rng;
    
    // Telemetry data for anti-cheat evasion
    struct TelemetryData {
        Vector3 position;
        Vector3 velocity;
        float speed;
        uint64_t timestamp;
    };
    
    std::queue<TelemetryData> telemetryQueue;
    std::mutex telemetryMutex;
    static const size_t MAX_TELEMETRY_SAMPLES = 100;
    
    // Movement smoothing
    Vector3 lastPosition;
    Vector3 lastVelocity;
    float smoothingFactor = 0.15f;
    
    // Camera control
    float pitchAngle = 0.0f;
    float yawAngle = 0.0f;
    float rollAngle = 0.0f;
    
    // Collision avoidance
    const float COLLISION_RADIUS = 1.0f;
    const float COLLISION_LOOKAHEAD = 2.0f;
    
    // Stamina system
    float currentStamina = 100.0f;
    const float MAX_STAMINA = 100.0f;
    const float STAMINA_DRAIN_RATE = 5.0f;
    const float STAMINA_REGEN_RATE = 2.0f;
    
    void UpdateStamina(float deltaTime) {
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
            currentStamina = std::max(0.0f, currentStamina - STAMINA_DRAIN_RATE * deltaTime);
        } else {
            currentStamina = std::min(MAX_STAMINA, currentStamina + STAMINA_REGEN_RATE * deltaTime);
        }
        
        // Write stamina to memory
        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + STAMINA_OFFSET),
            &currentStamina, sizeof(currentStamina), nullptr);
    }
    
    bool CheckCollision(const Vector3& pos, const Vector3& direction) {
        // Simple collision check
        Vector3 checkPos = pos;
        checkPos.x += direction.x * COLLISION_LOOKAHEAD;
        checkPos.y += direction.y * COLLISION_LOOKAHEAD;
        checkPos.z += direction.z * COLLISION_LOOKAHEAD;
        
        DWORD collisionFlags;
        ReadProcessMemory(processHandle,
            (LPCVOID)(PLAYER_BASE + COLLISION_OFFSET),
            &collisionFlags, sizeof(collisionFlags), nullptr);
            
        return (collisionFlags & 0x1) != 0;
    }
    
    void UpdateCamera() {
        // Get view angles from mouse movement
        POINT mousePos;
        GetCursorPos(&mousePos);
        
        static POINT lastMousePos = mousePos;
        
        float deltaX = (float)(mousePos.x - lastMousePos.x);
        float deltaY = (float)(mousePos.y - lastMousePos.y);
        
        yawAngle += deltaX * 0.1f;
        pitchAngle = std::clamp(pitchAngle + deltaY * 0.1f, -89.0f, 89.0f);
        
        lastMousePos = mousePos;
        
        // Write camera angles
        struct CameraData {
            float pitch;
            float yaw;
            float roll;
        } camera = { pitchAngle, yawAngle, rollAngle };
        
        WriteProcessMemory(processHandle,
            (LPVOID)(PLAYER_BASE + CAMERA_OFFSET),
            &camera, sizeof(camera), nullptr);
    }
    
    void AddTelemetrySample(const Vector3& pos, const Vector3& vel, float speed) {
        TelemetryData data;
        data.position = pos;
        data.velocity = vel;
        data.speed = speed;
        data.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        std::lock_guard<std::mutex> lock(telemetryMutex);
        telemetryQueue.push(data);
        
        while (telemetryQueue.size() > MAX_TELEMETRY_SAMPLES) {
            telemetryQueue.pop();
        }
    }
    
    void TelemetryWorker() {
        while (running) {
            // Process telemetry data to detect patterns
            std::vector<TelemetryData> samples;
            {
                std::lock_guard<std::mutex> lock(telemetryMutex);
                while (!telemetryQueue.empty()) {
                    samples.push_back(telemetryQueue.front());
                    telemetryQueue.pop();
                }
            }
            
            if (!samples.empty()) {
                // Analyze movement patterns
                float avgSpeed = 0.0f;
                Vector3 avgVelocity = {0, 0, 0};
                
                for (const auto& sample : samples) {
                    avgSpeed += sample.speed;
                    avgVelocity.x += sample.velocity.x;
                    avgVelocity.y += sample.velocity.y;
                    avgVelocity.z += sample.velocity.z;
                }
                
                avgSpeed /= samples.size();
                avgVelocity.x /= samples.size();
                avgVelocity.y /= samples.size();
                avgVelocity.z /= samples.size();
                
                // Add random variations to movement
                std::normal_distribution<float> speedNoise(0.0f, 0.1f);
                std::normal_distribution<float> dirNoise(0.0f, 0.05f);
                
                avgSpeed += speedNoise(rng);
                avgVelocity.x += dirNoise(rng);
                avgVelocity.y += dirNoise(rng);
                avgVelocity.z += dirNoise(rng);
                
                // Write network movement data
                struct NetworkData {
                    float speed;
                    Vector3 velocity;
                    uint64_t timestamp;
                } netData = {
                    avgSpeed,
                    avgVelocity,
                    std::chrono::system_clock::now().time_since_epoch().count()
                };
                
                WriteProcessMemory(processHandle,
                    (LPVOID)(PLAYER_BASE + NETWORK_OFFSET),
                    &netData, sizeof(netData), nullptr);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    void FlyWorker() {
        auto lastUpdate = std::chrono::high_resolution_clock::now();
        
        while (running) {
            auto now = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(now - lastUpdate).count();
            lastUpdate = now;
            
            // Read current position and velocity
            Vector3 pos, vel;
            ReadProcessMemory(processHandle, 
                (LPCVOID)(PLAYER_BASE + POSITION_OFFSET),
                &pos, sizeof(pos), nullptr);
                
            ReadProcessMemory(processHandle,
                (LPCVOID)(PLAYER_BASE + VELOCITY_OFFSET),
                &vel, sizeof(vel), nullptr);

            UpdateStamina(deltaTime);
            UpdateCamera();

            // Check if space key pressed for flying and has stamina
            if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && currentStamina > 0) {
                // Gradually increase speed
                currentSpeed = std::min(currentSpeed + acceleration * deltaTime, maxSpeed);
                
                // Calculate movement direction from camera angles
                float pitch = pitchAngle * 3.14159f / 180.0f;
                float yaw = yawAngle * 3.14159f / 180.0f;
                
                Vector3 moveDir;
                moveDir.x = cos(pitch) * sin(yaw);
                moveDir.y = sin(pitch);
                moveDir.z = cos(pitch) * cos(yaw);
                
                // Check for collisions
                if (!CheckCollision(pos, moveDir)) {
                    // Calculate new position with smooth acceleration
                    Vector3 targetPos = pos;
                    targetPos.x += moveDir.x * currentSpeed * (flySpeed / maxSpeed);
                    targetPos.y += moveDir.y * currentSpeed * (flySpeed / maxSpeed);
                    targetPos.z += moveDir.z * currentSpeed * (flySpeed / maxSpeed);
                    
                    // Smooth position changes
                    pos.x = lastPosition.x + (targetPos.x - lastPosition.x) * smoothingFactor;
                    pos.y = lastPosition.y + (targetPos.y - lastPosition.y) * smoothingFactor;
                    pos.z = lastPosition.z + (targetPos.z - lastPosition.z) * smoothingFactor;
                    
                    // Add slight random horizontal movement
                    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
                    pos.x += dist(rng);
                    pos.z += dist(rng);
                    
                    // Update velocity vector with smoothing
                    Vector3 targetVel;
                    targetVel.x = moveDir.x * currentSpeed;
                    targetVel.y = moveDir.y * currentSpeed;
                    targetVel.z = moveDir.z * currentSpeed;
                    
                    vel.x = lastVelocity.x + (targetVel.x - lastVelocity.x) * smoothingFactor;
                    vel.y = lastVelocity.y + (targetVel.y - lastVelocity.y) * smoothingFactor;
                    vel.z = lastVelocity.z + (targetVel.z - lastVelocity.z) * smoothingFactor;
                    
                    // Apply air resistance
                    vel.x *= 0.98f;
                    vel.y *= 0.98f;
                    vel.z *= 0.98f;
                    
                    // Store last position and velocity
                    lastPosition = pos;
                    lastVelocity = vel;
                    
                    // Write back modified position
                    WriteProcessMemory(processHandle,
                        (LPVOID)(PLAYER_BASE + POSITION_OFFSET),
                        &pos, sizeof(pos), nullptr);
                        
                    // Write velocity
                    WriteProcessMemory(processHandle,
                        (LPVOID)(PLAYER_BASE + VELOCITY_OFFSET),
                        &vel, sizeof(vel), nullptr);
                        
                    // Set flags to disable gravity
                    DWORD flags = 0x1; // IN_AIR flag
                    WriteProcessMemory(processHandle,
                        (LPVOID)(PLAYER_BASE + FLAGS_OFFSET), 
                        &flags, sizeof(flags), nullptr);
                        
                    // Set flying animation
                    DWORD animState = 0x2; // FLYING animation
                    WriteProcessMemory(processHandle,
                        (LPVOID)(PLAYER_BASE + ANIMATION_OFFSET),
                        &animState, sizeof(animState), nullptr);
                        
                    // Add telemetry sample
                    AddTelemetrySample(pos, vel, currentSpeed);
                }
            }
            else {
                currentSpeed = std::max(0.0f, currentSpeed - acceleration * 2 * deltaTime);
            }

            // Variable delay to avoid detection
            std::uniform_int_distribution<> delay(12, 28);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay(rng)));
        }
    }

public:
    struct Vector3 {
        float x, y, z;
    };

    FlyHack() : running(false) {
        // Get process handle
        DWORD pid = GetCurrentProcessId();
        processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE,
            FALSE, pid);
            
        // Initialize RNG
        std::random_device rd;
        rng.seed(rd());
        
        // Initialize last position and velocity
        lastPosition = {0, 0, 0};
        lastVelocity = {0, 0, 0};
    }

    void Start() {
        running = true;
        currentSpeed = 0.0f;
        currentStamina = MAX_STAMINA;
        flyThread = std::thread(&FlyHack::FlyWorker, this);
        telemetryThread = std::thread(&FlyHack::TelemetryWorker, this);
    }

    void Stop() {
        running = false;
        if (flyThread.joinable()) {
            flyThread.join();
        }
        if (telemetryThread.joinable()) {
            telemetryThread.join();
        }
        CloseHandle(processHandle);
    }

    void SetFlySpeed(float speed) {
        flySpeed = std::min(speed, maxSpeed);
    }
    
    void SetMaxSpeed(float speed) {
        maxSpeed = speed;
    }
    
    void SetAcceleration(float accel) {
        acceleration = accel;
    }
    
    void SetSmoothingFactor(float factor) {
        smoothingFactor = std::clamp(factor, 0.0f, 1.0f);
    }
};

// Global instance
static std::unique_ptr<FlyHack> g_FlyHack;

// Exports
extern "C" __declspec(dllexport) void EnableFly() {
    if (!g_FlyHack) {
        g_FlyHack = std::make_unique<FlyHack>();
    }
    g_FlyHack->Start();
}

extern "C" __declspec(dllexport) void DisableFly() {
    if (g_FlyHack) {
        g_FlyHack->Stop();
        g_FlyHack.reset();
    }
}

extern "C" __declspec(dllexport) void SetFlySpeed(float speed) {
    if (g_FlyHack) {
        g_FlyHack->SetFlySpeed(speed);
    }
}

extern "C" __declspec(dllexport) void SetMaxFlySpeed(float speed) {
    if (g_FlyHack) {
        g_FlyHack->SetMaxSpeed(speed);
    }
}

extern "C" __declspec(dllexport) void SetFlyAcceleration(float accel) {
    if (g_FlyHack) {
        g_FlyHack->SetAcceleration(accel);
    }
}

extern "C" __declspec(dllexport) void SetFlySmoothingFactor(float factor) {
    if (g_FlyHack) {
        g_FlyHack->SetSmoothingFactor(factor);
    }
}
