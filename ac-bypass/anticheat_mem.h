#pragma once
#include <Windows.h>
#include <vector>
#include <cstdint>

namespace ac_bypass {

class MemoryBypass {
private:
    std::vector<BYTE> original_bytes;
    LPVOID target_address;
    SIZE_T patch_size;
    
    bool WriteMemory(LPVOID address, BYTE* buffer, SIZE_T size) {
        DWORD old_protect;
        if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old_protect))
            return false;
            
        memcpy(address, buffer, size);
        
        VirtualProtect(address, size, old_protect, &old_protect);
        return true;
    }

public:
    MemoryBypass() : target_address(nullptr), patch_size(0) {}
    
    bool Initialize(LPVOID address, SIZE_T size) {
        target_address = address;
        patch_size = size;
        
        original_bytes.resize(size);
        memcpy(original_bytes.data(), address, size);
        
        return true;
    }
    
    bool PatchMemory(BYTE* new_bytes) {
        return WriteMemory(target_address, new_bytes, patch_size);
    }
    
    bool RestoreMemory() {
        if (original_bytes.empty())
            return false;
            
        return WriteMemory(target_address, original_bytes.data(), patch_size);
    }
    
    ~MemoryBypass() {
        RestoreMemory();
    }
};

}