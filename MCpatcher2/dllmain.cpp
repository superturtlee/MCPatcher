#include "pch.h"
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <cstdint>
#include <string>
#include <psapi.h> // Add this include at the top of your file (after windows.h) to define MODULEINFO
#include "pattern.h"
// Pattern matching structure
struct Pattern {
    std::vector<uint8_t> original;
    std::vector<uint8_t> patched;
    std::vector<int> mask; // -1 for wildcard (??)
};

// Global patterns based on your specifications
std::vector<Pattern> g_patterns;

// Helper: Convert hex string to bytes
std::vector<uint8_t> HexStringToBytes(const std::string& hex, std::vector<int>& mask) {
    std::vector<uint8_t> bytes;
    mask.clear();

    for (size_t i = 0; i < hex.length(); i++) {
        if (hex[i] == ' ') continue;

        if (hex[i] == '?' && i + 1 < hex.length() && hex[i + 1] == '?') {
            bytes.push_back(0x00);
            mask.push_back(-1); // Wildcard
            i++;
        }
        else {
            std::string byteStr = hex.substr(i, 2);
            uint8_t byte = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
            bytes.push_back(byte);
            mask.push_back(1); // Must match
            i++;
        }
    }
    return bytes;
}

// Pattern matching with wildcard support
bool PatternMatch(const uint8_t* data, const std::vector<uint8_t>& pattern, const std::vector<int>& mask) {
    for (size_t i = 0; i < pattern.size(); i++) {
        if (mask[i] == -1) continue; // Skip wildcard
        if (data[i] != pattern[i]) return false;
    }
    return true;
}

// Search for pattern in memory region
bool FindAndPatch(uint8_t* baseAddress, size_t regionSize, const Pattern& pattern) {
    bool patched = false;

    for (size_t offset = 0; offset <= regionSize - pattern.original.size(); offset++) {
        if (PatternMatch(baseAddress + offset, pattern.original, pattern.mask)) {
            DWORD oldProtect;

            // Change memory protection to allow writing
            if (VirtualProtect(baseAddress + offset, pattern.patched.size(),
                PAGE_EXECUTE_READWRITE, &oldProtect)) {

                // Apply patch
                memcpy(baseAddress + offset, pattern.patched.data(), pattern.patched.size());

                // Restore original protection
                VirtualProtect(baseAddress + offset, pattern.patched.size(),
                    oldProtect, &oldProtect);

                // Flush instruction cache
                FlushInstructionCache(GetCurrentProcess(), baseAddress + offset,
                    pattern.patched.size());

                patched = true;
            }
        }
    }

    return patched;
}

void InitializePatterns() {
    // Pattern 1
    Pattern pattern1;
    pattern1.mask.clear();



    pattern1.original = HexStringToBytes(original1, pattern1.mask);
    std::vector<int> dummyMask;
    pattern1.patched = HexStringToBytes(patched1, dummyMask);
    g_patterns.push_back(pattern1);

    // Pattern 2
    Pattern pattern2;
    pattern2.mask.clear();



    pattern2.original = HexStringToBytes(original2, pattern2.mask);
    pattern2.patched = HexStringToBytes(patched2, dummyMask);
    g_patterns.push_back(pattern2);
}

// Main patching routine
void PerformPatching() {
    DWORD currentProcessId = GetCurrentProcessId();
    DWORD currentThreadId = GetCurrentThreadId();

   
    InitializePatterns();
    // Get module information
    MODULEINFO moduleInfo;
    HMODULE hModule = GetModuleHandle(NULL);

    if (GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo))) {
        uint8_t* baseAddress = (uint8_t*)moduleInfo.lpBaseOfDll;
        size_t moduleSize = moduleInfo.SizeOfImage;

        // Parse PE headers to find .text section
        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)baseAddress;
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(baseAddress + dosHeader->e_lfanew);
        PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

        for (int i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
            // Check if this is a code section (.text or executable)
            if (sectionHeader[i].Characteristics & IMAGE_SCN_CNT_CODE ||
                sectionHeader[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {

                uint8_t* sectionBase = baseAddress + sectionHeader[i].VirtualAddress;
                size_t sectionSize = sectionHeader[i].Misc.VirtualSize;

                // Search and patch all patterns
                for (const auto& pattern : g_patterns) {
                    FindAndPatch(sectionBase, sectionSize, pattern);
                }
            }
        }
    }   
}

// DLL Entry Point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);
        PerformPatching();
        break;

    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
