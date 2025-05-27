#pragma once

#include <cstdint>
#include "MeshTypes.h"
#include <unordered_map>
#include <vector>
#include <algorithm>

/**
 * Structure to hold information about a next hop candidate
 */
struct NextHopCandidate {
    uint8_t nodeId;           // The node ID of the next hop
    uint32_t successCount;    // Number of successful transmissions through this hop
    uint32_t failureCount;    // Number of failed transmissions through this hop
    uint32_t lastUsedTime;    // Last time this hop was used (millis)
    float successRate;        // Calculated success rate

    NextHopCandidate(uint8_t id) : nodeId(id), successCount(0), failureCount(0), lastUsedTime(0), successRate(0.0f) {}

    // Update success rate calculation
    void updateSuccessRate() {
        uint32_t total = successCount + failureCount;
        successRate = total > 0 ? (float)successCount / total : 0.0f;
    }
};

/**
 * Class to manage and select the best next hops for packet routing
 * Maintains a list of the top 3 next hops for each destination
 */
class NextHopSelector {
private:
    // Map of destination node to its list of next hop candidates
    std::unordered_map<NodeNum, std::vector<NextHopCandidate>> nextHopMap;
    
    // Maximum number of next hops to maintain per destination
    static constexpr uint8_t MAX_NEXT_HOPS = 3;
    
    // Minimum number of attempts before considering success rate
    static constexpr uint32_t MIN_ATTEMPTS = 3;

public:
    NextHopSelector() {}

    /**
     * Get the best next hop for a destination
     * @param dest The destination node
     * @param currentRelay The current relay node (to avoid routing back)
     * @return The best next hop node ID, or NO_NEXT_HOP_PREFERENCE if no good route exists
     */
    uint8_t getBestNextHop(NodeNum dest, uint8_t currentRelay);

    /**
     * Record a successful transmission through a next hop
     * @param dest The destination node
     * @param nextHop The next hop node that successfully relayed
     */
    void recordSuccess(NodeNum dest, uint8_t nextHop);

    /**
     * Record a failed transmission through a next hop
     * @param dest The destination node
     * @param nextHop The next hop node that failed to relay
     */
    void recordFailure(NodeNum dest, uint8_t nextHop);

    /**
     * Reset the next hop selection for a destination
     * @param dest The destination node to reset
     */
    void resetNextHops(NodeNum dest);

    /**
     * Get debug info about the top candidates for a destination
     * @param dest The destination node
     * @return Vector of the top candidates, sorted by success rate
     */
    std::vector<NextHopCandidate> getTopCandidates(NodeNum dest) const {
        auto it = nextHopMap.find(dest);
        if (it != nextHopMap.end()) {
            return it->second;
        }
        return {};
    }

private:
    /**
     * Add or update a next hop candidate
     * @param dest The destination node
     * @param nextHop The next hop node to add/update
     */
    void addOrUpdateNextHop(NodeNum dest, uint8_t nextHop);

    /**
     * Sort and trim the next hop list for a destination
     * @param dest The destination node
     */
    void sortAndTrimNextHops(NodeNum dest);
}; 