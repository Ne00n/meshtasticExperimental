#pragma once

#include "mesh-pb-constants.h"
#include "NodeDB.h"
#include <unordered_map>
#include <deque>
#include <vector>
#include <algorithm>
#include <cstdint>

// Maximum number of packets to track for success rate calculation
constexpr uint8_t MAX_PACKET_HISTORY = 10;
// Number of top routes to keep track of
constexpr uint8_t MAX_TOP_ROUTES = 3;

// Structure to track route statistics
struct RouteStats {
    uint32_t totalPackets = 0;      // Total packets received through this route
    uint8_t minHops = 0;            // Minimum number of hops observed
    uint8_t maxHops = 0;            // Maximum number of hops observed
    float avgHops = 0.0f;           // Average number of hops
    std::deque<bool> recentResults; // Recent packet delivery results for success rate calculation
};

// Structure to represent a route with its score
struct ScoredRoute {
    uint8_t nextHop;
    float score;
    
    ScoredRoute(uint8_t nh, float s) : nextHop(nh), score(s) {}
    
    bool operator<(const ScoredRoute& other) const {
        return score > other.score; // Higher score comes first
    }
};

class NextHopOptimizer {
public:
    NextHopOptimizer(NodeDB *nodeDB);
    
    // Update route statistics when a packet is sent
    void updateRouteStats(NodeNum destination, uint8_t nextHop, uint8_t hopCount);
    
    // Update route statistics when a packet is successfully delivered
    void updateRouteSuccess(NodeNum destination, uint8_t nextHop, uint8_t hopCount);
    
    // Get the best next hop for a destination based on statistics
    uint8_t getBestNextHop(NodeNum destination, uint8_t currentNextHop);
    
    // Reset statistics for a specific route
    void resetRouteStats(NodeNum destination, uint8_t nextHop);

private:
    NodeDB *nodeDB;
    
    // Map to store route statistics: key is (destination << 8 | nextHop)
    std::unordered_map<uint32_t, RouteStats> routeStats;
    
    // Map to store top 3 routes for each destination
    std::unordered_map<NodeNum, std::vector<ScoredRoute>> topRoutes;
    
    // Calculate success rate for a route
    float calculateSuccessRate(const RouteStats &stats);
    
    // Calculate route score based on success rate and hop count
    float calculateRouteScore(const RouteStats &stats);
    
    // Get route key from destination and next hop
    uint32_t getRouteKey(NodeNum destination, uint8_t nextHop);
    
    // Update top routes for a destination
    void updateTopRoutes(NodeNum destination);
}; 