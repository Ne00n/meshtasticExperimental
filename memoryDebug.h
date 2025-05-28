#pragma once

#include "memGet.h"

class MemoryDebug {
public:
    static void printMemoryStats();
    static void printMemoryStats(const char* label);
    static void checkPeriodicStats();
private:
    static unsigned long lastCheckTime;
    static const unsigned long CHECK_INTERVAL = 30000;
}; 