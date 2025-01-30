/*
 * reversed by @mystorik, @de-virtualized and @sodareverse (most work done by @sodareverse)
 */

#include <Windows.h>
#include <TlHelp32.h>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <queue>
#include <deque>
#include <functional>
#include <future>
#include <condition_variable>

namespace ricochet {

// Forward declarations
class NetworkMonitor;
class FileMonitor;
class ProcessMonitor;
class MemoryMonitor;
class IntegrityChecker;
class HeartbeatManager;

// Constants
constexpr int HEARTBEAT_INTERVAL_MS = 30000; // 30 seconds
constexpr int MAX_RETRY_ATTEMPTS = 3;
constexpr int SCAN_THREADS = 4;

// Utility functions
namespace utils {
    std::string GetLastErrorAsString() {
        DWORD error = GetLastError();
        if (error == 0) return "";

        LPSTR msgBuf = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&msgBuf,
            0,
            NULL
        );

        std::string msg(msgBuf, size);
        LocalFree(msgBuf);
        return msg;
    }

    std::vector<uint8_t> ReadFileBytes(const std::wstring& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file) return {};

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        return buffer;
    }

    uint32_t CalculateChecksum(const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint32_t checksum = 0;
        for (size_t i = 0; i < size; i++) {
            checksum = ((checksum << 5) | (checksum >> 27)) + bytes[i];
        }
        return checksum;
    }
}

// Heartbeat manager to verify client is still running and hasn't been tampered with
class HeartbeatManager {
private:
    std::atomic<bool> running;
    std::thread heartbeatThread;
    std::mutex mtx;
    std::condition_variable cv;
    
    struct HeartbeatData {
        uint32_t sequence;
        uint32_t checksum;
        uint64_t timestamp;
        std::vector<uint8_t> challenge;
    };

    uint32_t sequence;
    std::queue<HeartbeatData> pendingHeartbeats;
    
    // Server communication
    bool SendHeartbeat(const HeartbeatData& data) {
        // Serialize heartbeat data
        std::vector<uint8_t> packet;
        packet.reserve(sizeof(HeartbeatData) + data.challenge.size());
        
        // Add header
        packet.push_back('H');
        packet.push_back('B');
        
        // Add sequence
        auto seqBytes = reinterpret_cast<uint8_t*>(&data.sequence);
        packet.insert(packet.end(), seqBytes, seqBytes + sizeof(data.sequence));
        
        // Add checksum
        auto checksumBytes = reinterpret_cast<uint8_t*>(&data.checksum);
        packet.insert(packet.end(), checksumBytes, checksumBytes + sizeof(data.checksum));
        
        // Add timestamp
        auto timeBytes = reinterpret_cast<uint8_t*>(&data.timestamp);
        packet.insert(packet.end(), timeBytes, timeBytes + sizeof(data.timestamp));
        
        // Add challenge response
        packet.insert(packet.end(), data.challenge.begin(), data.challenge.end());
        
        // sends packet to server, protected under strings
        // simulating packet->server system
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        return true;
    }
    
    void HeartbeatWorker() {
        while (running) {
            HeartbeatData data;
            data.sequence = ++sequence;
            data.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            
            // Generate random challenge
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);
            
            data.challenge.resize(32);
            std::generate(data.challenge.begin(), data.challenge.end(), 
                [&]() { return static_cast<uint8_t>(dis(gen)); });
            
            // Calculate checksum of all data
            data.checksum = utils::CalculateChecksum(&data, sizeof(data) - sizeof(data.checksum));
            
            // Send heartbeat
            if (!SendHeartbeat(data)) {
                // Log failure and potentially trigger violation
                continue;
            }
            
            // Store pending heartbeat
            {
                std::lock_guard<std::mutex> lock(mtx);
                pendingHeartbeats.push(data);
                while (pendingHeartbeats.size() > 10) {
                    pendingHeartbeats.pop();
                }
            }
            
            // Wait for next interval
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS),
                [this]() { return !running; });
        }
    }

public:
    HeartbeatManager() : running(false), sequence(0) {}
    
    void Start() {
        running = true;
        heartbeatThread = std::thread(&HeartbeatManager::HeartbeatWorker, this);
    }
    
    void Stop() {
        running = false;
        cv.notify_all();
        if (heartbeatThread.joinable()) {
            heartbeatThread.join();
        }
    }
    
    bool ValidateHeartbeat(uint32_t seq, uint32_t checksum) {
        std::lock_guard<std::mutex> lock(mtx);
        
        // Find matching heartbeat
        auto it = std::find_if(pendingHeartbeats._Get_container().begin(),
                             pendingHeartbeats._Get_container().end(),
                             [seq](const HeartbeatData& data) {
                                 return data.sequence == seq;
                             });
                             
        if (it == pendingHeartbeats._Get_container().end()) {
            return false;
        }
        
        return it->checksum == checksum;
    }
};

// Core anti-cheat system
class RicochetCore {
private:
    std::atomic<bool> running;
    std::mutex mtx;
    std::vector<std::thread> scanThreads;
    std::unique_ptr<HeartbeatManager> heartbeat;
    
    struct ScanResult {
        bool detected;
        std::string reason;
        DWORD processId;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::deque<ScanResult> detections;
    
    // Memory scanning
    bool ScanMemoryRegion(HANDLE process, MEMORY_BASIC_INFORMATION& mbi) {
        if (mbi.State != MEM_COMMIT || 
            (mbi.Protect & PAGE_GUARD) ||
            (mbi.Protect & PAGE_NOACCESS)) {
            return false;
        }

        std::vector<uint8_t> buffer(mbi.RegionSize);
        SIZE_T bytesRead;
        
        if (!ReadProcessMemory(process, mbi.BaseAddress, buffer.data(), mbi.RegionSize, &bytesRead)) {
            return false;
        }

        // Scan for known cheat signatures
        static const std::vector<std::vector<uint8_t>> signatures = {
            // Example signatures
            {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14},
            {0x48, 0x89, 0x5C, 0x24, 0x08},
            {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20}
        };

        for (const auto& sig : signatures) {
            if (std::search(buffer.begin(), buffer.end(), sig.begin(), sig.end()) != buffer.end()) {
                LogDetection("Memory signature detected", GetProcessId(process));
                return true;
            }
        }

        // Scan for suspicious patterns
        static const std::vector<std::string> patterns = {
            "cheat",
            "hack",
            "inject",
            "memory"
        };

        std::string memString(reinterpret_cast<char*>(buffer.data()), bytesRead);
        std::transform(memString.begin(), memString.end(), memString.begin(), ::tolower);

        for (const auto& pattern : patterns) {
            if (memString.find(pattern) != std::string::npos) {
                LogDetection("Suspicious memory pattern detected", GetProcessId(process));
                return true;
            }
        }

        return false;
    }

    void ScanProcess(DWORD processId) {
        HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId);
        if (!process) return;

        MEMORY_BASIC_INFORMATION mbi;
        uint8_t* addr = nullptr;

        while (VirtualQueryEx(process, addr, &mbi, sizeof(mbi))) {
            if (ScanMemoryRegion(process, mbi)) {
                // Memory violation detected
                LogViolation("Memory scan violation", processId);
            }
            addr += mbi.RegionSize;
        }

        CloseHandle(process);
    }

    void LogDetection(const std::string& reason, DWORD processId) {
        std::lock_guard<std::mutex> lock(mtx);
        
        ScanResult result;
        result.detected = true;
        result.reason = reason;
        result.processId = processId;
        result.timestamp = std::chrono::system_clock::now();
        
        detections.push_back(result);
        
        // Keep only last 100 detections
        while (detections.size() > 100) {
            detections.pop_front();
        }
    }

    void LogViolation(const std::string& reason, DWORD processId = 0) {
        //not reversed
    }

public:
    RicochetCore() : running(false), heartbeat(std::make_unique<HeartbeatManager>()) {}

    bool Initialize() {
        running = true;
        
        // Start heartbeat system
        heartbeat->Start();
        
        // Start scanning threads
        for (int i = 0; i < SCAN_THREADS; i++) {
            scanThreads.emplace_back([this]() {
                while (running) {
                    // Scan system periodically
                    ScanProcesses();
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            });
        }

        return true;
    }

    void Shutdown() {
        running = false;
        
        heartbeat->Stop();
        
        for (auto& thread : scanThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    bool IsRunning() const {
        return running;
    }
    
    std::vector<ScanResult> GetDetections() {
        std::lock_guard<std::mutex> lock(mtx);
        return std::vector<ScanResult>(detections.begin(), detections.end());
    }
};

// Anti-cheat manager class
class RicochetManager {
private:
    std::unique_ptr<RicochetCore> core;
    
public:
    RicochetManager() : core(std::make_unique<RicochetCore>()) {}

    bool Start() {
        return core->Initialize();
    }

    void Stop() {
        if (core) {
            core->Shutdown();
        }
    }
    
    std::vector<RicochetCore::ScanResult> GetDetections() {
        return core->GetDetections();
    }
};

} // namespace ricochet

// Global instance
static std::unique_ptr<ricochet::RicochetManager> g_RicochetManager;

// Entry point
extern "C" __declspec(dllexport) bool InitializeAntiCheat() {
    g_RicochetManager = std::make_unique<ricochet::RicochetManager>();
    return g_RicochetManager->Start();
}

extern "C" __declspec(dllexport) void ShutdownAntiCheat() {
    if (g_RicochetManager) {
        g_RicochetManager->Stop();
    }
}
