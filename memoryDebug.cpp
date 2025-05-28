#include "memoryDebug.h"
#include <Arduino.h>

// Initialize static variables
unsigned long MemoryDebug::lastCheckTime = 0;

void MemoryDebug::printMemoryStats() {
    printMemoryStats("Memory Stats");
}

void MemoryDebug::printMemoryStats(const char* label) {
    Serial.println("----------------------------------------");
    Serial.println(label);
    Serial.println("----------------------------------------");
    
    // Main SRAM (Heap)
    uint32_t freeHeap = memGet.getFreeHeap();
    uint32_t totalHeap = memGet.getHeapSize();
    uint32_t usedHeap = totalHeap - freeHeap;
    
    Serial.printf("Main SRAM (Heap):\n");
    Serial.printf("  Total: %u bytes (%.1f KB)\n", totalHeap, totalHeap/1024.0f);
    Serial.printf("  Used:  %u bytes (%.1f KB)\n", usedHeap, usedHeap/1024.0f);
    Serial.printf("  Free:  %u bytes (%.1f KB)\n", freeHeap, freeHeap/1024.0f);
    Serial.printf("  Usage: %.1f%%\n", (float)usedHeap / totalHeap * 100.0f);
    
    // RTC SRAM (if available)
    #ifdef ARCH_ESP32
    uint32_t rtcFree = ESP.getFreeRtcMemory();
    uint32_t rtcTotal = 16384;  // 16KB RTC SRAM
    uint32_t rtcUsed = rtcTotal - rtcFree;
    
    Serial.printf("\nRTC SRAM:\n");
    Serial.printf("  Total: %u bytes (%.1f KB)\n", rtcTotal, rtcTotal/1024.0f);
    Serial.printf("  Used:  %u bytes (%.1f KB)\n", rtcUsed, rtcUsed/1024.0f);
    Serial.printf("  Free:  %u bytes (%.1f KB)\n", rtcFree, rtcFree/1024.0f);
    Serial.printf("  Usage: %.1f%%\n", (float)rtcUsed / rtcTotal * 100.0f);
    #endif
    
    // PSRAM (if available)
    uint32_t freePsram = memGet.getFreePsram();
    uint32_t totalPsram = memGet.getPsramSize();
    
    if (totalPsram > 0) {
        uint32_t usedPsram = totalPsram - freePsram;
        Serial.printf("\nPSRAM:\n");
        Serial.printf("  Total: %u bytes (%.1f KB)\n", totalPsram, totalPsram/1024.0f);
        Serial.printf("  Used:  %u bytes (%.1f KB)\n", usedPsram, usedPsram/1024.0f);
        Serial.printf("  Free:  %u bytes (%.1f KB)\n", freePsram, freePsram/1024.0f);
        Serial.printf("  Usage: %.1f%%\n", (float)usedPsram / totalPsram * 100.0f);
    }
    
    Serial.println("----------------------------------------");
}

void MemoryDebug::checkPeriodicStats() {
    unsigned long currentTime = millis();
    
    // Check if 30 seconds have passed
    if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
        printMemoryStats("Periodic Memory Check");
        lastCheckTime = currentTime;
    }
} 