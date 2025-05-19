#pragma once

#include "configuration.h"
#include "mesh-pb-constants.h"
#include "MeshTypes.h"
#include <unordered_map>
#include <cstdint>

class RateLimiter {
private:
    // Map of sender node number to last packet timestamp for each rate-limited port
    std::unordered_map<NodeNum, std::unordered_map<meshtastic_PortNum, uint32_t>> lastPacketTimes;
    
    // Rate limit period in milliseconds (1 hour)
    static constexpr uint32_t RATE_LIMIT_PERIOD_MS = 3600000;
    
    // List of port numbers that should be rate limited
    static constexpr meshtastic_PortNum RATE_LIMITED_PORTS[] = {
        meshtastic_PortNum_NODEINFO_APP,
        meshtastic_PortNum_POSITION_APP
    };
    
    // Number of rate limited ports
    static constexpr size_t NUM_RATE_LIMITED_PORTS = sizeof(RATE_LIMITED_PORTS) / sizeof(RATE_LIMITED_PORTS[0]);

public:
    /**
     * Check if a packet should be rate limited
     * @param from The sender's node number
     * @param portnum The port number of the packet
     * @return true if the packet should be rate limited (dropped), false otherwise
     */
    bool shouldRateLimit(NodeNum from, meshtastic_PortNum portnum) {
        // Check if this port should be rate limited
        bool isRateLimitedPort = false;
        for (size_t i = 0; i < NUM_RATE_LIMITED_PORTS; i++) {
            if (RATE_LIMITED_PORTS[i] == portnum) {
                isRateLimitedPort = true;
                LOG_DEBUG("Port %d is a rate-limited port", portnum);
                break;
            }
        }
        
        if (!isRateLimitedPort) {
            LOG_DEBUG("Port %d is not rate-limited, allowing packet", portnum);
            return false; // Don't rate limit non-rate-limited ports
        }
        
        uint32_t currentTime = millis();
        auto& senderTimes = lastPacketTimes[from];
        
        // Check if we've seen a packet from this sender on this port recently
        auto it = senderTimes.find(portnum);
        if (it != senderTimes.end()) {
            uint32_t timeSinceLastPacket = currentTime - it->second;
            if (timeSinceLastPacket < RATE_LIMIT_PERIOD_MS) {
                uint32_t remainingTime = RATE_LIMIT_PERIOD_MS - timeSinceLastPacket;
                LOG_DEBUG("Rate limiting packet from node 0x%x on port %d. Last packet was %u ms ago, %u ms remaining in rate limit period", 
                         from, portnum, timeSinceLastPacket, remainingTime);
                return true; // Rate limit this packet
            } else {
                LOG_DEBUG("Rate limit period expired for node 0x%x on port %d. Last packet was %u ms ago", 
                         from, portnum, timeSinceLastPacket);
            }
        } else {
            LOG_DEBUG("First packet from node 0x%x on port %d, not rate limited", from, portnum);
        }
        
        // Update the last packet time for this sender and port
        senderTimes[portnum] = currentTime;
        LOG_DEBUG("Updated last packet time for node 0x%x on port %d to %u", from, portnum, currentTime);
        return false;
    }
    
    /**
     * Clear rate limit history for a specific node
     * @param from The node number to clear history for
     */
    void clearNodeHistory(NodeNum from) {
        if (lastPacketTimes.erase(from) > 0) {
            LOG_DEBUG("Cleared rate limit history for node 0x%x", from);
        } else {
            LOG_DEBUG("No rate limit history found to clear for node 0x%x", from);
        }
    }
}; 