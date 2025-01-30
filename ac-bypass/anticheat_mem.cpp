#include <Windows.h>
#include <vector>
#include <cstdint>
#include <thread>
#include <chrono>
#include <TlHelp32.h>
#include <iostream>
#include <random>
#include <array>
#include <memory>

//this is the memory bypass for ricochet anticheat
//written by @mystorik, @de-virtualized and @sodareverse

class RicochetAntiCheatBypass {
private:
    HANDLE processHandle;
    DWORD processId;
    std::vector<MEMORY_BASIC_INFORMATION> memRegions;
    std::mt19937 rng;
    
    struct MemorySignature {
        std::vector<uint8_t> pattern;
        std::vector<uint8_t> mask;
        std::string name;
        bool critical;
    };

    struct PatchLocation {
        LPVOID address;
        std::vector<uint8_t> originalBytes;
        std::vector<uint8_t> patchedBytes;
        bool verified;
    };

    std::vector<PatchLocation> patchedLocations;
    
    bool VerifyMemoryAccess(LPVOID address, SIZE_T size) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(processHandle, address, &mbi, sizeof(mbi))) {
            return (mbi.Protect & PAGE_EXECUTE_READWRITE) || 
                   (mbi.Protect & PAGE_READWRITE) ||
                   (mbi.Protect & PAGE_EXECUTE_READ);
        }
        return false;
    }

    std::vector<uint8_t> GenerateRandomBytes(size_t length) {
        std::vector<uint8_t> bytes(length);
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& byte : bytes) {
            byte = static_cast<uint8_t>(dist(rng));
        }
        return bytes;
    }

    void ObfuscateMemoryWrites(LPVOID address, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> encrypted;
        encrypted.reserve(data.size());
        
        std::array<uint8_t, 4> keys = {
            static_cast<uint8_t>(rng() % 255 + 1),
            static_cast<uint8_t>(rng() % 255 + 1),
            static_cast<uint8_t>(rng() % 255 + 1),
            static_cast<uint8_t>(rng() % 255 + 1)
        };

        for (size_t i = 0; i < data.size(); i++) {
            encrypted.push_back(data[i] ^ keys[i % 4]);
        }

        SIZE_T bytesWritten;
        WriteProcessMemory(processHandle, address, encrypted.data(), encrypted.size(), &bytesWritten);
        
        std::uniform_int_distribution<int> delay_dist(10, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(rng)));
        
        for (size_t i = 0; i < encrypted.size(); i++) {
            uint8_t decrypted = encrypted[i] ^ keys[i % 4];
            WriteProcessMemory(processHandle, (LPVOID)((uintptr_t)address + i), &decrypted, 1, &bytesWritten);
            
            if (i % 4 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(delay_dist(rng)));
            }
        }
    }

    void ScanAndPatchMemoryRegions() {
        MEMORY_BASIC_INFORMATION mbi;
        LPVOID address = nullptr;
        size_t totalMemory = 0;

        while (VirtualQueryEx(processHandle, address, &mbi, sizeof(mbi))) {
            if ((mbi.State == MEM_COMMIT) && 
                ((mbi.Protect & PAGE_EXECUTE_READ) || 
                 (mbi.Protect & PAGE_EXECUTE_READWRITE) ||
                 (mbi.Protect & PAGE_READWRITE))) {
                
                memRegions.push_back(mbi);
                totalMemory += mbi.RegionSize;
                
                if (totalMemory > 1024 * 1024 * 512) { // limit to 512mb
                    break;
                }
            }
            address = (LPVOID)((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
        }
    }

    bool VerifyPatch(const PatchLocation& loc) {
        std::vector<uint8_t> buffer(loc.patchedBytes.size());
        SIZE_T bytesRead;
        
        if (!ReadProcessMemory(processHandle, loc.address, buffer.data(), buffer.size(), &bytesRead)) {
            return false;
        }

        return buffer == loc.patchedBytes;
    }

    void RepairFailedPatches() {
        for (auto& loc : patchedLocations) {
            if (!VerifyPatch(loc)) {
                ObfuscateMemoryWrites(loc.address, loc.patchedBytes);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    bool PatchAntiCheatModule() {
        const std::vector<MemorySignature> signatures = {
            {{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10}, 
             {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
             "msf", true},
            {{0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9},
             {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
             "int check", true},
            {{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10},
             {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
             "process scanner", true},
            {{0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x05},
             {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
             "hook detection", false},
            {{0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18},
             {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
             "mem protection", true}
        };

        size_t patchesApplied = 0;
        const size_t requiredPatches = 3;

        for (const auto& region : memRegions) {
            std::vector<uint8_t> buffer(region.RegionSize);
            SIZE_T bytesRead;
            
            if (ReadProcessMemory(processHandle, region.BaseAddress, buffer.data(), region.RegionSize, &bytesRead)) {
                for (const auto& sig : signatures) {
                    for (size_t i = 0; i < buffer.size() - sig.pattern.size(); i++) {
                        bool found = true;
                        for (size_t j = 0; j < sig.pattern.size(); j++) {
                            if ((buffer[i + j] & sig.mask[j]) != (sig.pattern[j] & sig.mask[j])) {
                                found = false;
                                break;
                            }
                        }
                        
                        if (found) {
                            LPVOID patchAddress = (LPVOID)((uintptr_t)region.BaseAddress + i);
                            std::vector<uint8_t> nopPatch(sig.pattern.size(), 0x90);
                            
                            PatchLocation loc;
                            loc.address = patchAddress;
                            loc.originalBytes = std::vector<uint8_t>(
                                buffer.begin() + i,
                                buffer.begin() + i + sig.pattern.size()
                            );
                            loc.patchedBytes = nopPatch;
                            loc.verified = false;
                            
                            ObfuscateMemoryWrites(patchAddress, nopPatch);
                            patchedLocations.push_back(loc);
                            
                            if (sig.critical) {
                                patchesApplied++;
                            }
                        }
                    }
                }
            }
        }

        return patchesApplied >= requiredPatches;
    }

    void InstallHooks() {
        DWORD oldProtect;
        //hook bytes
        //all hook bytes are 10 bytes
        // 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, addr
        // 0xFF, 0xE0                                                      // jmp rax
        // 0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,  // mov [rsp+0x10], rax
        // 0x48, 0x89, 0x74, 0x24, 0x20, 0x48, 0x83, 0xEC, 0x28, 0x48,  // mov [rsp+0x20], rsi
        // 0x48, 0x83, 0xC4, 0x28, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90,  // add rsp, 0x28
        // 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,  // nop (10)
        std::vector<uint8_t> hookBytes = {
            0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // mov rax, addr
            0xFF, 0xE0                                                      // jmp rax
        };

        for (const auto& region : memRegions) {
            if (VirtualProtectEx(processHandle, region.BaseAddress, region.RegionSize, 
                               PAGE_EXECUTE_READWRITE, &oldProtect)) {
                
                for (size_t offset = 0; offset < region.RegionSize - hookBytes.size(); offset += 0x1000) {
                    LPVOID targetAddr = (LPVOID)((uintptr_t)region.BaseAddress + offset);
                    if (VerifyMemoryAccess(targetAddr, hookBytes.size())) {
                        std::vector<uint8_t> customHook = hookBytes;
                        *(uint64_t*)&customHook[2] = (uint64_t)targetAddr + hookBytes.size();
                        ObfuscateMemoryWrites(targetAddr, customHook);
                    }
                }

                VirtualProtectEx(processHandle, region.BaseAddress, region.RegionSize, oldProtect, &oldProtect);
            }
        }
    }

public:
    RicochetAntiCheatBypass() : processHandle(NULL), processId(0), rng(std::random_device{}()) {}

    bool Initialize() {
        HWND windowHandle = FindWindowA("cc-bypass", NULL);
        if (!windowHandle) return false;

        GetWindowThreadProcessId(windowHandle, &processId);
        if (!processId) return false;

        processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (!processHandle) return false;

        ScanAndPatchMemoryRegions();
        return !memRegions.empty();
    }

    bool ExecuteBypass() {
        if (!PatchAntiCheatModule()) return false;
        
        DWORD oldProtect;
        for (const auto& region : memRegions) {
            VirtualProtectEx(processHandle, region.BaseAddress, region.RegionSize, 
                           PAGE_EXECUTE_READWRITE, &oldProtect);
        }

        InstallHooks();

        std::uniform_int_distribution<int> size_dist(16, 64);
        
        for (size_t i = 0; i < 15; i++) {
            std::vector<uint8_t> decoyData = GenerateRandomBytes(size_dist(rng));
            
            for (const auto& region : memRegions) {
                if (VerifyMemoryAccess(region.BaseAddress, decoyData.size())) {
                    ObfuscateMemoryWrites(region.BaseAddress, decoyData);
                }
            }
            
            RepairFailedPatches();
            std::this_thread::sleep_for(std::chrono::milliseconds(75 + (rng() % 50)));
        }

        return true;
    }

    ~RicochetAntiCheatBypass() {
        if (processHandle) {
            CloseHandle(processHandle);
        }
    }
};